# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import date
import logging
import os
import re
import shutil
import sys
import tempfile
from typing import Any, Dict, List, Optional
import unittest

from gpu_tests import color_profile_manager
from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import gpu_helper
from gpu_tests import gpu_integration_test
from gpu_tests import skia_gold_matching_algorithms as algo

from skia_gold_common import skia_gold_properties as sgp
from skia_gold_common import skia_gold_session as sgs
from skia_gold_common import skia_gold_session_manager as sgsm

import gpu_path_util

from py_utils import cloud_storage

from telemetry.util import image_util

TEST_DATA_DIRS = [
    gpu_path_util.GPU_DATA_DIR,
    os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'media', 'test', 'data'),
]

SKIA_GOLD_CORPUS = 'chrome-gpu'


class _ImageParameters():
  def __init__(self):
    # Parameters for cloud storage reference images.
    self.vendor_id: Optional[int] = None
    self.device_id: Optional[int] = None
    self.vendor_string: Optional[str] = None
    self.device_string: Optional[str] = None
    self.msaa: bool = False
    self.model_name: Optional[str] = None
    self.driver_version: Optional[str] = None
    self.driver_vendor: Optional[str] = None
    self.display_server: Optional[str] = None
    self.skia_graphite_status: Optional[str] = None


# This and its subclasses could potentially be switched to using dataclasses,
# but due to Python's method resolution order, inheritance breaks if a parent
# class has fields with defaults and a child class has fields without defaults.
# This might be possible to resolve with kw_only, but that requires at least
# Python 3.10.
# The third party attrs module supposedly handles this situation
# (https://stackoverflow.com/a/53085935), but attempting to do so on version
# 21.4.0 fails.
class SkiaGoldTestCase():
  """Base class for any Gold-enabled test case definition.

  Only information used within SkiaGoldIntegrationTestBase should be stored
  here. Additional information should be stored in the appropriate subclass.
  """
  # pylint: disable=too-many-arguments
  def __init__(
      self,
      name: str,
      gpu_process_disabled: bool = False,
      grace_period_end: Optional[date] = None,
      matching_algorithm: Optional[algo.SkiaGoldMatchingAlgorithm] = None,
      refresh_after_finish: bool = False):
    """
    Args:
      name: A string containing the name of the test.
      gpu_process_disabled: Whether the test runs with the GPU process disabled.
      grace_period_end: An optional datetime.date, before which Gold comparison
          failures will be ignored. This allows a newly added test to be
          exempted for a (hopefully) short period after being added. This is so
          that any slightly different different but valid images that get
          produced by the CI builders can be triaged without turning the
          builders red.
      matching_algorithm: A
          skia_gold_matching_algoriths.SkiaGoldMatchingAlgorithm that specifies
          which matching algorithm Skia Gold should use for the test. Defaults
          to exact matching.
      refresh_after_finish: Whether to refresh the entire page after the test
          finishes.
    """
    self.name = name
    self.gpu_process_disabled = gpu_process_disabled
    self.grace_period_end = grace_period_end
    self.matching_algorithm = (matching_algorithm
                               or algo.ExactMatchingAlgorithm())
    self.refresh_after_finish = refresh_after_finish

  #pylint: enable=too-many-arguments


class SkiaGoldIntegrationTestBase(gpu_integration_test.GpuIntegrationTest):
  """Base class for all tests that upload results to Skia Gold."""
  _error_image_cloud_storage_bucket = 'chromium-browser-gpu-tests'

  # This information is class-scoped, so that it can be shared across
  # invocations of tests; but it's zapped every time the browser is
  # restarted with different command line arguments.
  _image_parameters: Optional[_ImageParameters] = None

  _skia_gold_temp_dir: Optional[str] = None
  _skia_gold_session_manager: Optional[sgsm.SkiaGoldSessionManager] = None
  _skia_gold_properties: Optional[sgp.SkiaGoldProperties] = None

  # Loaded from disk at a later point.
  _dom_automation_controller_script: Optional[str] = None

  @classmethod
  def SetUpProcess(cls) -> None:
    super(SkiaGoldIntegrationTestBase, cls).SetUpProcess()
    options = cls.GetOriginalFinderOptions()
    color_profile_manager.ForceUntilExitSRGB(
        options.dont_restore_color_profile_after_test)
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs(cls._GetStaticServerDirs())
    cls._skia_gold_temp_dir = tempfile.mkdtemp()

  @classmethod
  def _GetStaticServerDirs(cls) -> List[str]:
    return TEST_DATA_DIRS

  @classmethod
  def _SetClassVariablesFromOptions(cls, options: ct.ParsedCmdArgs) -> None:
    super()._SetClassVariablesFromOptions(options)
    if not cls._dom_automation_controller_script:
      with open(
          os.path.join(gpu_path_util.GPU_TEST_HARNESS_JAVASCRIPT_DIR,
                       'websocket_heartbeat.js')) as f:
        cls._dom_automation_controller_script = f.read()
      cls._dom_automation_controller_script += '\n'
      with open(
          os.path.join(gpu_path_util.GPU_TEST_HARNESS_JAVASCRIPT_DIR,
                       'dom_automation_controller.js')) as f:
        cls._dom_automation_controller_script += f.read()

  @classmethod
  def GetSkiaGoldProperties(cls) -> sgp.SkiaGoldProperties:
    if not cls._skia_gold_properties:
      cls._skia_gold_properties = sgp.SkiaGoldProperties(
          cls.GetOriginalFinderOptions())
    return cls._skia_gold_properties

  @classmethod
  def GetSkiaGoldSessionManager(cls) -> sgsm.SkiaGoldSessionManager:
    if not cls._skia_gold_session_manager:
      cls._skia_gold_session_manager = sgsm.SkiaGoldSessionManager(
          cls._skia_gold_temp_dir, cls.GetSkiaGoldProperties())
    return cls._skia_gold_session_manager

  @classmethod
  def GenerateBrowserArgs(cls, additional_args: List[str]) -> List[str]:
    """Adds default arguments to |additional_args|.

    See the parent class' method documentation for additional information.
    """
    default_args = super(SkiaGoldIntegrationTestBase,
                         cls).GenerateBrowserArgs(additional_args)
    default_args.extend([cba.ENABLE_GPU_BENCHMARKING, cba.TEST_TYPE_GPU])
    force_color_profile_arg = [
        arg for arg in default_args if arg.startswith('--force-color-profile=')
    ]
    if not force_color_profile_arg:
      default_args.extend([
          cba.FORCE_COLOR_PROFILE_SRGB,
          cba.ENSURE_FORCED_COLOR_PROFILE,
      ])
    return default_args

  @classmethod
  def StopBrowser(cls) -> None:
    super(SkiaGoldIntegrationTestBase, cls).StopBrowser()
    cls.ResetGpuInfo()

  @classmethod
  def TearDownProcess(cls) -> None:
    super(SkiaGoldIntegrationTestBase, cls).TearDownProcess()
    shutil.rmtree(cls._skia_gold_temp_dir)
    cls._skia_gold_session_manager = None

  @classmethod
  def AddCommandlineArgs(cls, parser: ct.CmdArgParser) -> None:
    super(SkiaGoldIntegrationTestBase, cls).AddCommandlineArgs(parser)
    parser.add_argument('--git-revision', help='Chrome revision being tested.')
    parser.add_argument(
        '--test-machine-name',
        default='',
        help=('Name of the test machine. Specifying this argument causes this '
              'script to upload failure images and diffs to cloud storage '
              'directly, instead of relying on the '
              'archive_gpu_pixel_test_results.py script.'))
    parser.add_argument(
        '--dont-restore-color-profile-after-test',
        action='store_true',
        default=False,
        help=("(Mainly on Mac) don't restore the system's original color "
              'profile after the test completes; leave the system using the '
              'sRGB color profile. See http://crbug.com/784456.'))
    parser.add_argument('--gerrit-issue',
                        default='',
                        help='For Skia Gold integration. Gerrit issue ID.')
    parser.add_argument(
        '--gerrit-patchset',
        default='',
        help='For Skia Gold integration. Gerrit patch set number.')
    parser.add_argument('--buildbucket-id',
                        default='',
                        help='For Skia Gold integration. Buildbucket build ID.')
    parser.add_argument(
        '--no-skia-gold-failure',
        action='store_true',
        default=False,
        help=('For Skia Gold integration. Always report that the test passed '
              'even if the Skia Gold image comparison reported a failure, but '
              'otherwise perform the same steps as usual.'))
    # Telemetry is *still* using optparse instead of argparse, so we can't have
    # these two options in a mutually exclusive group.
    # TODO(crbug.com/40807291): Use a mutually exclusive group once the
    # optparse -> argparse migration is complete.
    parser.add_argument(
        '--local-pixel-tests',
        action='store_true',
        default=None,
        help=('Specifies to run the test harness in local run mode or not. '
              'When run in local mode, uploading to Gold is disabled and links '
              'to help with local debugging are output. Running in local mode '
              'also implies --no-luci-auth. If both this and '
              '--no-local-pixel-tests are left unset, the test harness will '
              'attempt to detect whether it is running on a workstation or not '
              'and set this option accordingly.'))
    parser.add_argument(
        '--no-local-pixel-tests',
        action='store_false',
        dest='local_pixel_tests',
        help=('Specifies to run the test harness in non-local (bot) mode. When '
              'run in this mode, data is actually uploaded to Gold and triage '
              'links are generated. If both this and --local-pixel-tests are '
              'left unset, the test harness will attempt to detect whether it '
              'is running on a workstation or not and set this option '
              'accordingly.'))
    parser.add_argument(
        '--skia-gold-local-png-write-directory',
        help=('Specifies a directory to save local image diffs to instead of '
              'the default of a temporary directory. Only has an effect when '
              'running tests locally, not on a bot.'))
    parser.add_argument(
        '--no-luci-auth',
        action='store_true',
        default=False,
        help=("Don't use the service account provided by LUCI for "
              'authentication for Skia Gold, instead relying on gsutil to be '
              'pre-authenticated. Meant for testing locally instead of on the '
              'bots.'))
    parser.add_argument(
        '--service-account',
        help=('Specifies the service account to use instead of using '
              'LUCI_CONTEXT or whatever is configured in gsutil. Implies '
              '--no-luci-auth. Only meant for use in Skylab where the tests '
              'are automated but do not have LUCI_CONTEXT available.'))
    parser.add_argument(
        '--bypass-skia-gold-functionality',
        action='store_true',
        default=False,
        help=('Bypass all interaction with Skia Gold, effectively disabling '
              'the image comparison portion of any tests that use Gold. Only '
              'meant to be used in case a Gold outage occurs and cannot be '
              'fixed quickly.'))

  @classmethod
  def ResetGpuInfo(cls) -> None:
    cls._image_parameters = None

  @classmethod
  def GetImageParameters(cls, test_case: SkiaGoldTestCase) -> _ImageParameters:
    if not cls._image_parameters:
      cls._ComputeGpuInfo(test_case)
    return cls._image_parameters

  @classmethod
  def _ComputeGpuInfo(cls, test_case: SkiaGoldTestCase) -> None:
    if cls._image_parameters:
      return
    browser = cls.browser
    system_info = browser.GetSystemInfo()
    if not system_info:
      raise Exception('System info must be supported by the browser')
    if not system_info.gpu:
      raise Exception('GPU information was absent')
    device = system_info.gpu.devices[0]
    cls._image_parameters = _ImageParameters()
    params = cls._image_parameters
    if device.vendor_id and device.device_id:
      params.vendor_id = device.vendor_id
      params.device_id = device.device_id
    elif device.vendor_string and device.device_string:
      params.vendor_string = device.vendor_string
      params.device_string = device.device_string
    elif test_case.gpu_process_disabled:
      # Match the vendor and device IDs that the browser advertises
      # when the software renderer is active.
      params.vendor_id = 65535
      params.device_id = 65535
    else:
      raise Exception('GPU device information was incomplete')
    # TODO(senorblanco): This should probably be checking
    # for the presence of the extensions in system_info.gpu_aux_attributes
    # in order to check for MSAA, rather than sniffing the blocklist.
    params.msaa = not (('disable_chromium_framebuffer_multisample' in
                        system_info.gpu.driver_bug_workarounds) or
                       ('disable_multisample_render_to_texture' in system_info.
                        gpu.driver_bug_workarounds))
    params.model_name = system_info.model_name
    params.driver_version = device.driver_version
    params.driver_vendor = device.driver_vendor
    params.display_server = gpu_helper.GetDisplayServer(browser.browser_type)
    params.skia_graphite_status = gpu_helper.GetSkiaGraphiteStatus(
        system_info.gpu)

  @classmethod
  def _UploadBitmapToCloudStorage(cls,
                                  bucket: str,
                                  name: str,
                                  bitmap: Any,
                                  public: bool = False) -> None:
    # This sequence of steps works on all platforms to write a temporary
    # PNG to disk, following the pattern in bitmap_unittest.py. The key to
    # avoiding PermissionErrors seems to be to not actually try to write to
    # the temporary file object, but to re-open its name for all operations.
    temp_file = tempfile.NamedTemporaryFile(suffix='.png').name
    image_util.WritePngFile(bitmap, temp_file)
    cloud_storage.Insert(bucket, name, temp_file, publicly_readable=public)

  # Not used consistently, but potentially useful for debugging issues on the
  # bots, so kept around for future use.
  @classmethod
  def _UploadGoldErrorImageToCloudStorage(cls, image_name: str,
                                          screenshot: ct.Screenshot) -> None:
    revision = cls.GetSkiaGoldProperties().git_revision
    machine_name = re.sub(r'\W+', '_',
                          cls.GetOriginalFinderOptions().test_machine_name)
    base_bucket = '%s/gold_failures' % (cls._error_image_cloud_storage_bucket)
    image_name_with_revision_and_machine = '%s_%s_%s.png' % (
        image_name, machine_name, revision)
    cls._UploadBitmapToCloudStorage(
        base_bucket,
        image_name_with_revision_and_machine,
        screenshot,
        public=True)

  @staticmethod
  def _UrlToImageName(url: str) -> str:
    image_name = re.sub(r'^(http|https|file)://(/*)', '', url)
    image_name = re.sub(r'\.\./', '', image_name)
    image_name = re.sub(r'(\.|/|-)', '_', image_name)
    return image_name

  def GetGoldJsonKeys(self, test_case: SkiaGoldTestCase) -> Dict[str, str]:
    """Get all the JSON metadata that will be passed to goldctl."""
    img_params = self.GetImageParameters(test_case)
    # The frequently changing last part of the ANGLE driver version (revision of
    # some sort?) messes a bit with inexact matching since each revision will
    # be treated as a separate trace, so strip it off.
    _StripAngleRevisionFromDriver(img_params)
    # When running under the validating decoder, the device string is reported
    # as the actual device, while under the passthrough decoder it is an ANGLE
    # string that contains the device and some additional information. This
    # difference can be annoying when trying to set up forwarding rules for the
    # public Gold instance, so only use the actual device.
    _ConvertAngleDeviceStringToActualDevice(img_params)
    # All values need to be strings, otherwise goldctl fails.
    gpu_keys = {
        'vendor_id':
        _ToHexOrNone(img_params.vendor_id),
        'device_id':
        _ToHexOrNone(img_params.device_id),
        'vendor_string':
        _ToNonEmptyStrOrNone(img_params.vendor_string),
        'device_string':
        _ToNonEmptyStrOrNone(img_params.device_string),
        'msaa':
        str(img_params.msaa),
        'model_name':
        _ToNonEmptyStrOrNone(img_params.model_name),
        'os':
        _ToNonEmptyStrOrNone(self.browser.platform.GetOSName()),
        'os_version':
        _ToNonEmptyStrOrNone(self.browser.platform.GetOSVersionName()),
        'os_version_detail_string':
        _ToNonEmptyStrOrNone(self.browser.platform.GetOSVersionDetailString()),
        'driver_version':
        _ToNonEmptyStrOrNone(img_params.driver_version),
        'driver_vendor':
        _ToNonEmptyStrOrNone(img_params.driver_vendor),
        'display_server':
        _ToNonEmptyStrOrNone(img_params.display_server),
        'combined_hardware_identifier':
        _GetCombinedHardwareIdentifier(img_params),
        'browser_type':
        _ToNonEmptyStrOrNone(self.browser.browser_type),
        'skia_graphite_status':
        _ToNonEmptyStrOrNone(img_params.skia_graphite_status),
    }
    # If we have a grace period active, then the test is potentially flaky.
    # Include a pair that will cause Gold to ignore any untriaged images, which
    # will prevent it from automatically commenting on unrelated CLs that happen
    # to produce a new image.
    if _GracePeriodActive(test_case):
      # This is put in the regular keys dict instead of the optional one because
      # ignore rules do not apply to optional keys.
      gpu_keys['ignore'] = '1'
    return gpu_keys

  # pylint: disable=no-self-use
  def GetGoldOptionalKeys(self) -> Dict[str, str]:
    """Get all the optional JSON metadata that will be passed to goldctl.

    This optional data is unrelated to the configurations that images are
    produced on, e.g. a comment that will be surfaced in Gold's UI.
    """
    return {}

  # pylint: enable=no-self-use

  def _UploadTestResultToSkiaGold(self, image_name: str,
                                  screenshot: ct.Screenshot,
                                  test_case: SkiaGoldTestCase) -> None:
    """Compares the given image using Skia Gold and uploads the result.

    No uploading is done if the test is being run in local run mode. Compares
    the given screenshot to baselines provided by Gold, raising an Exception if
    a match is not found.

    Args:
      image_name: the name of the image being checked.
      screenshot: the image being checked as a Telemetry Bitmap.
      test_case: the GPU SkiaGoldTestCase object for the test.
    """
    # Write screenshot to PNG file on local disk.
    png_temp_file = tempfile.NamedTemporaryFile(
        suffix='.png', dir=self._skia_gold_temp_dir).name
    image_util.WritePngFile(screenshot, png_temp_file)

    gpu_keys = self.GetGoldJsonKeys(test_case)
    gold_session = self.GetSkiaGoldSessionManager().GetSkiaGoldSession(
        gpu_keys, corpus=SKIA_GOLD_CORPUS)
    gold_properties = self.GetSkiaGoldProperties()
    use_luci = not (gold_properties.local_pixel_tests
                    or gold_properties.no_luci_auth)
    optional_keys = self.GetGoldOptionalKeys()

    status, error = gold_session.RunComparison(
        name=image_name,
        png_file=png_temp_file,
        inexact_matching_args=test_case.matching_algorithm.GetCmdline(),
        use_luci=use_luci,
        service_account=gold_properties.service_account,
        optional_keys=optional_keys)
    if not status:
      return

    status_codes =\
        self.GetSkiaGoldSessionManager().GetSessionClass().StatusCodes
    if status == status_codes.AUTH_FAILURE:
      logging.error('Gold authentication failed with output %s', error)
    elif status == status_codes.INIT_FAILURE:
      logging.error('Gold initialization failed with output %s', error)
    elif status == status_codes.COMPARISON_FAILURE_REMOTE:
      public_triage_link, internal_triage_link = gold_session.GetTriageLinks(
          image_name)
      if not public_triage_link or not internal_triage_link:
        logging.error('Failed to get triage links for %s, raw output: %s',
                      image_name, error)
        logging.error('Reason for no triage links: %s',
                      gold_session.GetTriageLinkOmissionReason(image_name))
      elif gold_properties.IsTryjobRun():
        self.artifacts.CreateLink('public_triage_link_for_entire_cl',
                                  public_triage_link)
        self.artifacts.CreateLink('internal_triage_link_for_entire_cl',
                                  internal_triage_link)
      else:
        self.artifacts.CreateLink('public_gold_triage_link', public_triage_link)
        self.artifacts.CreateLink('internal_gold_triage_link',
                                  internal_triage_link)
    elif status == status_codes.COMPARISON_FAILURE_LOCAL:
      logging.error('Local comparison failed. Local diff files:')
      _OutputLocalDiffFiles(gold_session, image_name)
    elif status == status_codes.LOCAL_DIFF_FAILURE:
      logging.error(
          'Local comparison failed and an error occurred during diff '
          'generation: %s', error)
      # There might be some files, so try outputting them.
      logging.error('Local diff files:')
      _OutputLocalDiffFiles(gold_session, image_name)
    else:
      logging.error(
          'Given unhandled SkiaGoldSession StatusCode %s with error %s', status,
          error)
    if self._ShouldReportGoldFailure(test_case):
      raise Exception(
          'goldctl command returned non-zero exit code, see above for details. '
          'This probably just means that the test produced an image that has '
          'not been triaged as positive.')

  def _ShouldReportGoldFailure(self, test_case: SkiaGoldTestCase) -> bool:
    """Determines if a Gold failure should actually be surfaced.

    Args:
      test_case: The GPU SkiaGoldTestCase object for the test.

    Returns:
      True if the failure should be surfaced, i.e. the test should fail,
      otherwise False.
    """
    parsed_options = self.GetOriginalFinderOptions()
    # Don't surface if we're explicitly told not to.
    if parsed_options.no_skia_gold_failure:
      return False
    # Don't surface if the test was recently added and we're still within its
    # grace period.
    if _GracePeriodActive(test_case):
      return False
    return True

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    del options
    raise NotImplementedError(
        'GenerateGpuTests must be overridden in a subclass')

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    raise NotImplementedError(
        'RunActualGpuTest must be overridden in a subclass')


def _ToHex(num: str) -> str:
  return hex(int(num))


def _ToHexOrNone(num: Optional[str]) -> str:
  return 'None' if num is None else _ToHex(num)


def _ToNonEmptyStrOrNone(val: Optional[str]) -> str:
  return 'None' if val == '' else str(val)


def _GracePeriodActive(test_case: SkiaGoldTestCase) -> bool:
  """Returns whether a grace period is currently active for a test.

  Args:
    test_case: The GPU SkiaGoldTestCase object for the test in question.

  Returns:
    True if a grace period is defined for |test_case| and has not yet expired.
    Otherwise, False.
  """
  return (test_case.grace_period_end
          and date.today() <= test_case.grace_period_end)


def _StripAngleRevisionFromDriver(img_params: _ImageParameters) -> None:
  """Strips the revision off the end of an ANGLE driver version.

  E.g. 2.1.0.b50541b2d6c4 -> 2.1.0

  Modifies the string in place. No-ops if the driver vendor is not ANGLE.

  Args:
    img_params: An _ImageParameters instance to modify.
  """
  if 'ANGLE' not in img_params.driver_vendor or not img_params.driver_version:
    return
  # Assume that we're never going to have portions of the driver we care about
  # that are longer than 8 characters.
  driver_parts = img_params.driver_version.split('.')
  kept_parts = []
  for part in driver_parts:
    if len(part) > 8:
      break
    kept_parts.append(part)
  img_params.driver_version = '.'.join(kept_parts)


def _ConvertAngleDeviceStringToActualDevice(
    img_params: _ImageParameters) -> None:
  """Converts an ANGLE device string to only have the actual device.

  E.g. ANGLE(Qualcomm, Adreno (TM) 640, OpenGL ES ...) -> Adreno (TM) 640

  Modifies the string in place. No-ops if the driver string is not ANGLE.

  Args:
    img_params: An _ImageParameters instance to modify.
  """
  device_id = gpu_helper.GetANGLEGpuDeviceId(img_params.device_string)
  if device_id:
    img_params.device_string = device_id


def _GetCombinedHardwareIdentifier(img_params: _ImageParameters) -> str:
  """Combine all relevant hardware identifiers into a single key.

  This makes Gold forwarding more precise by allowing us to forward explicit
  configurations instead of individual components.
  """
  vendor_id = _ToHexOrNone(img_params.vendor_id)
  device_id = _ToHexOrNone(img_params.device_id)
  device_string = _ToNonEmptyStrOrNone(img_params.device_string)
  combined_hw_identifiers = ('vendor_id:{vendor_id}, '
                             'device_id:{device_id}, '
                             'device_string:{device_string}')
  combined_hw_identifiers = combined_hw_identifiers.format(
      vendor_id=vendor_id, device_id=device_id, device_string=device_string)
  return combined_hw_identifiers


def _OutputLocalDiffFiles(gold_session: sgs.SkiaGoldSession,
                          image_name: str) -> None:
  """Logs the local diff image files from the given SkiaGoldSession.

  Args:
    gold_session: A skia_gold_session.SkiaGoldSession instance to pull files
        from.
    image_name: A string containing the name of the image/test that was
        compared.
  """
  given_file = gold_session.GetGivenImageLink(image_name)
  closest_file = gold_session.GetClosestImageLink(image_name)
  diff_file = gold_session.GetDiffImageLink(image_name)
  failure_message = 'Unable to retrieve link'
  logging.error('Generated image: %s', given_file or failure_message)
  logging.error('Closest image: %s', closest_file or failure_message)
  logging.error('Diff image: %s', diff_file or failure_message)


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
from datetime import date
import json
import logging
import os
import re
import subprocess
from subprocess import CalledProcessError
import shutil
import sys
import tempfile

from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import color_profile_manager

from py_utils import cloud_storage

from telemetry.util import image_util
from telemetry.util import rgba_color

GPU_RELATIVE_PATH = "content/test/data/gpu/"
GPU_DATA_DIR = os.path.join(path_util.GetChromiumSrcDir(), GPU_RELATIVE_PATH)
TEST_DATA_DIRS = [GPU_DATA_DIR,
                  os.path.join(
                      path_util.GetChromiumSrcDir(), 'media/test/data')]

goldctl_bin = os.path.join(
    path_util.GetChromiumSrcDir(), 'tools', 'skia_goldctl')
if sys.platform == 'win32':
  goldctl_bin = os.path.join(goldctl_bin, 'win', 'goldctl') + '.exe'
elif sys.platform == 'darwin':
  goldctl_bin = os.path.join(goldctl_bin, 'mac', 'goldctl')
else:
  goldctl_bin = os.path.join(goldctl_bin, 'linux', 'goldctl')

SKIA_GOLD_INSTANCE = 'chrome-gpu'


@contextlib.contextmanager
def RunInChromiumSrc():
  old_cwd = os.getcwd()
  os.chdir(path_util.GetChromiumSrcDir())
  try:
    yield
  finally:
    os.chdir(old_cwd)


# This is mainly used to determine if we need to run a subprocess through the
# shell - on Windows, finding executables via PATH doesn't work properly unless
# run through the shell.
def IsWin():
  return sys.platform == 'win32'


class _ImageParameters(object):
  def __init__(self):
    # Parameters for cloud storage reference images.
    self.vendor_id = None
    self.device_id = None
    self.vendor_string = None
    self.device_string = None
    self.msaa = False
    self.model_name = None


class SkiaGoldIntegrationTestBase(gpu_integration_test.GpuIntegrationTest):
  """Base class for all tests that upload results to Skia Gold."""
  # The command line options (which are passed to subclasses'
  # GenerateGpuTests) *must* be configured here, via a call to
  # SetParsedCommandLineOptions. If they are not, an error will be
  # raised when running the tests.
  _parsed_command_line_options = None

  _error_image_cloud_storage_bucket = 'chromium-browser-gpu-tests'

  # This information is class-scoped, so that it can be shared across
  # invocations of tests; but it's zapped every time the browser is
  # restarted with different command line arguments.
  _image_parameters = None

  _skia_gold_temp_dir = None

  _local_run = None
  _build_revision = None

  @classmethod
  def SetParsedCommandLineOptions(cls, options):
    cls._parsed_command_line_options = options

  @classmethod
  def GetParsedCommandLineOptions(cls):
    return cls._parsed_command_line_options

  @classmethod
  def SetUpProcess(cls):
    options = cls.GetParsedCommandLineOptions()
    color_profile_manager.ForceUntilExitSRGB(
      options.dont_restore_color_profile_after_test)
    super(SkiaGoldIntegrationTestBase, cls).SetUpProcess()
    cls.CustomizeBrowserArgs(cls._AddDefaultArgs([]))
    cls.StartBrowser()
    cls.SetStaticServerDirs(TEST_DATA_DIRS)
    cls._skia_gold_temp_dir = tempfile.mkdtemp()

  @staticmethod
  def _AddDefaultArgs(browser_args):
    if not browser_args:
      browser_args = []
    # All tests receive the following options.
    return [
      '--force-color-profile=srgb',
      '--ensure-forced-color-profile',
      '--enable-gpu-benchmarking',
      '--test-type=gpu'] + browser_args

  @classmethod
  def StopBrowser(cls):
    super(SkiaGoldIntegrationTestBase, cls).StopBrowser()
    cls.ResetGpuInfo()

  @classmethod
  def TearDownProcess(cls):
    super(SkiaGoldIntegrationTestBase, cls).TearDownProcess()
    if not cls.GetParsedCommandLineOptions().local_run:
      shutil.rmtree(cls._skia_gold_temp_dir)

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(SkiaGoldIntegrationTestBase, cls).AddCommandlineArgs(parser)
    parser.add_option(
      '--build-revision',
      help='Chrome revision being tested.',
      default=None)
    parser.add_option(
      '--test-machine-name',
      help='Name of the test machine. Specifying this argument causes this '
      'script to upload failure images and diffs to cloud storage directly, '
      'instead of relying on the archive_gpu_pixel_test_results.py script.',
      default='')
    parser.add_option(
      '--dont-restore-color-profile-after-test',
      dest='dont_restore_color_profile_after_test',
      action='store_true', default=False,
      help='(Mainly on Mac) don\'t restore the system\'s original color '
      'profile after the test completes; leave the system using the sRGB color '
      'profile. See http://crbug.com/784456.')
    parser.add_option(
      '--review-patch-issue',
      help='For Skia Gold integration. Gerrit issue ID.',
      default='')
    parser.add_option(
      '--review-patch-set',
      help='For Skia Gold integration. Gerrit patch set number.',
      default='')
    parser.add_option(
      '--buildbucket-build-id',
      help='For Skia Gold integration. Buildbucket build ID.',
      default='')
    parser.add_option(
      '--no-skia-gold-failure',
      action='store_true', default=False,
      help='For Skia Gold integration. Always report that the test passed even '
           'if the Skia Gold image comparison reported a failure, but '
           'otherwise perform the same steps as usual.')
    parser.add_option(
      '--local-run', default=None, type=int,
      help='Specifies to run the test harness in local run mode or not. When '
           'run in local mode, uploading to Gold is disabled and links to '
           'help with local debugging are output. Running in local mode also '
           'implies --no-luci-auth. If left unset, the test harness will '
           'attempt to detect whether it is running on a workstation or not '
           'and set this option accordingly.')
    parser.add_option(
      '--no-luci-auth',
      action='store_true', default=False,
      help='Don\'t use the service account provided by LUCI for authentication '
           'for Skia Gold, instead relying on gsutil to be pre-authenticated. '
           'Meant for testing locally instead of on the bots.')

  @classmethod
  def ResetGpuInfo(cls):
    cls._image_parameters = None

  @classmethod
  def GetImageParameters(cls, tab, page):
    if not cls._image_parameters:
      cls._ComputeGpuInfo(tab, page)
    return cls._image_parameters

  @classmethod
  def _ComputeGpuInfo(cls, tab, page):
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
    elif page.gpu_process_disabled:
      # Match the vendor and device IDs that the browser advertises
      # when the software renderer is active.
      params.vendor_id = 65535
      params.device_id = 65535
    else:
      raise Exception('GPU device information was incomplete')
    # TODO(senorblanco): This should probably be checking
    # for the presence of the extensions in system_info.gpu_aux_attributes
    # in order to check for MSAA, rather than sniffing the blacklist.
    params.msaa = not (
        ('disable_chromium_framebuffer_multisample' in
          system_info.gpu.driver_bug_workarounds) or
        ('disable_multisample_render_to_texture' in
          system_info.gpu.driver_bug_workarounds))
    params.model_name = system_info.model_name

  @classmethod
  def _UploadBitmapToCloudStorage(cls, bucket, name, bitmap, public=False):
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
  def _UploadGoldErrorImageToCloudStorage(cls, image_name, screenshot):
    machine_name = re.sub(r'\W+', '_',
                          cls.GetParsedCommandLineOptions().test_machine_name)
    base_bucket = '%s/gold_failures' % (cls._error_image_cloud_storage_bucket)
    image_name_with_revision_and_machine = '%s_%s_%s.png' % (
      image_name, machine_name, cls._GetBuildRevision())
    cls._UploadBitmapToCloudStorage(
      base_bucket, image_name_with_revision_and_machine, screenshot,
      public=True)

  def _CompareScreenshotSamples(self, tab, screenshot, expected_colors,
                                tolerance, device_pixel_ratio,
                                test_machine_name):
    # First scan through the expected_colors and see if there are any scale
    # factor overrides that would preempt the device pixel ratio. This
    # is mainly a workaround for complex tests like the Maps test.
    for expectation in expected_colors:
      if 'scale_factor_overrides' in expectation:
        for override in expectation['scale_factor_overrides']:
          # Require exact matches to avoid confusion, because some
          # machine models and names might be subsets of others
          # (e.g. Nexus 5 vs Nexus 5X).
          if ('device_type' in override and
              (tab.browser.platform.GetDeviceTypeName() ==
               override['device_type'])):
            logging.warning(
              'Overriding device_pixel_ratio ' + str(device_pixel_ratio) +
              ' with scale factor ' + str(override['scale_factor']) +
              ' for device type ' + override['device_type'])
            device_pixel_ratio = override['scale_factor']
            break
          if (test_machine_name and 'machine_name' in override and
              override["machine_name"] == test_machine_name):
            logging.warning(
              'Overriding device_pixel_ratio ' + str(device_pixel_ratio) +
              ' with scale factor ' + str(override['scale_factor']) +
              ' for machine name ' + test_machine_name)
            device_pixel_ratio = override['scale_factor']
            break
        # Only support one "scale_factor_overrides" in the expectation format.
        break
    for expectation in expected_colors:
      if "scale_factor_overrides" in expectation:
        continue
      location = expectation["location"]
      size = expectation["size"]
      x0 = int(location[0] * device_pixel_ratio)
      x1 = int((location[0] + size[0]) * device_pixel_ratio)
      y0 = int(location[1] * device_pixel_ratio)
      y1 = int((location[1] + size[1]) * device_pixel_ratio)
      for x in range(x0, x1):
        for y in range(y0, y1):
          if (x < 0 or y < 0 or x >= image_util.Width(screenshot) or
              y >= image_util.Height(screenshot)):
            self.fail(
                ('Expected pixel location [%d, %d] is out of range on ' +
                 '[%d, %d] image') %
                (x, y, image_util.Width(screenshot),
                 image_util.Height(screenshot)))

          actual_color = image_util.GetPixelColor(screenshot, x, y)
          expected_color = rgba_color.RgbaColor(
              expectation["color"][0],
              expectation["color"][1],
              expectation["color"][2],
              expectation["color"][3] if len(expectation["color"]) > 3 else 255)
          if not actual_color.IsEqual(expected_color, tolerance):
            self.fail('Expected pixel at ' + str(location) +
                ' (actual pixel (' + str(x) + ', ' + str(y) + ')) ' +
                ' to be ' +
                str(expectation["color"]) + " but got [" +
                str(actual_color.r) + ", " +
                str(actual_color.g) + ", " +
                str(actual_color.b) + ", " +
                str(actual_color.a) + "]")

  def ToHex(self, num):
    return hex(int(num))

  def ToHexOrNone(self, num):
    return 'None' if num == None else self.ToHex(num)

  def ToNonEmptyStrOrNone(self, val):
    return 'None' if val == '' else str(val)

  def _UrlToImageName(self, url):
    image_name = re.sub(r'^(http|https|file)://(/*)', '', url)
    image_name = re.sub(r'\.\./', '', image_name)
    image_name = re.sub(r'(\.|/|-)', '_', image_name)
    return image_name

  def _GetBuildIdArgs(self):
    # Get all the information that goldctl requires.
    parsed_options = self.GetParsedCommandLineOptions()
    build_id_args = [
      '--commit',
      self._GetBuildRevision(),
    ]
    # If --review-patch-issue is passed, then we assume we're running on a
    # trybot.
    if parsed_options.review_patch_issue:
      build_id_args += [
        '--issue',
        parsed_options.review_patch_issue,
        '--patchset',
        parsed_options.review_patch_set,
        '--jobid',
        parsed_options.buildbucket_build_id
      ]
    return build_id_args

  def GetGoldJsonKeys(self, tab, page):
    """Get all the JSON metadata that will be passed to golctl."""
    img_params = self.GetImageParameters(tab, page)
    # All values need to be strings, otherwise goldctl fails.
    gpu_keys = {
      'vendor_id': self.ToHexOrNone(img_params.vendor_id),
      'device_id': self.ToHexOrNone(img_params.device_id),
      'vendor_string': self.ToNonEmptyStrOrNone(img_params.vendor_string),
      'device_string': self.ToNonEmptyStrOrNone(img_params.device_string),
      'msaa': str(img_params.msaa),
      'model_name': self.ToNonEmptyStrOrNone(img_params.model_name),
    }
    return gpu_keys

  def _UploadTestResultToSkiaGold(self, image_name, screenshot,
                                  tab, page, build_id_args=None):
    """Compares the given image using Skia Gold and uploads the result.

    No uploading is done if the test is being run in local run mode. Compares
    the given screenshot to baselines provided by Gold, raising an Exception if
    a match is not found.

    Args:
      image_name: the name of the image being checked.
      screenshot: the image being checked as a Telemetry Bitmap.
      tab: the Telemetry Tab object that the test was run in.
      page: the GPU PixelTestPage object for the test.
      build_id_args: a list of build-identifying flags and values.
    """
    if not isinstance(build_id_args, list) or '--commit' not in build_id_args:
      raise Exception('Requires build args to be specified, including --commit')

    # Write screenshot to PNG file on local disk.
    png_temp_file = tempfile.NamedTemporaryFile(
        suffix='.png', dir=self._skia_gold_temp_dir).name
    image_util.WritePngFile(screenshot, png_temp_file)

    gpu_keys = self.GetGoldJsonKeys(tab, page)
    json_temp_file = tempfile.NamedTemporaryFile(
        suffix='.json', dir=self._skia_gold_temp_dir).name
    failure_file = tempfile.NamedTemporaryFile(
        suffix='.txt', dir=self._skia_gold_temp_dir).name
    with open(json_temp_file, 'w+') as f:
      json.dump(gpu_keys, f)

    # Figure out any extra args we need to pass to goldctl.
    extra_imgtest_args = []
    extra_auth_args = []
    parsed_options = self.GetParsedCommandLineOptions()
    if self._IsLocalRun():
      extra_imgtest_args.append('--dryrun')
    elif not parsed_options.no_luci_auth:
      extra_auth_args = ['--luci']

    # Run goldctl for a result.
    try:
      subprocess.check_output([goldctl_bin, 'auth',
                               '--work-dir', self._skia_gold_temp_dir]
                               + extra_auth_args,
            stderr=subprocess.STDOUT)
      cmd = ([goldctl_bin, 'imgtest', 'add', '--passfail',
              '--test-name', image_name,
              '--instance', SKIA_GOLD_INSTANCE,
              '--keys-file', json_temp_file,
              '--png-file', png_temp_file,
              '--work-dir', self._skia_gold_temp_dir,
              '--failure-file', failure_file] +
              build_id_args + extra_imgtest_args)
      subprocess.check_output(cmd, stderr=subprocess.STDOUT)
    except CalledProcessError as e:
      # The triage link for the image is output to the failure file, so report
      # that if it's available so it shows up in Milo. If for whatever reason
      # the file is not present or malformed, the triage link will still be
      # present in the stdout of the goldctl command.
      # If we're running on a trybot, instead generate a link to all results
      # for the CL so that the user can visit a single page instead of
      # clicking on multiple links on potentially multiple bots.
      if parsed_options.review_patch_issue:
        cl_images = ('https://%s-gold.skia.org/search?'
                     'issue=%s&new_clstore=true' % (
                       SKIA_GOLD_INSTANCE, parsed_options.review_patch_issue))
        self.artifacts.CreateLink('triage_link_for_entire_cl', cl_images)
      else:
        try:
          with open(failure_file, 'r') as ff:
            self.artifacts.CreateLink('gold_triage_link', ff.read())
        except Exception:
          logging.error('Failed to read contents of goldctl failure file')

      logging.error('goldctl failed with output: %s', e.output)
      if parsed_options.local_run:
        logging.error(
            'Image produced by %s: file://%s', image_name, png_temp_file)
        gold_images = ('https://%s-gold.skia.org/search?'
                      'match=name&metric=combined&pos=true&'
                      'query=name%%3D%s&unt=false' % (
                          SKIA_GOLD_INSTANCE, image_name))
        logging.error(
            'Approved images for %s in Gold: %s', image_name, gold_images)
      if self._ShouldReportGoldFailure(page):
        raise Exception('goldctl command failed, see above for details')

  def _ShouldReportGoldFailure(self, page):
    """Determines if a Gold failure should actually be surfaced.

    Args:
      page: The GPU PixelTestPage object for the test.

    Returns:
      True if the failure should be surfaced, i.e. the test should fail,
      otherwise False.
    """
    parsed_options = self.GetParsedCommandLineOptions()
    # Don't surface if we're explicitly told not to.
    if parsed_options.no_skia_gold_failure:
      return False
    # Don't surface if the test was recently added and we're still within its
    # grace period. However, fail if we're on a trybot so that as many images
    # can be triaged as possible before a new test is committed.
    if (page.grace_period_end and date.today() <= page.grace_period_end and
        not parsed_options.review_patch_issue):
      return False
    return True

  def _ValidateScreenshotSamplesWithSkiaGold(self, tab, page, screenshot,
                                             device_pixel_ratio,
                                             build_id_args):
    """Samples the given screenshot and verifies pixel color values.

    In case any of the samples do not match the expected color, it raises
    a Failure and uploads the image to Gold.

    Args:
      tab: the Telemetry Tab object that the test was run in.
      page: the GPU PixelTestPage object for the test.
      screenshot: the screenshot of the test page as a Telemetry Bitmap.
      device_pixel_ratio: the device pixel ratio for the test device as a float.
      build_id_args: a list of build-identifying flags and values.
    """
    try:
      self._CompareScreenshotSamples(
        tab, screenshot, page.expected_colors, page.tolerance,
        device_pixel_ratio,
        self.GetParsedCommandLineOptions().test_machine_name)
    except Exception:
      # An exception raised from self.fail() indicates a failure.
      image_name = self._UrlToImageName(page.name)
      # We want to report the screenshot comparison failure, not any failures
      # related to Gold.
      try:
        self._UploadTestResultToSkiaGold(
          image_name, screenshot,
          tab, page,
          build_id_args=build_id_args)
      except Exception as e:
        logging.error(str(e))
      raise

  @classmethod
  def _IsLocalRun(cls):
    """Returns whether the test is running on a local workstation or not."""
    # Do nothing if we've already determine whether we're in local mode or not.
    if cls._local_run is not None:
      pass
    # Use the --local-run value if it's been set.
    elif cls.GetParsedCommandLineOptions().local_run is not None:
      cls._local_run = cls.GetParsedCommandLineOptions().local_run
    # Look for the presence of a git repo as a heuristic to determine whether
    # we're running on a workstation or a bot.
    else:
      with RunInChromiumSrc():
        try:
          subprocess.check_call(['git', 'status'], shell=IsWin())
          logging.warning(
              'Automatically determined that test is running on a workstation')
          cls._local_run = True
        except subprocess.CalledProcessError:
          logging.warning(
              'Automatically determined that test is running on a bot')
          cls._local_run = False
    return cls._local_run

  @classmethod
  def _GetBuildRevision(cls):
    """Returns the current git master revision being tested."""
    # Do nothing if we've already determined the build revision.
    if cls._build_revision is not None:
      pass
    # use the --build-revision value if it's been set.
    elif cls.GetParsedCommandLineOptions().build_revision:
      cls._build_revision = cls.GetParsedCommandLineOptions().build_revision
    # Try to determine what revision we're on using git.
    else:
      with RunInChromiumSrc():
        try:
          cls._build_revision = subprocess.check_output(
              ['git', 'rev-parse', 'origin/master'], shell=IsWin()).strip()
          logging.warning('Automatically determined build revision to be %s',
              cls._build_revision)
        except subprocess.CalledProcessError:
          raise Exception('--build-revision not passed, and unable to '
                          'determine revision using git')
    return cls._build_revision

  @classmethod
  def GenerateGpuTests(cls, options):
    del options
    return []

  def RunActualGpuTest(self, options):
    raise NotImplementedError(
        'RunActualGpuTest must be overridden in a subclass')

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])

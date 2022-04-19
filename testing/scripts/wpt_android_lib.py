# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run web platform tests for Chromium-related products on Android."""

import argparse
import contextlib
import logging
import os
import sys

import common
import wpt_common

logger = logging.getLogger(__name__)

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

BUILD_ANDROID = os.path.join(SRC_DIR, 'build', 'android')
BLINK_TOOLS_DIR = os.path.join(
    SRC_DIR, 'third_party', 'blink', 'tools')
CATAPULT_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult')
PYUTILS = os.path.join(CATAPULT_DIR, 'common', 'py_utils')
TOMBSTONE_PARSER = os.path.join(SRC_DIR, 'build', 'android', 'tombstones.py')

if PYUTILS not in sys.path:
  sys.path.append(PYUTILS)

if BLINK_TOOLS_DIR not in sys.path:
  sys.path.append(BLINK_TOOLS_DIR)

if BUILD_ANDROID not in sys.path:
  sys.path.append(BUILD_ANDROID)

# This import adds `devil` to `sys.path`.
import devil_chromium

from blinkpy.web_tests.port.android import (
    PRODUCTS,
    PRODUCTS_TO_EXPECTATION_FILE_PATHS,
    ANDROID_WEBLAYER,
    ANDROID_WEBVIEW,
    CHROME_ANDROID,
    ANDROID_DISABLED_TESTS,
)

from devil import devil_env
from devil.android import apk_helper
from devil.android import device_utils
from devil.android.device_errors import CommandFailedError
from devil.android.tools import webview_app

from py_utils.tempfile_ext import NamedTemporaryDirectory
from pylib.local.emulator import avd


def _make_pass_through_action(dest, map_arg=lambda arg: arg):
    class PassThroughAction(argparse.Action):
        def __init__(self, option_strings, dest, nargs=None, **kwargs):
            if nargs is not None and not isinstance(nargs, int):
                raise ValueError('nargs {} not supported for {}'.format(
                    nargs, option_strings))
            super(PassThroughAction, self).__init__(option_strings, dest,
                                                    nargs=nargs, **kwargs)

        def __call__(self, parser, namespace, values, option_string=None):
            if not option_string:
                return
            args = [option_string]
            if self.nargs is None:
                # Typically a single-arg option, but *not* wrapped in a list,
                # as is the case for `nargs=1`.
                args.append(str(values))
            else:
                args.extend(map(str, values))
            # Use the single-arg form of a long option. Easier to read with
            # option prefixing. Example:
            #   --binary-arg=--enable-blink-features=Feature
            # instead of
            #   --binary-arg=--enable-blink-features --binary-arg=Feature
            if len(args) == 2 and args[0].startswith('--'):
                args = ['%s=%s' % (args[0], args[1])]
            wpt_args = getattr(namespace, dest, [])
            wpt_args.extend(map(map_arg, args))
            setattr(namespace, dest, wpt_args)

    return PassThroughAction


WPTPassThroughAction = _make_pass_through_action('wpt_args')
BinaryPassThroughAction = _make_pass_through_action(
    'wpt_args',
    lambda arg: '--binary-arg=%s' % arg)


class WPTAndroidAdapter(wpt_common.BaseWptScriptAdapter):

  def __init__(self):
    self._metadata_dir = None
    super(WPTAndroidAdapter, self).__init__()
    # Parent adapter adds extra arguments, so it is safe to parse the arguments
    # and set options here.
    try:
      self.parse_args()
      product_cls = _product_registry[self.options.product_name]
      self.product = product_cls(self.options, self.select_python_executable())
    except ValueError as exc:
      self._parser.error(str(exc))

  def parse_args(self, args=None):
    super(WPTAndroidAdapter, self).parse_args(args)
    if not hasattr(self.options, 'wpt_args'):
        self.options.wpt_args = []
    logging.basicConfig(
        level=self.log_level,
        # Align level name for easier reading.
        format='%(asctime)s [%(levelname)-8s] %(name)s: %(message)s',
        force=True)

  @property
  def rest_args(self):
    rest_args = super(WPTAndroidAdapter, self).rest_args

    rest_args.extend([
      '--webdriver-binary',
      self.options.webdriver_binary,
      '--symbols-path',
      self.output_directory,
      '--stackwalk-binary',
      TOMBSTONE_PARSER,
      '--headless',
      # Exclude webdriver tests for now.
      "--exclude=webdriver",
      "--exclude=infrastructure/webdriver",
      '--binary-arg=--enable-blink-features=MojoJS,MojoJSTest',
      '--binary-arg=--enable-blink-test-features',
      '--binary-arg=--disable-field-trial-config',
      '--binary-arg=--enable-features=DownloadService<DownloadServiceStudy',
      '--binary-arg=--force-fieldtrials=DownloadServiceStudy/Enabled',
      '--binary-arg=--force-fieldtrial-params=DownloadServiceStudy.Enabled:'
      'start_up_delay_ms/0',
    ])
    rest_args.extend(self.product.wpt_args)

    # if metadata was created then add the metadata directory
    # to the list of wpt arguments
    if self._metadata_dir:
      rest_args.extend(['--metadata', self._metadata_dir])

    if self.options.test_filter:
      for pattern in self.options.test_filter.split(':'):
        rest_args.extend([
          '--include',
          self.path_finder.strip_wpt_path(pattern),
        ])

    rest_args.extend(self.options.wpt_args)
    return rest_args

  def _extra_metadata_builder_args(self):
    expectations = [ANDROID_DISABLED_TESTS]
    expectations.extend(self.options.additional_expectations)
    if not self.options.ignore_browser_specific_expectations:
      expectations.extend(self.product.expectations)
    return ['--additional-expectations=%s' % expectation
            for expectation in expectations]

  def _maybe_build_metadata(self):
    metadata_builder_cmd = [
      self.select_python_executable(),
      os.path.join(wpt_common.BLINK_TOOLS_DIR, 'build_wpt_metadata.py'),
      '--android-product',
      self.wpt_product_name(),
      '--metadata-output-dir',
      self._metadata_dir,
      '--use-subtest-results',
    ]
    if self.options.ignore_default_expectations:
      metadata_builder_cmd += [ '--ignore-default-expectations' ]
    metadata_builder_cmd.extend(self._extra_metadata_builder_args())
    return common.run_command(metadata_builder_cmd)

  @property
  def log_level(self):
    if self.options.verbose >= 2:
      return logging.DEBUG
    elif self.options.verbose >= 1:
      return logging.INFO
    return logging.WARNING

  def run_test(self):
    with NamedTemporaryDirectory() as tmp_dir, self.product.test_env():
      self._metadata_dir = os.path.join(tmp_dir, 'metadata_dir')
      metadata_command_ret = self._maybe_build_metadata()
      if metadata_command_ret != 0:
        return metadata_command_ret

      # If there is no metadata then we need to create an
      # empty directory to pass to wptrunner
      if not os.path.exists(self._metadata_dir):
        os.makedirs(self._metadata_dir)
      return super(WPTAndroidAdapter, self).run_test()

  def clean_up_after_test_run(self):
    # Avoid having a dangling reference to the temp directory
    # which was deleted
    self._metadata_dir = None

  def add_extra_arguments(self, parser):
    super(WPTAndroidAdapter, self).add_extra_arguments(parser)
    parser.description = __doc__
    self.add_metadata_arguments(parser)
    self.add_binary_arguments(parser)
    self.add_test_arguments(parser)
    self.add_android_arguments(parser)
    parser.add_argument('-p',
                        '--product',
                        # TODO(crbug/1274933): Change to 'chrome' once it
                        # implements 'Product'.
                        dest='product_name',
                        default='clank',
                        # The parser converts the value before checking if it is
                        # in choices, so we avoid looking up the class right
                        # away.
                        choices=sorted(_product_registry, key=len),
                        help='Product (browser or browser component) to test.')
    parser.add_argument('--webdriver-binary', required=True,
                        help='Path of the webdriver binary.  It needs to have'
                        ' the same major version as the apk.')
    parser.add_argument('--webdriver-arg', action=WPTPassThroughAction,
                        help='WebDriver args.')

  def add_metadata_arguments(self, parser):
      group = parser.add_argument_group(
          'Metadata Builder',
          'Options for building WPT metadata from web test expectations.')
      group.add_argument(
          '--additional-expectations',
          metavar='EXPECTATIONS_FILE',
          action='append',
          default=[],
          help='Paths to additional test expectations files.')
      group.add_argument(
          '--ignore-default-expectations',
          action='store_true',
          help='Do not use the default set of TestExpectations files.')
      group.add_argument(
          '--ignore-browser-specific-expectations',
          action='store_true',
          default=False,
          help='Ignore browser-specific expectation files.')
      return group

  def add_binary_arguments(self, parser):
      group = parser.add_argument_group(
          'Binary Configuration',
          'Options for configuring the binary under test.')
      group.add_argument(
          '--enable-features',
          metavar='FEATURES',
          action=BinaryPassThroughAction,
          help='Chromium features to enable during testing.')
      group.add_argument(
          '--disable-features',
          metavar='FEATURES',
          action=BinaryPassThroughAction,
          help='Chromium features to disable during testing.')
      group.add_argument(
          '--force-fieldtrials',
          metavar='TRIALS',
          action=BinaryPassThroughAction,
          help='Force trials for Chromium features.')
      group.add_argument(
          '--force-fieldtrial-params',
          metavar='TRIAL_PARAMS',
          action=BinaryPassThroughAction,
          help='Force trial params for Chromium features.')
      return group

  def add_test_arguments(self, parser):
      group = parser.add_argument_group(
          'Test Selection',
          'Options for selecting tests to run.')
      group.add_argument(
          '--include',
          metavar='TEST_OR_DIR',
          action=WPTPassThroughAction,
          help=('Test(s) to run. '
                "Defaults to all tests, if '--default-exclude' not provided."))
      group.add_argument(
          '--include-file',
          action=WPTPassThroughAction,
          help='A file listing test(s) to run.')
      group.add_argument(
          '--test-filter',
          '--gtest_filter',
          metavar='TESTS_OR_DIRS',
          help='Colon-separated list of test names (URL prefixes).')
      return group

  def add_mode_arguments(self, parser):
      group = super(WPTAndroidAdapter, self).add_mode_arguments(parser)
      group.add_argument(
          '--list-tests',
          nargs=0,
          action=WPTPassThroughAction,
          help='List all tests that will run.')
      return group

  def add_output_arguments(self, parser):
      group = super(WPTAndroidAdapter, self).add_output_arguments(parser)
      group.add_argument(
          '--log-raw',
          metavar='RAW_REPORT_FILE',
          action=WPTPassThroughAction,
          help='Log raw report.')
      group.add_argument(
          '--log-html',
          metavar='HTML_REPORT_FILE',
          action=WPTPassThroughAction,
          help='Log html report.')
      group.add_argument(
          '--log-xunit',
          metavar='XUNIT_REPORT_FILE',
          action=WPTPassThroughAction,
          help='Log xunit report.')
      return group

  def add_android_arguments(self, parser):
      group = parser.add_argument_group(
          'Android',
          'Options for configuring Android devices and tooling.')
      add_emulator_args(group)
      group.add_argument(
          '--apk',
          # Aliases for backwards compatibility.
          '--chrome-apk',
          '--weblayer-support',
          action='append',
          default=[],
          help='Path to an APK to install.')
      group.add_argument(
          '--shell-apk',
          # Aliases for backwards compatibility.
          '--system-webview-shell',
          '--weblayer-shell',
          help=('Path to a shell APK to install. '
                '(WebView and WebLayer only. '
                'Defaults to an on-device APK if not provided.)'))
      group.add_argument(
          '--webview-provider',
          help=('Path to a WebView provider APK to install. '
                '(WebView only.)'))
      group.add_argument(
          '--release-channel',
          help='Install WebView from release channel. (WebView only.)')
      group.add_argument(
          '--package-name',
          # Aliases for backwards compatibility.
          '--chrome-package-name',
          help='Package name to run tests against.')
      group.add_argument(
          '--adb-binary',
          type=os.path.realpath,
          help='Path to adb binary to use.')
      return group

  def wpt_product_name(self):
    # `self.product` may not be set yet, so `self.product.name` is unavailable.
    # `self._options.product_name` may be an alias, so we need to translate it
    # into its wpt-accepted name.
    product_cls = _product_registry[self._options.product_name]
    return product_cls.name


class Product:
    """A product (browser or browser component) that can run web platform tests.

    Attributes:
        name (str): The official wpt-accepted name of this product.
        aliases (list[str]): Human-friendly aliases for the official name.
    """
    name = ''
    aliases = []

    def __init__(self, options, python_executable=None):
        self._options = options
        self._python_executable = python_executable
        self._tasks = contextlib.ExitStack()
        self._validate_options()

    def _validate_options(self):
        """Validate product-specific command-line options.

        The validity of some options may depend on the product. We check these
        options here instead of at parse time because the product itself is an
        option and the parser cannot handle that dependency.

        The test environment will not be set up at this point, so checks should
        not depend on external resources.

        Raises:
            ValueError: When the given options are invalid for this product.
                The user will see the error's message (formatted with
                `argparse`, not a traceback) and the program will exit early,
                which avoids wasted runtime.
        """

    @contextlib.contextmanager
    def test_env(self):
        """Set up and clean up the test environment."""
        with self._tasks:
            yield

    @property
    def wpt_args(self):
        """list[str]: Arguments to add to a 'wpt run' command."""
        args = []
        version = self.get_version()
        if version:
            args.append('--browser-version=%s' % version)
        return args

    @property
    def expectations(self):
        """list[str]: Paths to additional expectations to build metadata for."""
        return []

    def get_version(self):
        """Get the product version, if available."""
        return None


@contextlib.contextmanager
def _install_apk(device, path):
    """Helper context manager for ensuring a device uninstalls an APK."""
    device.Install(path)
    try:
        yield
    finally:
        device.Uninstall(path)


class ChromeAndroidBase(Product):
    def __init__(self, options, python_executable=None):
        super(ChromeAndroidBase, self).__init__(options, python_executable)
        self.devices = {}

    @contextlib.contextmanager
    def test_env(self):
        with super(ChromeAndroidBase, self).test_env():
            devil_chromium.Initialize(adb_path=self._options.adb_binary)
            if not self._options.adb_binary:
                self._options.adb_binary = devil_env.config.FetchPath('adb')
            devices = self._tasks.enter_context(get_devices(self._options))
            if not devices:
                raise Exception(
                    'No devices attached to this host. '
                    "Make sure to provide '--avd-config' "
                    'if using only emulators.')
            for device in devices:
                self.provision_device(device)
            yield

    @property
    def wpt_args(self):
        wpt_args = list(super(ChromeAndroidBase, self).wpt_args)
        for serial in self.devices:
            wpt_args.append('--device-serial=%s' % serial)
        package_name = self.get_browser_package_name()
        if package_name:
            wpt_args.append('--package-name=%s' % package_name)
        if self._options.adb_binary:
            wpt_args.append('--adb-binary=%s' % self._options.adb_binary)
        return wpt_args

    @property
    def expectations(self):
        expectations = list(super(ChromeAndroidBase, self).expectations)
        maybe_path = PRODUCTS_TO_EXPECTATION_FILE_PATHS.get(self.name)
        if maybe_path:
            expectations.append(maybe_path)
        return expectations

    def get_version(self):
        version_provider = self.get_version_provider_package_name()
        if self.devices and version_provider:
            # Assume devices are identically provisioned, so select any.
            device = list(self.devices.values())[0]
            try:
                version = device.GetApplicationVersion(version_provider)
                logger.info('Product version: %s %s (package: %r)',
                            self.name, version, version_provider)
                return version
            except CommandFailedError:
                logger.warning('Failed to retrieve version of %s (package: %r)',
                               self.name, version_provider)
        return None

    def get_browser_package_name(self):
        """Get the name of the package to run tests against.

        For WebView and WebLayer, this package is the shell.

        Returns:
            Optional[str]: The name of a package installed on the devices or
                `None` to use wpt's best guess of the runnable package.

        See Also:
            https://github.com/web-platform-tests/wpt/blob/merge_pr_33203/tools/wpt/browser.py#L867-L924
        """
        return self._options.package_name

    def get_version_provider_package_name(self):
        """Get the name of the package containing the product version.

        Some Android products are made up of multiple packages with decoupled
        "versionName" fields. This method identifies the package whose
        "versionName" should be consider the product's version.

        Returns:
            Optional[str]: The name of a package installed on the devices or
                `None` to use wpt's best guess of the version.

        See Also:
            https://github.com/web-platform-tests/wpt/blob/merge_pr_33203/tools/wpt/run.py#L810-L816
            https://github.com/web-platform-tests/wpt/blob/merge_pr_33203/tools/wpt/browser.py#L850-L924
        """
        # Assume the product is a single APK.
        return self.get_browser_package_name()

    def provision_device(self, device):
        """Provision an Android device for a test."""
        for apk in self._options.apk:
            self._tasks.enter_context(_install_apk(device, apk))
        logger.info('Provisioned device (serial: %s)', device.serial)

        if device.serial in self.devices:
            raise Exception('duplicate device serial: %s' % device.serial)
        self.devices[device.serial] = device
        self._tasks.callback(self.devices.pop, device.serial, None)


@contextlib.contextmanager
def _install_webview_from_release(device, channel, python_executable=None):
    script_path = os.path.join(SRC_DIR, 'clank', 'bin', 'install_webview.py')
    python_executable = python_executable or sys.executable
    command = [python_executable, script_path, '-s', device.serial, '--channel',
               channel]
    exit_code = common.run_command(command)
    if exit_code != 0:
        raise Exception('failed to install webview from release '
                        '(serial: %r, channel: %r, exit code: %d)'
                        % (device.serial, channel, exit_code))
    yield


class ChromeAndroidShellBase(ChromeAndroidBase):
    def get_browser_package_name(self):
        package_name = super(ChromeAndroidShellBase,
                             self).get_browser_package_name()
        if package_name:
            return package_name
        elif self._options.shell_apk:
            with contextlib.suppress(apk_helper.ApkHelperError):
                return apk_helper.GetPackageName(self._options.shell_apk)
        return None

    def provision_device(self, device):
        if self._options.shell_apk:
            self._tasks.enter_context(_install_apk(device,
                                                   self._options.shell_apk))
        super(ChromeAndroidShellBase, self).provision_device(device)


class WebLayer(ChromeAndroidShellBase):
    name = ANDROID_WEBLAYER
    aliases = ['weblayer']

    @property
    def wpt_args(self):
        args = list(super(WebLayer, self).wpt_args)
        args.append('--test-type=testharness')
        return args

    def get_browser_package_name(self):
        return (super(WebLayer, self).get_browser_package_name()
                or 'org.chromium.weblayer.shell')

    def get_version_provider_package_name(self):
        # Read version from support APK.
        if self._options.apk:
            with contextlib.suppress(apk_helper.ApkHelperError):
                return apk_helper.GetPackageName(self._options.apk[0])
        return super(WebLayer, self).get_version_provider_package_name()


class WebView(ChromeAndroidShellBase):
    name = ANDROID_WEBVIEW
    aliases = ['webview']

    def _install_webview(self, device):
        # Prioritize local builds.
        if self._options.webview_provider:
            return webview_app.UseWebViewProvider(
                device,
                self._options.webview_provider)
        else:
            assert self._options.release_channel, 'no webview install method'
            return _install_webview_from_release(
                device,
                self._options.release_channel,
                self._python_executable)

    def _validate_options(self):
        super(WebView, self)._validate_options()
        if not self._options.webview_provider \
                and not self._options.release_channel:
            raise ValueError(
                "Must provide either '--webview-provider' or "
                "'--release-channel' to install WebView.")

    def get_browser_package_name(self):
        return (super(WebView, self).get_browser_package_name()
                or 'org.chromium.webview_shell')

    def get_version_provider_package_name(self):
        # Prioritize using the webview provider, not the shell, since the
        # provider is distributed to end users. The shell is developer-facing,
        # so its version is usually not actively updated.
        if self._options.webview_provider:
            with contextlib.suppress(apk_helper.ApkHelperError):
                return apk_helper.GetPackageName(self._options.webview_provider)
        return super(WebView, self).get_version_provider_package_name()

    def provision_device(self, device):
        self._tasks.enter_context(self._install_webview(device))
        super(WebView, self).provision_device(device)


class ChromeAndroid(ChromeAndroidBase):
    name = CHROME_ANDROID
    aliases = ['clank']

    def _validate_options(self):
        super(ChromeAndroid, self)._validate_options()
        if not self._options.package_name and not self._options.apk:
            raise ValueError(
                "Must provide either '--package-name' or '--apk' "
                'for %r.' % self.name)

    def get_browser_package_name(self):
        package_name = super(ChromeAndroid, self).get_browser_package_name()
        if package_name:
            return package_name
        elif self._options.apk:
            with contextlib.suppress(apk_helper.ApkHelperError):
                return apk_helper.GetPackageName(self._options.apk[0])
        return None


def add_emulator_args(parser):
  parser.add_argument(
      '--avd-config',
      type=os.path.realpath,
      help=('Path to the avd config. Required for Android products. '
            '(See //tools/android/avd/proto for message definition '
            'and existing *.textpb files.)'))
  parser.add_argument(
      '--emulator-window',
      action='store_true',
      default=False,
      help='Enable graphical window display on the emulator.')
  parser.add_argument(
      '-j', '--processes', dest='processes',
      type=int,
      default=1,
      help='Number of emulator to run.')


def _make_product_registry():
    """Create a mapping from all product names (including aliases) to their
    respective classes.
    """
    product_registry = {}
    product_classes = [ChromeAndroid, WebView, WebLayer]
    for product_cls in product_classes:
        names = [product_cls.name] + product_cls.aliases
        product_registry.update((name, product_cls) for name in names)
    return product_registry


_product_registry = _make_product_registry()


@contextlib.contextmanager
def get_device(args):
  with get_devices(args) as devices:
    yield None if not devices else devices[0]


@contextlib.contextmanager
def get_devices(args):
  instances = []
  try:
    if args.avd_config:
      avd_config = avd.AvdConfig(args.avd_config)
      logger.warning('Installing emulator from %s', args.avd_config)
      avd_config.Install()
      for _ in range(max(args.processes, 1)):
        instance = avd_config.CreateInstance()
        instance.Start(writable_system=True, window=args.emulator_window)
        instances.append(instance)

    #TODO(weizhong): when choose device, make sure abi matches with target
    yield device_utils.DeviceUtils.HealthyDevices()
  finally:
    for instance in instances:
      instance.Stop()

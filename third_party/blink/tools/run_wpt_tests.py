#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run web platform tests for Chromium-related products."""

import argparse
import contextlib
import json
import logging
import os
import shutil
import sys

from blinkpy.common import exit_codes
from blinkpy.common import path_finder
from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.port.android import (
    ANDROID_WEBLAYER,
    ANDROID_WEBVIEW,
    CHROME_ANDROID,
)

path_finder.add_testing_dir_to_sys_path()
path_finder.add_build_android_to_sys_path()

from scripts import wpt_common
from scripts import common

logger = logging.getLogger(__name__)

UPSTREAM_GIT_URL = 'https://github.com/web-platform-tests/wpt.git'

try:
    # This import adds `devil` to `sys.path`.
    import devil_chromium
    from devil import devil_env
    from devil.utils.parallelizer import SyncParallelizer
    from devil.android import apk_helper
    from devil.android import device_utils
    from devil.android.device_errors import CommandFailedError
    from devil.android.tools import webview_app
    from pylib.local.emulator import avd
    _ANDROID_ENABLED = True
except ImportError:
    logger.warning('Android tools not found')
    _ANDROID_ENABLED = False


def _make_pass_through_action(dest, map_arg=lambda arg: arg):
    class PassThroughAction(argparse.Action):
        def __init__(self, option_strings, dest, nargs=None, **kwargs):
            if nargs is not None and not isinstance(nargs, int):
                raise ValueError('nargs {} not supported for {}'.format(
                    nargs, option_strings))
            super().__init__(option_strings, dest, nargs=nargs, **kwargs)

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
    'wpt_args', lambda arg: '--binary-arg=%s' % arg)


class WPTAdapter(wpt_common.BaseWptScriptAdapter):
    def __init__(self):
        self._tmp_dir = None
        super().__init__()
        # Parent adapter adds extra arguments, so it is safe to parse the
        # arguments and set options here.
        try:
            self.parse_args()
            product_cls = _product_registry[self.options.product_name]
            self.product = product_cls(self.host, self.options,
                                       self.select_python_executable())
            self.add_default_child_process()
        except ValueError as exc:
            self._parser.error(str(exc))

    @property
    def _upstream_dir(self):
        return self.fs.join(self._tmp_dir, 'upstream_wpt')

    def add_default_child_process(self):
        if self._options.processes is None:
            if self.product.name == 'chrome' or self.product.name == 'content_shell':
                self._options.processes = self.port.default_child_processes()
            else:
                self._options.processes = 1

    def parse_args(self, args=None):
        super().parse_args(args)
        if not hasattr(self.options, 'wpt_args'):
            self.options.wpt_args = []
        logging.basicConfig(
            level=self.log_level,
            # Align level name for easier reading.
            format='%(asctime)s [%(levelname)-8s] %(name)s: %(message)s',
            force=True)

    @property
    def _default_retry_limit(self) -> int:
        if self.options.use_upstream_wpt:
            return 0
        return super()._default_retry_limit

    @property
    def wpt_binary(self):
        if self.options.use_upstream_wpt:
            return os.path.join(self._upstream_dir, "wpt")
        return super().wpt_binary

    @property
    def wpt_root_dir(self):
        if self.options.use_upstream_wpt:
            return self._upstream_dir
        return super().wpt_root_dir

    @property
    def rest_args(self):
        rest_args = super().rest_args
        rest_args.extend([
            '--webdriver-arg=--enable-chrome-logs',
            # TODO(crbug/1316055): Enable tombstone with '--stackwalk-binary'
            # and '--symbols-path'.
            # Exclude webdriver tests for now. The CI runs them separately.
            '--exclude=webdriver',
            '--exclude=infrastructure/webdriver',
            '--binary-arg=--host-resolver-rules='
            'MAP nonexistent.*.test ~NOTFOUND, MAP *.test 127.0.0.1',
            '--binary-arg=--enable-experimental-web-platform-features',
            '--binary-arg=--enable-blink-features=MojoJS,MojoJSTest',
            '--binary-arg=--enable-blink-test-features',
            '--binary-arg=--disable-field-trial-config',
            '--binary-arg=--enable-features='
            'DownloadService<DownloadServiceStudy',
            '--binary-arg=--force-fieldtrials=DownloadServiceStudy/Enabled',
            '--binary-arg=--force-fieldtrial-params='
            'DownloadServiceStudy.Enabled:start_up_delay_ms/0',
            '--run-info=%s' % self._tmp_dir,
        ])
        if self.options.use_upstream_wpt:
            # when running with upstream, the goal is to get wpt report that can
            # be uploaded to wpt.fyi. We do not really cares if tests failed or
            # not. Add '--no-fail-on-unexpected' so that the overall result is
            # success. Add '--no-restart-on-unexpected' to speed up the test. On
            # Android side, we are always running with one emulator or worker,
            # so do not add '--run-by-dir=0'
            rest_args.extend([
                '--no-fail-on-unexpected',
                '--no-restart-on-unexpected',
            ])
        else:
            # By default, wpt will treat unexpected passes as errors, so we
            # disable that to be consistent with Chromium CI. Add
            # '--run-by-dir=0' so that tests can be more evenly distributed
            # among workers.
            rest_args.extend([
                '--no-fail-on-unexpected-pass',
                '--no-restart-on-new-group',
                '--run-by-dir=0',
            ])

        rest_args.extend(self.product.wpt_args)

        if self.options.headless:
            rest_args.append('--headless')

        if self.options.run_wpt_internal:
            rest_args.extend([
                '--config',
                self.path_finder.path_from_web_tests("wptrunner.blink.ini")
            ])

        if self.options.flag_specific:
            configs = self.port.flag_specific_configs()
            rest_args.extend('--binary-arg=%s' % arg
                             for arg in configs[self.options.flag_specific][0])
            if configs[self.options.flag_specific][1] is not None:
                smoke_file_path = self.path_finder.path_from_web_tests(
                    configs[self.options.flag_specific][1])
                if not self._has_explicit_tests:
                    rest_args.append('--include-file=%s' % smoke_file_path)

        if self.options.test_filter:
            for pattern in self.options.test_filter.split(':'):
                rest_args.extend([
                    '--include',
                    self.path_finder.strip_wpt_path(pattern),
                ])

        rest_args.extend(self.options.wpt_args)
        return rest_args

    @property
    def log_level(self):
        if self.options.verbose >= 2:
            return logging.DEBUG
        if self.options.verbose >= 1:
            return logging.INFO
        return logging.WARNING

    def run_test(self):
        with contextlib.ExitStack() as stack:
            self._tmp_dir = stack.enter_context(self.fs.mkdtemp())
            # Manually remove the temporary directory's contents recursively
            # after the tests complete. Otherwise, `mkdtemp()` raise an error.
            stack.callback(self.fs.rmtree, self._tmp_dir)
            stack.enter_context(self.product.test_env())
            if self.options.use_upstream_wpt:
                logger.info("Using upstream wpt, cloning to %s ..." %
                            self._upstream_dir)
                # check if directory exists, if it does remove it
                if os.path.isdir(self._upstream_dir):
                    shutil.rmtree(self._upstream_dir, ignore_errors=True)
                # make a temp directory and git pull into it
                clone_cmd = [
                    'git', 'clone', UPSTREAM_GIT_URL, self._upstream_dir,
                    '--depth=1'
                ]
                common.run_command(clone_cmd)

            self._create_extra_run_info()
            return super().run_test(self.path_finder.web_tests_dir())

    def _create_extra_run_info(self):
        run_info = {
            # This property should always be a string so that the metadata
            # updater works, even when wptrunner is not running a flag-specific
            # suite.
            'flag_specific': self.options.flag_specific or '',
            'used_upstream': self.options.use_upstream_wpt,
            'sanitizer_enabled': self.options.enable_sanitizer,
        }
        if self.options.use_upstream_wpt:
            # `run_wpt_tests` does not run in the upstream checkout's git
            # context, so wptrunner cannot infer the latest revision. Manually
            # add the revision here.
            run_info['revision'] = self.host.git(
                path=self.wpt_root_dir).current_revision()
        # The filename must be `mozinfo.json` for wptrunner to read it.
        # The `--run-info` parameter passed to wptrunner is the directory
        # containing `mozinfo.json`.
        run_info_path = self.fs.join(self._tmp_dir, 'mozinfo.json')
        with self.fs.open_text_file_for_writing(run_info_path) as file_handle:
            json.dump(run_info, file_handle)

    def do_post_test_run_tasks(self):
        process_return = self.process_and_upload_results()

        if (process_return != exit_codes.INTERRUPTED_EXIT_STATUS
                and self.options.show_results_in_browser):
            self.show_results_in_browser()

    def show_results_in_browser(self):
        results_file = self.fs.join(self.fs.dirname(self.wpt_output),
                                    self.layout_test_results_subdir,
                                    'results.html')
        self.port.show_results_html_file(results_file)

    def clean_up_after_test_run(self):
        super().clean_up_after_test_run()
        # Avoid having a dangling reference to the temp directory
        # which was deleted
        self._tmp_dir = None

    def add_extra_arguments(self, parser):
        super().add_extra_arguments(parser)
        parser.description = __doc__
        self.add_binary_arguments(parser)
        self.add_test_arguments(parser)
        if _ANDROID_ENABLED:
            self.add_android_arguments(parser)
        parser.add_argument(
            '-p',
            '--product',
            dest='product_name',
            default='content_shell',
            # The parser converts the value before checking if it is in choices,
            # so we avoid looking up the class right away.
            choices=sorted(_product_registry, key=len),
            help='Product (browser or browser component) to test.')
        parser.add_argument('--webdriver-binary',
                            type=os.path.abspath,
                            help=('Path of the webdriver binary.'
                                  'It needs to have the same major version '
                                  'as the browser binary or APK.'))
        parser.add_argument('--webdriver-arg',
                            action=WPTPassThroughAction,
                            help='WebDriver args.')
        parser.add_argument(
            '-j',
            '--processes',
            '--child-processes',
            type=lambda processes: max(0, int(processes)),
            help=('Number of drivers to start in parallel. (For Android, '
                  'this number is the number of emulators started.) '
                  'The actual number of devices tested may be higher '
                  'if physical devices are available.)'))
        parser.add_argument('--use-upstream-wpt',
                            action='store_true',
                            help=('Use the upstream wpt, this tag will clone '
                                  'the upstream github wpt to a temporary '
                                  'directory and will use the binary and '
                                  'tests from upstream'))
        parser.add_argument('--flag-specific',
                            choices=sorted(self.port.flag_specific_configs()),
                            metavar='FLAG_SPECIFIC',
                            help='The name of a flag-specific suite to run.')
        parser.add_argument('--no-headless',
                            action='store_false',
                            dest='headless',
                            default=True,
                            help=('Use this tag to not run wptrunner in'
                                  'headless mode'))
        parser.add_argument('--show-results-in-browser',
                            action='store_true',
                            help='Open the results viewer in a browser.')
        parser.add_argument(
            '--enable-sanitizer',
            action='store_true',
            help='Only report sanitizer-related errors and crashes.')

    def add_binary_arguments(self, parser):
        group = parser.add_argument_group(
            'Binary Configuration',
            'Options for configuring the binary under test.')
        group.add_argument('--enable-features',
                           metavar='FEATURES',
                           action=BinaryPassThroughAction,
                           help='Chromium features to enable during testing.')
        group.add_argument('--disable-features',
                           metavar='FEATURES',
                           action=BinaryPassThroughAction,
                           help='Chromium features to disable during testing.')
        group.add_argument('--force-fieldtrials',
                           metavar='TRIALS',
                           action=BinaryPassThroughAction,
                           help='Force trials for Chromium features.')
        group.add_argument('--force-fieldtrial-params',
                           metavar='TRIAL_PARAMS',
                           action=BinaryPassThroughAction,
                           help='Force trial params for Chromium features.')
        return group

    def add_test_arguments(self, parser):
        group = parser.add_argument_group(
            'Test Selection', 'Options for selecting tests to run.')
        group.add_argument('--include',
                           metavar='TEST_OR_DIR',
                           action=WPTPassThroughAction,
                           help=('Test(s) to run. Defaults to all tests, '
                                 "if '--default-exclude' not provided."))
        group.add_argument('--include-file',
                           action=WPTPassThroughAction,
                           type=os.path.abspath,
                           help='A file listing test(s) to run.')
        group.add_argument(
            '--test-filter',
            '--gtest_filter',
            metavar='TESTS_OR_DIRS',
            help='Colon-separated list of test names (URL prefixes).')
        parser.add_argument('--no-wpt-internal',
                            action='store_false',
                            dest='run_wpt_internal',
                            default=True,
                            help='Do not run internal WPTs.')
        return group

    def add_mode_arguments(self, parser):
        group = super().add_mode_arguments(parser)
        group.add_argument('--list-tests',
                           nargs=0,
                           action=WPTPassThroughAction,
                           help='List all tests that will run.')
        return group

    def add_output_arguments(self, parser):
        group = super().add_output_arguments(parser)
        group.add_argument('--log-raw',
                           metavar='RAW_REPORT_FILE',
                           action=WPTPassThroughAction,
                           help='Log raw report.')
        group.add_argument('--log-html',
                           metavar='HTML_REPORT_FILE',
                           action=WPTPassThroughAction,
                           help='Log html report.')
        group.add_argument('--log-xunit',
                           metavar='XUNIT_REPORT_FILE',
                           action=WPTPassThroughAction,
                           help='Log xunit report.')
        return group

    def add_android_arguments(self, parser):
        group = parser.add_argument_group(
            'Android', 'Options for configuring Android devices and tooling.')
        add_emulator_args(group)
        group.add_argument(
            '--browser-apk',
            # Aliases for backwards compatibility.
            '--chrome-apk',
            '--system-webview-shell',
            '--weblayer-shell',
            type=os.path.abspath,
            help=('Path to the browser APK to install and run. '
                  '(For WebView and WebLayer, this value is the shell. '
                  'Defaults to an on-device APK if not provided.)'))
        group.add_argument('--webview-provider',
                           type=os.path.abspath,
                           help=('Path to a WebView provider APK to install. '
                                 '(WebView only.)'))
        group.add_argument(
            '--additional-apk',
            # Aliases for backwards compatibility.
            '--weblayer-support',
            type=os.path.abspath,
            action='append',
            default=[],
            help='Paths to additional APKs to install.')
        group.add_argument(
            '--release-channel',
            help='Install WebView from release channel. (WebView only.)')
        group.add_argument(
            '--package-name',
            # Aliases for backwards compatibility.
            '--chrome-package-name',
            help='Package name to run tests against.')
        group.add_argument('--adb-binary',
                           type=os.path.abspath,
                           help='Path to adb binary to use.')
        return group

    def wpt_product_name(self):
        # `self.product` may not be set yet, so `self.product.name` is
        # unavailable. `self._options.product_name` may be an alias, so we need
        # to translate it into its wpt-accepted name.
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

    def __init__(self, host, options, python_executable=None):
        self._host = host
        self._path_finder = PathFinder(self._host.filesystem)
        self._options = options
        self._python_executable = python_executable
        self._tasks = contextlib.ExitStack()
        self._validate_options()

    def _path_from_target(self, *components):
        return self._path_finder.path_from_chromium_base(
            'out', self._options.target, *components)

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
        version = self.get_version()  # pylint: disable=assignment-from-none
        if version:
            args.append('--browser-version=%s' % version)
        webdriver = self.webdriver_binary
        if webdriver and self._host.filesystem.exists(webdriver):
            args.append('--webdriver-binary=%s' % webdriver)
        return args

    def get_version(self):
        """Get the product version, if available."""
        return None

    @property
    def webdriver_binary(self):
        """Optional[str]: Path to the webdriver binary, if available."""
        return self._options.webdriver_binary


class ChromeBase(Product):
    @property
    def binary(self):
        raise NotImplementedError

    @property
    def wpt_args(self):
        wpt_args = list(super().wpt_args)
        wpt_args.extend([
            '--binary=%s' % self.binary,
            '--processes=%d' % self._options.processes,
        ])
        return wpt_args


class Chrome(ChromeBase):
    name = 'chrome'

    @property
    def binary(self):
        binary_path = 'chrome'
        if self._host.platform.is_win():
            binary_path += '.exe'
        elif self._host.platform.is_mac():
            binary_path = self._host.filesystem.join('Chromium.app',
                                                     'Contents', 'MacOS',
                                                     'Chromium')
        return self._path_from_target(binary_path)

    @property
    def webdriver_binary(self):
        default_binary = 'chromedriver'
        if self._host.platform.is_win():
            default_binary += '.exe'
        return (super().webdriver_binary
                or self._path_from_target(default_binary))


class ContentShell(ChromeBase):
    name = 'content_shell'

    @property
    def binary(self):
        binary_path = 'content_shell'
        if self._host.platform.is_win():
            binary_path += '.exe'
        elif self._host.platform.is_mac():
            binary_path = self._host.filesystem.join('Content Shell.app',
                                                     'Contents', 'MacOS',
                                                     'Content Shell')
        return self._path_from_target(binary_path)


class ChromeiOS(Product):
    name = 'chrome_ios'

    @property
    def wpt_args(self):
        wpt_args = list(super().wpt_args)
        cwt_chromedriver_path = os.path.realpath(
            os.path.join(
                os.path.dirname(__file__),
                '../../../ios/chrome/test/wpt/tools/'
                'run_cwt_chromedriver_wrapper.py'))
        wpt_args.extend([
            '--processes=%d' % self._options.processes,
            '--webdriver-binary=%s' % cwt_chromedriver_path,
        ])
        return wpt_args


@contextlib.contextmanager
def _install_apk(device, path):
    """Helper context manager for ensuring a device uninstalls an APK."""
    device.Install(path)
    try:
        yield
    finally:
        device.Uninstall(path)


class ChromeAndroidBase(Product):
    def __init__(self, host, options, python_executable=None):
        super().__init__(host, options, python_executable)
        self.devices = {}

    @contextlib.contextmanager
    def test_env(self):
        with super().test_env():
            devil_chromium.Initialize(adb_path=self._options.adb_binary)
            if not self._options.adb_binary:
                self._options.adb_binary = devil_env.config.FetchPath('adb')
            devices = self._tasks.enter_context(get_devices(self._options))
            if not devices:
                raise Exception('No devices attached to this host. '
                                "Make sure to provide '--avd-config' "
                                'if using only emulators.')

            self.provision_devices(devices)
            yield

    @property
    def wpt_args(self):
        wpt_args = list(super().wpt_args)
        for serial in self.devices:
            wpt_args.append('--device-serial=%s' % serial)
        package_name = self.get_browser_package_name()
        if package_name:
            wpt_args.append('--package-name=%s' % package_name)
        if self._options.adb_binary:
            wpt_args.append('--adb-binary=%s' % self._options.adb_binary)
        return wpt_args

    def get_version(self):
        version_provider = self.get_version_provider_package_name()
        if self.devices and version_provider:
            # Assume devices are identically provisioned, so select any.
            device = list(self.devices.values())[0]
            try:
                version = device.GetApplicationVersion(version_provider)
                logger.info('Product version: %s %s (package: %r)', self.name,
                            version, version_provider)
                return version
            except CommandFailedError:
                logger.warning(
                    'Failed to retrieve version of %s (package: %r)',
                    self.name, version_provider)
        return None

    @property
    def webdriver_binary(self):
        default_binary = self._path_from_target('clang_x64', 'chromedriver')
        return super().webdriver_binary or default_binary

    def get_browser_package_name(self):
        """Get the name of the package to run tests against.

        For WebView and WebLayer, this package is the shell.

        Returns:
            Optional[str]: The name of a package installed on the devices or
                `None` to use wpt's best guess of the runnable package.

        See Also:
            https://github.com/web-platform-tests/wpt/blob/merge_pr_33203/tools/wpt/browser.py#L867-L924
        """
        if self._options.package_name:
            return self._options.package_name
        if self._options.browser_apk:
            with contextlib.suppress(apk_helper.ApkHelperError):
                return apk_helper.GetPackageName(self._options.browser_apk)
        return None

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

    def provision_devices(self, devices):
        """Provisions a set of Android devices in parallel."""
        contexts = [self._provision_device(device) for device in devices]
        self._tasks.enter_context(SyncParallelizer(contexts))

        for device in devices:
            if device.serial in self.devices:
                raise Exception('duplicate device serial: %s' % device.serial)
            self.devices[device.serial] = device
            self._tasks.callback(self.devices.pop, device.serial, None)

    @contextlib.contextmanager
    def _provision_device(self, device):
        """Provision a single Android device for a test.

        This method will be executed in parallel on all devices, so
        it is crucial that it is thread safe.
        """
        with contextlib.ExitStack() as exit_stack:
            if self._options.browser_apk:
                exit_stack.enter_context(
                    _install_apk(device, self._options.browser_apk))
            for apk in self._options.additional_apk:
                exit_stack.enter_context(_install_apk(device, apk))
            logger.info('Provisioned device (serial: %s)', device.serial)
            yield


class WebLayer(ChromeAndroidBase):
    name = ANDROID_WEBLAYER
    aliases = ['weblayer']

    @property
    def wpt_args(self):
        args = list(super().wpt_args)
        args.append('--test-type=testharness')
        return args

    def get_browser_package_name(self):
        return (super().get_browser_package_name()
                or 'org.chromium.weblayer.shell')

    def get_version_provider_package_name(self):
        if self._options.additional_apk:
            support_apk = self._options.additional_apk[0]
            with contextlib.suppress(apk_helper.ApkHelperError):
                return apk_helper.GetPackageName(support_apk)
        return super().get_version_provider_package_name()


class WebView(ChromeAndroidBase):
    name = ANDROID_WEBVIEW
    aliases = ['webview']

    def _install_webview(self, device):
        # Prioritize local builds.
        if self._options.webview_provider:
            return webview_app.UseWebViewProvider(
                device, self._options.webview_provider)
        assert self._options.release_channel, 'no webview install method'
        return self._install_webview_from_release(device)

    def _validate_options(self):
        super()._validate_options()
        if not self._options.webview_provider \
                and not self._options.release_channel:
            raise ValueError("Must provide either '--webview-provider' or "
                             "'--release-channel' to install WebView.")

    def get_browser_package_name(self):
        return (super().get_browser_package_name()
                or 'org.chromium.webview_shell')

    def get_version_provider_package_name(self):
        # Prioritize using the webview provider, not the shell, since the
        # provider is distributed to end users. The shell is developer-facing,
        # so its version is usually not actively updated.
        if self._options.webview_provider:
            with contextlib.suppress(apk_helper.ApkHelperError):
                return apk_helper.GetPackageName(
                    self._options.webview_provider)
        return super().get_version_provider_package_name()

    @contextlib.contextmanager
    def _provision_device(self, device):
        with self._install_webview(device), super()._provision_device(device):
            yield

    @contextlib.contextmanager
    def _install_webview_from_release(self, device):
        script_path = self._path_finder.path_from_chromium_base(
            'clank', 'bin', 'install_webview.py')
        command = [
            self._python_executable, script_path, '-s', device.serial,
            '--channel', self._options.release_channel
        ]
        exit_code = common.run_command(command)
        if exit_code != 0:
            raise Exception(
                'failed to install webview from release '
                '(serial: %r, channel: %r, exit code: %d)' %
                (device.serial, self._options.release_channel, exit_code))
        yield


class ChromeAndroid(ChromeAndroidBase):
    name = CHROME_ANDROID
    aliases = ['clank']

    def _validate_options(self):
        super()._validate_options()
        if not self._options.package_name and not self._options.browser_apk:
            raise ValueError(
                "Must provide either '--package-name' or '--browser-apk' "
                'for %r.' % self.name)


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


def _make_product_registry():
    """Create a mapping from all product names (including aliases) to their
    respective classes.
    """
    product_registry = {}
    product_classes = [Chrome, ContentShell, ChromeiOS]
    if _ANDROID_ENABLED:
        product_classes.extend([ChromeAndroid, WebView, WebLayer])
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
    if not _ANDROID_ENABLED:
        raise Exception('Android is not available')
    instances = []
    try:
        if args.avd_config:
            avd_config = avd.AvdConfig(args.avd_config)
            logger.warning('Installing emulator from %s', args.avd_config)
            avd_config.Install()

            for _ in range(max(args.processes, 1)):
                instance = avd_config.CreateInstance()
                instances.append(instance)

            SyncParallelizer(instances).Start(writable_system=True,
                                              window=args.emulator_window,
                                              require_fast_start=True)

        #TODO(weizhong): when choose device, make sure abi matches with target
        yield device_utils.DeviceUtils.HealthyDevices()
    finally:
        SyncParallelizer(instances).Stop()


def main():
    # This environment fix is needed on windows as codec is trying
    # to encode in cp1252 rather than utf-8 and throwing errors.
    os.environ['PYTHONUTF8'] = '1'

    try:
        adapter = WPTAdapter()
        return adapter.run_test()
    except KeyboardInterrupt:
        return exit_codes.INTERRUPTED_EXIT_STATUS


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
    json.dump([], args.output)


if __name__ == '__main__':
    # Conform minimally to the protocol defined by ScriptTest.
    if 'compile_targets' in sys.argv:
        funcs = {
            'run': None,
            'compile_targets': main_compile_targets,
        }
        sys.exit(common.run_script(sys.argv[1:], funcs))
    sys.exit(main())

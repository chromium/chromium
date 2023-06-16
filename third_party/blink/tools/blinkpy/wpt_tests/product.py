# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Product classes that encapsulate the interfaces for the testing targets"""

import contextlib
import subprocess
import logging

from blinkpy.common import path_finder
from blinkpy.common.memoized import memoized


def do_delay_imports():
    global devil_chromium, devil_env, apk_helper
    global device_utils, webview_app, avd
    global CommandFailedError, SyncParallelizer
    # Packages here are only isolated when running test for Android
    # This import adds `devil` to `sys.path`.
    path_finder.add_build_android_to_sys_path()
    import devil_chromium
    from devil import devil_env
    from devil.android import apk_helper
    from devil.android import device_utils
    from devil.android.device_errors import CommandFailedError
    from devil.android.tools import webview_app
    from devil.utils.parallelizer import SyncParallelizer
    from pylib.local.emulator import avd


@memoized
def make_product_registry():
    """Create a mapping from all product names (including aliases) to their
    respective classes.
    """
    product_registry = {}
    product_classes = [Chrome, ContentShell, ChromeiOS, ChromeAndroid, WebView]
    for product_cls in product_classes:
        names = [product_cls.name] + product_cls.aliases
        product_registry.update((name, product_cls) for name in names)
    return product_registry


class Product:
    """A product (browser or browser component) that can run web platform tests.

    Attributes:
        name (str): The official wpt-accepted name of this product.
        aliases (list[str]): Human-friendly aliases for the official name.
    """
    name = ''
    aliases = []

    def __init__(self, port, options):
        self._port = port
        self._host = port.host
        self._options = options
        self._tasks = contextlib.ExitStack()

    @contextlib.contextmanager
    def test_env(self):
        """Set up and clean up the test environment."""
        with self._tasks:
            yield

    def product_specific_options(self):
        """Product-specific wptrunner parameters needed to run tests."""
        return {
            'processes': (self._options.child_processes
                          or self._port.default_child_processes()),
        }

    def additional_webdriver_args(self):
        """Additional webdriver parameters for the product"""
        return []

    def get_version(self):
        """Get the product version, if available."""
        return None

    @property
    def default_webdriver_binary(self):
        """Path to the default webdriver binary, if available."""
        return None

    @property
    def default_binary(self):
        return None


class Chrome(Product):
    name = 'chrome'

    def product_specific_options(self):
        """Product-specific wptrunner parameters needed to run tests."""
        return {
            **super().product_specific_options(),
            'binary': self.default_binary,
            'webdriver_binary': self.default_webdriver_binary,
        }

    @property
    def default_binary(self):
        binary_path = 'chrome'
        if self._host.platform.is_win():
            binary_path += '.exe'
        elif self._host.platform.is_mac():
            binary_path = self._host.filesystem.join('Chromium.app',
                                                     'Contents', 'MacOS',
                                                     'Chromium')
        return self._port.build_path(binary_path)

    @property
    def default_webdriver_binary(self):
        if self._host.platform.is_win():
            path = 'chromedriver.exe'
        else:
            path = 'chromedriver'  #linux and mac
        return self._port.build_path(path)


class ContentShell(Product):
    name = 'content_shell'

    def product_specific_options(self):
        """Product-specific wptrunner parameters needed to run tests."""
        return {
            **super().product_specific_options(),
            'binary': self.default_binary,
        }

    @property
    def default_binary(self):
        binary_path = 'content_shell'
        if self._host.platform.is_win():
            binary_path += '.exe'
        elif self._host.platform.is_mac():
            binary_path = self._host.filesystem.join('Content Shell.app',
                                                     'Contents', 'MacOS',
                                                     'Content Shell')
        return self._port.build_path(binary_path)


class ChromeiOS(Product):
    name = 'chrome_ios'

    def __init__(self, port, options):
        super().__init__(port, options)
        self.xcode_build_version = options.xcode_build_version

    def product_specific_options(self):
        """Product-specific wptrunner parameters needed to run tests."""
        return {'webdriver_binary': self.default_webdriver_binary}

    @property
    def default_webdriver_binary(self) -> str:
        return self._port._path_finder.path_from_chromium_base(
            'ios', 'chrome', 'test', 'wpt', 'tools',
            'run_cwt_chromedriver_wrapper.py')

    def additional_webdriver_args(self):
        # Set up xcode log output dir.
        output_dir = self._host.filesystem.join(
            self._port.artifacts_directory(), "xcode-output")
        return ['--out-dir=' + output_dir, '--os=16.0']

    @contextlib.contextmanager
    def test_env(self):
        path_finder.add_build_ios_to_sys_path()
        import xcode_util as xcode
        with super().test_env():
            # Install xcode.
            if self.xcode_build_version:
                try:
                    runtime_cache_folder = xcode.construct_runtime_cache_folder(
                        '../../Runtime-ios-', '16.0')
                    self._host.filesystem.maybe_make_directory(
                        runtime_cache_folder)
                    xcode.install('../../mac_toolchain',
                                  self._options.xcode_build_version,
                                  '../../Xcode.app',
                                  runtime_cache_folder=runtime_cache_folder,
                                  ios_version='16.0')
                    xcode.select('../../Xcode.app')
                except subprocess.CalledProcessError as e:
                    logging.error(
                        'Xcode build version %s failed to install: %s ',
                        self.xcode_build_version, e)
                else:
                    logging.info(
                        'Xcode build version %s successfully installed.',
                        self.xcode_build_version)
            else:
                logging.warning('Skip the Xcode installation, no '
                                '--xcode-build-version')
            yield


class ChromeAndroidBase(Product):
    def __init__(self, port, options):
        do_delay_imports()
        super().__init__(port, options)
        if options.browser_apk:
            self.browser_apk = options.browser_apk
        else:
            self.browser_apk = self.default_browser_apk
        self.adb_binary = devil_env.config.FetchPath('adb')  # pylint: disable=undefined-variable;
        self.devices = []

    @contextlib.contextmanager
    def _install_apk(self, device, path):
        """Helper context manager for ensuring a device uninstalls an APK."""
        device.Install(path)
        try:
            yield
        finally:
            device.Uninstall(path)

    @contextlib.contextmanager
    def get_devices(self):
        instances = []
        try:
            if self._options.avd_config:
                logging.info(
                    f'Installing emulator from {self._options.avd_config}')
                config = avd.AvdConfig(self._options.avd_config)  # pylint: disable=undefined-variable;
                config.Install()

                # use '--child-processes' to decide how many emulators to launch
                for _ in range(max(self._options.child_processes or 1, 1)):
                    instance = config.CreateInstance()
                    instances.append(instance)

                SyncParallelizer(instances).Start(  # pylint: disable=undefined-variable;
                    writable_system=True,
                    window=self._options.emulator_window,
                    require_fast_start=True)

            #TODO(weizhong): when choose device, make sure abi matches with target
            yield device_utils.DeviceUtils.HealthyDevices()  # pylint: disable=undefined-variable;
        finally:
            SyncParallelizer(instances).Stop()  # pylint: disable=undefined-variable;

    @contextlib.contextmanager
    def test_env(self):
        with super().test_env():
            devil_chromium.Initialize(adb_path=self.adb_binary)  # pylint: disable=undefined-variable;
            self.devices = self._tasks.enter_context(self.get_devices())
            if not self.devices:
                raise Exception('No devices attached to this host. '
                                "Make sure to provide '--avd-config' "
                                'if using only emulators.')

            if not self._options.no_install:
                self.provision_devices()
            yield

    def product_specific_options(self):
        return {
            'adb_binary': self.adb_binary,
            'device_serial': [device.serial for device in self.devices],
            'package_name': self.get_browser_package_name(),
            'browser_version': self.get_version(),
            'webdriver_binary': self.default_webdriver_binary
        }

    def get_version(self):
        version_provider = self.get_version_provider_package_name()
        if self.devices and version_provider:
            # Assume devices are identically provisioned, so select any.
            device = self.devices[0]
            try:
                version = device.GetApplicationVersion(version_provider)
                logging.info('Product version: %s %s (package: %r)', self.name,
                             version, version_provider)
                return version
            except CommandFailedError:  # pylint: disable=undefined-variable;
                logging.warning(
                    'Failed to retrieve version of %s (package: %r)',
                    self.name, version_provider)
        return None

    @property
    def default_webdriver_binary(self):
        return self._port.build_path('clang_x64', 'chromedriver')

    def get_browser_package_name(self):
        """Get the name of the package to run tests against.

        For WebView, this package is the shell.

        Returns:
            Optional[str]: The name of a package installed on the devices or
                `None` to use wpt's best guess of the runnable package.

        See Also:
            https://github.com/web-platform-tests/wpt/blob/merge_pr_33203/tools/wpt/browser.py#L867-L924
        """
        if self.browser_apk:
            # pylint: disable=undefined-variable;
            with contextlib.suppress(apk_helper.ApkHelperError):
                return apk_helper.GetPackageName(self.browser_apk)
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

    def provision_devices(self):
        """Provisions a set of Android devices in parallel."""
        contexts = [self._provision_device(device) for device in self.devices]
        self._tasks.enter_context(SyncParallelizer(contexts))  # pylint: disable=undefined-variable;

    @contextlib.contextmanager
    def _provision_device(self, device):
        """Provision a single Android device for a test.

        This method will be executed in parallel on all devices, so
        it is crucial that it is thread safe.
        """
        with contextlib.ExitStack() as exit_stack:
            exit_stack.enter_context(
                self._install_apk(device, self.browser_apk))
            logging.info('Provisioned device (serial: %s)', device.serial)
            yield


class WebView(ChromeAndroidBase):
    name = 'android_webview'
    aliases = ['webview']

    def __init__(self, port, options):
        super().__init__(port, options)
        if options.webview_provider:
            self.webview_provider = options.webview_provider
        else:
            self.webview_provider = self.default_webview_provider

    @property
    def default_browser_apk(self):
        return self._port.build_path('apks', 'SystemWebViewShell.apk')

    @property
    def default_webview_provider(self):
        return self._port.build_path('apks', 'SystemWebView.apk')

    def _install_webview(self, device):
        # Prioritize local builds.
        # pylint: disable=undefined-variable;
        return webview_app.UseWebViewProvider(device, self.webview_provider)

    def get_browser_package_name(self):
        return (super().get_browser_package_name()
                or 'org.chromium.webview_shell')

    def get_version_provider_package_name(self):
        # Use the version from the webview provider, not the shell, since the
        # provider is distributed to end users. The shell is developer-facing,
        # so its version is usually not actively updated.
        # pylint: disable=undefined-variable;
        with contextlib.suppress(apk_helper.ApkHelperError):
            return apk_helper.GetPackageName(self.webview_provider)

    @contextlib.contextmanager
    def _provision_device(self, device):
        with self._install_webview(device), super()._provision_device(device):
            yield


class ChromeAndroid(ChromeAndroidBase):
    name = 'chrome_android'
    aliases = ['clank']

    @property
    def default_browser_apk(self):
        return self._port.build_path('apks', 'ChromePublic.apk')

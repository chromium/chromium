# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import itertools
import logging
import os
import posixpath
import re
import sys
import threading
import time

from blinkpy.common import exit_codes
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.path_finder import WEB_TESTS_LAST_COMPONENT
from blinkpy.common.path_finder import get_chromium_src_dir
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.profiler import SingleFileOutputProfiler
from blinkpy.web_tests.breakpad.dump_reader_multipart import DumpReaderAndroid
from blinkpy.web_tests.models import test_run_results
from blinkpy.web_tests.port import base
from blinkpy.web_tests.port import linux
from blinkpy.web_tests.port import driver
from blinkpy.web_tests.port import factory
from blinkpy.web_tests.port import server_process


# These are stub globals used for android-specific modules. We
# don't import them unless we actually need a real Android port object,
# in order to not have the dependency on all of the android and catapult
# modules in non-Android ports.
# pylint: disable=invalid-name
battery_utils = None
device_errors = None
device_utils = None
devil_chromium = None
devil_env = None
intent = None
perf_control = None
# pylint: enable=invalid-name


def _import_android_packages_if_necessary():
    # pylint: disable=invalid-name
    global battery_utils
    global device_errors
    global device_utils
    global devil_chromium
    global devil_env
    global intent
    global perf_control
    # pylint: enable=invalid-name

    if not battery_utils:
        chromium_src_root = get_chromium_src_dir()
        devil_root = os.path.join(chromium_src_root, 'third_party', 'catapult',
                                  'devil')
        build_android_root = os.path.join(chromium_src_root, 'build', 'android')
        sys.path.insert(0, devil_root)
        sys.path.insert(0, build_android_root)
        from importlib import import_module

        battery_utils = import_module('devil.android.battery_utils')
        devil_env = import_module('devil.devil_env')
        device_errors = import_module('devil.android.device_errors')
        device_utils = import_module('devil.android.device_utils')
        devil_chromium = import_module('devil_chromium')
        intent = import_module('devil.android.sdk.intent')
        perf_control = import_module('devil.android.perf.perf_control')


_log = logging.getLogger(__name__)

# The root directory for test resources, which has the same structure as the
# source root directory of Chromium.
# This path is defined in Chromium's base/test/test_support_android.cc.
DEVICE_SOURCE_ROOT_DIR = '/data/local/tmp/'

# The web tests directory on device, which has two usages:
# 1. as a virtual path in file urls that will be bridged to HTTP.
# 2. pointing to some files that are pushed to the device for tests that
# don't work on file-over-http (e.g. blob protocol tests).
DEVICE_WEB_TESTS_DIR = DEVICE_SOURCE_ROOT_DIR + RELATIVE_WEB_TESTS

KPTR_RESTRICT_PATH = '/proc/sys/kernel/kptr_restrict'

# All the test cases are still served to the test runner through file protocol,
# but we use a file-to-http feature to bridge the file request to host's http
# server to get the real test files and corresponding resources.
# See webkit/support/platform_support_android.cc for the other side of this bridge.
# WEB_TEST_PATH_PREFIX should be matched to the local directory name of
# web_tests because some tests and test_runner find test root directory
# with it.
PERF_TEST_PATH_PREFIX = '/PerformanceTests'
WEB_TESTS_PATH_PREFIX = '/' + WEB_TESTS_LAST_COMPONENT

# We start netcat processes for each of the three stdio streams. In doing so,
# we attempt to use ports starting from 10201. This starting value is
# completely arbitrary.
FIRST_NETCAT_PORT = 10201

# Timeout in seconds to wait for starting/stopping the driver.
DRIVER_START_STOP_TIMEOUT_SECS = 10

# Test resources that need to be accessed as files directly.
# Each item can be the relative path of a directory or a file.
TEST_RESOURCES_TO_PUSH = [
    # Blob tests need to access files directly.
    'editing/pasteboard/resources',
    'fast/files/resources',
    'http/tests/local/resources',
    'http/tests/local/formdata/resources',
    # User style URLs are accessed as local files in webkit_support.
    'http/tests/security/resources/cssStyle.css',
    # Media tests need to access audio/video as files.
    'media/content',
    'compositing/resources/video.mp4',
]


# Information required when running web tests using content_shell as the test runner.
class ContentShellDriverDetails():

    def device_cache_directory(self):
        return self.device_directory() + 'cache/'

    def device_fonts_directory(self):
        return self.device_directory() + 'fonts/'

    def device_fifo_directory(self):
        return '/data/data/' + self.package_name() + '/files/'

    def apk_name(self):
        return 'apks/ContentShell.apk'

    def package_name(self):
        return 'org.chromium.content_shell_apk'

    def activity_name(self):
        return self.package_name() + '/.ContentShellActivity'

    def library_name(self):
        return 'libcontent_shell_content_view.so'

    def command_line_file(self):
        return '/data/local/tmp/content-shell-command-line'

    def device_crash_dumps_directory(self):
        return '/data/local/tmp/content-shell-crash-dumps'

    def additional_command_line_flags(self, use_breakpad):
        flags = ['--encode-binary']
        if use_breakpad:
            flags.extend(['--enable-crash-reporter', '--crash-dumps-dir=%s' % self.device_crash_dumps_directory()])
        return flags

    def device_directory(self):
        return DEVICE_SOURCE_ROOT_DIR + 'content_shell/'


# A class to encapsulate device status and information, such as the DeviceUtils
# instances and whether the device has been set up.
class AndroidDevices(object):
    # Percentage of battery a device needs to have in order for it to be considered
    # to participate in running the web tests.
    MINIMUM_BATTERY_PERCENTAGE = 30

    def __init__(self, default_devices=None, debug_logging=False):
        self._usable_devices = []
        self._default_devices = default_devices
        self._prepared_devices = []
        self._debug_logging = debug_logging

    def prepared_devices(self):
        return self._prepared_devices

    def usable_devices(self, executive):
        if self._usable_devices:
            return self._usable_devices

        if self._default_devices:
            self._usable_devices = [
                device_utils.DeviceUtils(d)
                for d in self._default_devices]
            return self._usable_devices

        devices = device_utils.DeviceUtils.HealthyDevices()
        self._usable_devices = [
            d for d in devices
            if (battery_utils.BatteryUtils(d).GetBatteryInfo().get('level', 0)
                >= AndroidDevices.MINIMUM_BATTERY_PERCENTAGE
                and d.IsScreenOn())]

        return self._usable_devices

    def get_device(self, executive, device_index):
        devices = self.usable_devices(executive)
        if device_index >= len(devices):
            raise AssertionError('Device index exceeds number of usable devices.')

        return devices[device_index]

    def is_device_prepared(self, device_serial):
        return device_serial in self._prepared_devices

    def set_device_prepared(self, device_serial):
        self._prepared_devices.append(device_serial)


class AndroidPort(base.Port):
    port_name = 'android'

    # Avoid initializing the adb path [worker count]+1 times by storing it as a static member.
    _adb_path = None

    SUPPORTED_VERSIONS = ('android')

    FALLBACK_PATHS = {'kitkat': ['android'] + linux.LinuxPort.latest_platform_fallback_path()}

    BUILD_REQUIREMENTS_URL = 'https://www.chromium.org/developers/how-tos/android-build-instructions'

    def __init__(self, host, port_name, **kwargs):
        _import_android_packages_if_necessary()
        super(AndroidPort, self).__init__(host, port_name, **kwargs)

        self._operating_system = 'android'
        self._version = 'kitkat'

        self._host_port = factory.PortFactory(host).get(**kwargs)
        self.server_process_constructor = self._android_server_process_constructor

        if not self.get_option('disable_breakpad'):
            self._dump_reader = DumpReaderAndroid(host, self._build_path())

        if self.driver_name() != self.CONTENT_SHELL_NAME:
            raise AssertionError('Web tests on Android only support content_shell as the driver.')

        self._driver_details = ContentShellDriverDetails()

        # Initialize the AndroidDevices class which tracks available devices.
        default_devices = None
        if hasattr(self._options, 'adb_devices') and len(self._options.adb_devices):
            default_devices = self._options.adb_devices

        self._debug_logging = self.get_option('android_logging')
        self._devices = AndroidDevices(default_devices, self._debug_logging)

        devil_chromium.Initialize(
            output_directory=self._build_path(),
            adb_path=self._path_from_chromium_base(
                'third_party', 'android_sdk', 'public', 'platform-tools', 'adb'))
        devil_env.config.InitializeLogging(
            logging.DEBUG
            if self._debug_logging and self.get_option('debug_rwt_logging')
            else logging.WARNING)

        prepared_devices = self.get_option('prepared_devices', [])
        for serial in prepared_devices:
            self._devices.set_device_prepared(serial)

    def default_smoke_test_only(self):
        return True

    def additional_driver_flags(self):
        return super(AndroidPort, self).additional_driver_flags() + \
            self._driver_details.additional_command_line_flags(use_breakpad=not self.get_option('disable_breakpad'))

    def default_timeout_ms(self):
        # Android platform has less computing power than desktop platforms.
        # Using 10 seconds allows us to pass most slow tests which are not
        # marked as slow tests on desktop platforms.
        return 10 * 1000

    def driver_stop_timeout(self):
        # The driver doesn't respond to closing stdin, so we might as well stop the driver immediately.
        return 0.0

    def default_child_processes(self):
        usable_devices = self._devices.usable_devices(self._executive)
        if not usable_devices:
            raise test_run_results.TestRunException(exit_codes.NO_DEVICES_EXIT_STATUS,
                                                    'Unable to find any attached Android devices.')
        return len(usable_devices)

    def check_build(self, needs_http, printer):
        exit_status = super(AndroidPort, self).check_build(needs_http, printer)
        if exit_status:
            return exit_status

        return self._check_devices(printer)

    def _check_devices(self, printer):

        # Push the executables and other files to the devices; doing this now
        # means we can do this in parallel in the manager process and not mix
        # this in with starting and stopping workers.

        lock = threading.Lock()

        def log_safely(msg, throttled=True):
            if throttled:
                callback = printer.write_throttled_update
            else:
                callback = printer.write_update
            with lock:
                callback('%s' % msg)

        def _setup_device_impl(device):
            log_safely('preparing device', throttled=False)

            if self._devices.is_device_prepared(device.serial):
                return

            device.EnableRoot()
            perf_control.PerfControl(device).SetPerfProfilingMode()

            # Required by webkit_support::GetWebKitRootDirFilePath().
            # Other directories will be created automatically by adb push.
            device.RunShellCommand(
                ['mkdir', '-p', DEVICE_SOURCE_ROOT_DIR + 'chrome'],
                check_return=True)

            # Allow the test driver to get full read and write access to the directory on the device,
            # as well as for the FIFOs. We'll need a world writable directory.
            device.RunShellCommand(
                ['mkdir', '-p', self._driver_details.device_directory()],
                check_return=True)

            # Make sure that the disk cache on the device resets to a clean state.
            device.RunShellCommand(
                ['rm', '-rf', self._driver_details.device_cache_directory()],
                check_return=True)

            device.EnableRoot()
            perf_control.PerfControl(device).SetPerfProfilingMode()

            # Required by webkit_support::GetWebKitRootDirFilePath().
            # Other directories will be created automatically by adb push.
            device.RunShellCommand(
                ['mkdir', '-p', DEVICE_SOURCE_ROOT_DIR + 'chrome'],
                check_return=True)

            # Allow the test driver to get full read and write access to the directory on the device,
            # as well as for the FIFOs. We'll need a world writable directory.
            device.RunShellCommand(
                ['mkdir', '-p', self._driver_details.device_directory()],
                check_return=True)

            # Make sure that the disk cache on the device resets to a clean state.
            device.RunShellCommand(
                ['rm', '-rf', self._driver_details.device_cache_directory()],
                check_return=True)

            device_path = lambda *p: posixpath.join(
                self._driver_details.device_directory(), *p)

            device.Install(self._path_to_driver())

            # Build up a list of what we want to push, including:
            host_device_tuples = []

            # - the custom font files
            # TODO(sergeyu): Rename these files, they can be used on platforms
            # other than Android.
            host_device_tuples.append(
                (self._build_path('test_fonts/android_main_fonts.xml'),
                 device_path('android_main_fonts.xml')))
            host_device_tuples.append(
                (self._build_path('test_fonts/android_fallback_fonts.xml'),
                 device_path('android_fallback_fonts.xml')))
            for font_file in self._get_font_files():
                host_device_tuples.append(
                    (font_file, device_path('fonts', os.path.basename(font_file))))

            # - the test resources
            host_device_tuples.extend(
                (self.host.filesystem.join(self.web_tests_dir(), resource),
                 posixpath.join(DEVICE_WEB_TESTS_DIR, resource))
                for resource in TEST_RESOURCES_TO_PUSH)

            # ... and then push them to the device.
            device.PushChangedFiles(host_device_tuples)

            device.RunShellCommand(
                ['mkdir', '-p', self._driver_details.device_fifo_directory()],
                check_return=True)

            device.RunShellCommand(
                ['chmod', '-R', '777', self._driver_details.device_directory()],
                check_return=True)
            device.RunShellCommand(
                ['chmod', '-R', '777', self._driver_details.device_fifo_directory()],
                check_return=True)

            # Mark this device as having been set up.
            self._devices.set_device_prepared(device.serial)

            log_safely('device prepared', throttled=False)

        def setup_device(device):
            try:
                _setup_device_impl(device)
            except (ScriptError,
                    driver.DeviceFailure,
                    device_errors.CommandFailedError,
                    device_errors.CommandTimeoutError,
                    device_errors.DeviceUnreachableError) as error:
                with lock:
                    _log.warning('[%s] failed to prepare_device: %s', device.serial, error)

        devices = self._devices.usable_devices(self.host.executive)
        device_utils.DeviceUtils.parallel(devices).pMap(setup_device)

        if not self._devices.prepared_devices():
            _log.error('Could not prepare any devices for testing.')
            return exit_codes.NO_DEVICES_EXIT_STATUS
        return exit_codes.OK_EXIT_STATUS

    def setup_test_run(self):
        super(AndroidPort, self).setup_test_run()

        # By setting this on the options object, we can propagate the list
        # of prepared devices to the workers (it is read in __init__()).
        if self._devices._prepared_devices:
            self._options.prepared_devices = self._devices.prepared_devices()
        else:
            # We were called with --no-build, so assume the devices are up to date.
            self._options.prepared_devices = [d.get_serial() for d in self._devices.usable_devices(self.host.executive)]

    def num_workers(self, requested_num_workers):
        return min(len(self._options.prepared_devices), requested_num_workers)

    def check_sys_deps(self):
        # _get_font_files() will throw if any of the required fonts is missing.
        self._get_font_files()
        return exit_codes.OK_EXIT_STATUS

    def requires_http_server(self):
        """Chromium Android runs tests on devices, and uses the HTTP server to
        serve the actual web tests to the test driver.
        """
        return True

    def start_http_server(self, additional_dirs, number_of_drivers):
        additional_dirs[PERF_TEST_PATH_PREFIX] = self._perf_tests_dir()
        additional_dirs[WEB_TESTS_PATH_PREFIX] = self.web_tests_dir()
        super(AndroidPort, self).start_http_server(additional_dirs, number_of_drivers)

    def create_driver(self, worker_number, no_timeout=False):
        return ChromiumAndroidDriver(self, worker_number,
                                     driver_details=self._driver_details,
                                     android_devices=self._devices,
                                     # Force no timeout to avoid test driver timeouts before NRWT.
                                     no_timeout=True)

    def driver_cmd_line(self):
        # Override to return the actual test driver's command line.
        return self.create_driver(0)._android_driver_cmd_line([])

    def clobber_old_port_specific_results(self):
        if not self.get_option('disable_breakpad'):
            self._dump_reader.clobber_old_results()

    # Overridden protected methods.

    def _build_path(self, *comps):
        return self._host_port._build_path(*comps)

    def _build_path_with_target(self, target, *comps):
        return self._host_port._build_path_with_target(target, *comps)

    def path_to_apache(self):
        return self._host_port.path_to_apache()

    def path_to_apache_config_file(self):
        return self._host_port.path_to_apache_config_file()

    def _path_to_driver(self, target=None):
        return self._build_path_with_target(target, self._driver_details.apk_name())

    def _path_to_image_diff(self):
        return self._host_port._path_to_image_diff()

    def _shut_down_http_server(self, pid):
        return self._host_port._shut_down_http_server(pid)

    def _driver_class(self):
        return ChromiumAndroidDriver

    # Local private methods.

    @staticmethod
    def _android_server_process_constructor(port, server_name, cmd_line, env=None, more_logging=False):
        return server_process.ServerProcess(port, server_name, cmd_line, env,
                                            treat_no_data_as_crash=True, more_logging=more_logging)


class AndroidPerf(SingleFileOutputProfiler):
    _cached_perf_host_path = None
    _have_searched_for_perf_host = False

    def __init__(self, host, executable_path, output_dir, device, symfs_path, kallsyms_path, identifier=None):
        super(AndroidPerf, self).__init__(host, executable_path, output_dir, 'data', identifier)
        self._device = device
        self._perf_process = None
        self._symfs_path = symfs_path
        self._kallsyms_path = kallsyms_path

    def check_configuration(self):
        # Check that perf is installed
        if not self._device.PathExists('/system/bin/perf'):
            _log.error('Cannot find /system/bin/perf on device %s', self._device.serial)
            return False

        # Check that the device is a userdebug build (or at least has the necessary libraries).
        if self._device.build_type != 'userdebug':
            _log.error('Device %s is not flashed with a userdebug build of Android', self._device.serial)
            return False

        # FIXME: Check that the binary actually is perf-able (has stackframe pointers)?
        # objdump -s a function and make sure it modifies the fp?
        # Instruct users to rebuild after export GYP_DEFINES="profiling=1 $GYP_DEFINES"
        return True

    def print_setup_instructions(self):
        _log.error("""
perf on android requires a 'userdebug' build of Android, see:
http://source.android.com/source/building-devices.html"

The perf command can be built from:
https://android.googlesource.com/platform/external/linux-tools-perf/
and requires libefl, libebl, libdw, and libdwfl available in:
https://android.googlesource.com/platform/external/elfutils/

The test driver must be built with profiling=1, make sure you've done:
export GYP_DEFINES="profiling=1 $GYP_DEFINES"
update-webkit --chromium-android
build-webkit --chromium-android

Googlers should read:
http://goto.google.com/cr-android-perf-howto
""")

    def attach_to_pid(self, pid):
        assert pid
        assert self._perf_process is None
        # FIXME: This can't be a fixed timeout!
        cmd = [self._device.adb.GetAdbPath(), '-s', self._device.serial,
               'shell', 'perf', 'record', '-g', '-p', pid, 'sleep', 30]
        self._perf_process = self._host.executive.popen(cmd)

    def _perf_version_string(self, perf_path):
        try:
            return self._host.executive.run_command([perf_path, '--version'])
        except:
            return None

    def _find_perfhost_binary(self):
        perfhost_version = self._perf_version_string('perfhost_linux')
        if perfhost_version:
            return 'perfhost_linux'
        perf_version = self._perf_version_string('perf')
        if perf_version:
            return 'perf'
        return None

    def _perfhost_path(self):
        if self._have_searched_for_perf_host:
            return self._cached_perf_host_path
        self._have_searched_for_perf_host = True
        self._cached_perf_host_path = self._find_perfhost_binary()
        return self._cached_perf_host_path

    def _first_ten_lines_of_profile(self, perf_output):
        match = re.search(r"^#[^\n]*\n((?: [^\n]*\n){1,10})", perf_output, re.MULTILINE)
        return match.group(1) if match else None

    def profile_after_exit(self):
        perf_exitcode = self._perf_process.wait()
        if perf_exitcode != 0:
            _log.debug("Perf failed (exit code: %i), can't process results.", perf_exitcode)
            return

        self._device.PullFile('/data/perf.data', self._output_path)

        perfhost_path = self._perfhost_path()
        perfhost_report_command = [
            'report',
            '--input', self._output_path,
            '--symfs', self._symfs_path,
            '--kallsyms', self._kallsyms_path,
        ]
        if perfhost_path:
            perfhost_args = [perfhost_path] + perfhost_report_command + ['--call-graph', 'none']
            perf_output = self._host.executive.run_command(perfhost_args)
            # We could save off the full -g report to a file if users found that useful.
            _log.debug(self._first_ten_lines_of_profile(perf_output))
        else:
            _log.debug("""
Failed to find perfhost_linux binary, can't process samples from the device.

perfhost_linux can be built from:
https://android.googlesource.com/platform/external/linux-tools-perf/
also, modern versions of perf (available from apt-get install goobuntu-kernel-tools-common)
may also be able to process the perf.data files from the device.

Googlers should read:
http://goto.google.com/cr-android-perf-howto
for instructions on installing pre-built copies of perfhost_linux
http://crbug.com/165250 discusses making these pre-built binaries externally available.
""")

        perfhost_display_patch = perfhost_path if perfhost_path else 'perfhost_linux'
        _log.debug('To view the full profile, run:')
        _log.debug(' '.join([perfhost_display_patch] + perfhost_report_command))


class ChromiumAndroidDriver(driver.Driver):

    def __init__(self, port, worker_number, driver_details, android_devices, no_timeout=False):
        super(ChromiumAndroidDriver, self).__init__(port, worker_number, no_timeout)
        self._write_stdin_process = None
        self._read_stdout_process = None
        self._read_stderr_process = None
        self._original_kptr_restrict = None

        self._android_devices = android_devices
        self._device = android_devices.get_device(port._executive, worker_number)  # pylint: disable=protected-access
        self._driver_details = driver_details
        self._debug_logging = self._port._debug_logging
        self._created_cmd_line = False
        self._device_failed = False

        # FIXME: If we taught ProfileFactory about "target" devices we could
        # just use the logic in Driver instead of duplicating it here.
        if self._port.get_option('profile'):
            # FIXME: This should be done once, instead of per-driver!
            symfs_path = self._find_or_create_symfs()
            kallsyms_path = self._update_kallsyms_cache(symfs_path)
            # FIXME: We should pass this some sort of "Bridge" object abstraction around ADB instead of a path/device pair.
            self._profiler = AndroidPerf(self._port.host, self._port._path_to_driver(), self._port.results_directory(),
                                         self._device, symfs_path, kallsyms_path)
            # FIXME: This is a layering violation and should be moved to Port.check_sys_deps
            # once we have an abstraction around an adb_path/device_serial pair to make it
            # easy to make these class methods on AndroidPerf.
            if not self._profiler.check_configuration():
                self._profiler.print_setup_instructions()
                sys.exit(1)
        else:
            self._profiler = None

    def __del__(self):
        self._teardown_performance()
        self._clean_up_cmd_line()
        super(ChromiumAndroidDriver, self).__del__()

    def _update_kallsyms_cache(self, output_dir):
        kallsyms_name = '%s-kallsyms' % self._device.serial
        kallsyms_cache_path = self._port.host.filesystem.join(output_dir, kallsyms_name)

        self._device.EnableRoot()

        saved_kptr_restrict = self._device.ReadFile(KPTR_RESTRICT_PATH).strip()
        self._device.WriteFile(KPTR_RESTRICT_PATH, '0')

        _log.debug('Updating kallsyms file (%s) from device', kallsyms_cache_path)
        self._device.PullFile('/proc/kallsysm', kallsyms_cache_path)
        self._device.WriteFile(KPTR_RESTRICT_PATH, saved_kptr_restrict)

        return kallsyms_cache_path

    def _find_or_create_symfs(self):
        env = self._port.host.environ.copy()
        fs = self._port.host.filesystem

        if 'ANDROID_SYMFS' in env:
            symfs_path = env['ANDROID_SYMFS']
        else:
            symfs_path = fs.join(self._port.results_directory(), 'symfs')
            _log.debug('ANDROID_SYMFS not set, using %s', symfs_path)

        # find the installed path, and the path of the symboled built library
        # FIXME: We should get the install path from the device!
        symfs_library_path = fs.join(symfs_path, 'data/app-lib/%s-1/%s' %
                                     (self._driver_details.package_name(), self._driver_details.library_name()))
        built_library_path = self._port._build_path('lib', self._driver_details.library_name())
        assert fs.exists(built_library_path)

        # FIXME: Ideally we'd check the sha1's first and make a soft-link instead
        # of copying (since we probably never care about windows).
        _log.debug('Updating symfs library (%s) from built copy (%s)', symfs_library_path, built_library_path)
        fs.maybe_make_directory(fs.dirname(symfs_library_path))
        fs.copyfile(built_library_path, symfs_library_path)

        return symfs_path

    def _log_error(self, message):
        _log.error('[%s] %s', self._device.serial, message)

    def _log_warning(self, message):
        _log.warning('[%s] %s', self._device.serial, message)

    def _log_debug(self, message):
        if self._debug_logging:
            _log.debug('[%s] %s', self._device.serial, message)

    def _abort(self, message):
        self._device_failed = True
        raise driver.DeviceFailure('[%s] %s' % (self._device.serial, message))

    def _get_last_stacktrace(self):
        try:
            tombstones = self._device.RunShellCommand(
                'ls -n /data/tombstones/tombstone_*',
                check_return=True, shell=True)
        except device_errors.CommandFailedError as exc:
            # FIXME: crbug.com/321489 ... figure out why we sometimes get
            #   permission denied.
            self._log_error('The driver crashed, but we were unable to read a tombstone: %s' % str(exc))
            return ''

        last_tombstone = None
        for tombstone in tombstones:
            # Format of fields:
            # 0          1      2      3     4          5     6
            # permission uid    gid    size  date       time  filename
            # -rw------- 1000   1000   45859 2011-04-13 06:00 tombstone_00
            fields = tombstone.split()
            if len(fields) != 7:
                self._log_warning("unexpected line in tombstone output, skipping: '%s'" % tombstone)
                continue

            if not last_tombstone or fields[4] + fields[5] >= last_tombstone[4] + last_tombstone[5]:
                last_tombstone = fields
            else:
                break

        if not last_tombstone:
            self._log_error('The driver crashed, but we could not find any valid tombstone!')
            return ''

        # Use Android tool vendor/google/tools/stack to convert the raw
        # stack trace into a human readable format, if needed.
        # It takes a long time, so don't do it here.
        tombstone_contents = self._device.ReadFile(
            '/data/tombstones/%s' % last_tombstone[6])
        return '%s\n%s' % (' '.join(last_tombstone), tombstone_contents)

    def _get_logcat(self):
        return '\n'.join(self._device.adb.Logcat(dump=True, logcat_format='threadtime'))

    def _teardown_performance(self):
        perf_control.PerfControl(self._device).SetDefaultPerfMode()

    def _get_crash_log(self, stdout, stderr, newer_than):
        if not stdout:
            stdout = ''
        stdout += '********* [%s] Logcat:\n%s' % (self._device.serial, self._get_logcat())
        if not stderr:
            stderr = ''
        stderr += '********* [%s] Tombstone file:\n%s' % (self._device.serial, self._get_last_stacktrace())

        if not self._port.get_option('disable_breakpad'):
            crashes = self._pull_crash_dumps_from_device()
            for crash in crashes:
                stack = self._port._dump_reader._get_stack_from_dump(crash)  # pylint: disable=protected-access
                try:
                  stack_str = stack.encode('ascii', 'replace')
                except Exception as e:
                  stack_str = '<No Stack> (%s)' % e
                stderr += '********* [%s] breakpad minidump %s:\n%s' % (
                    self._port.host.filesystem.basename(crash),
                    self._device.serial,
                    stack_str)

        return super(ChromiumAndroidDriver, self)._get_crash_log(
            stdout, stderr, newer_than)

    def cmd_line(self, per_test_args):
        # The returned command line is used to start _server_process. In our case, it's an interactive 'adb shell'.
        # The command line passed to the driver process is returned by _driver_cmd_line() instead.
        return [self._device.adb.GetAdbPath(), '-s', self._device.serial, 'shell']

    def _android_driver_cmd_line(self, per_test_args):
        return driver.Driver.cmd_line(self, per_test_args)

    @staticmethod
    def _loop_with_timeout(condition, timeout_secs):
        deadline = time.time() + timeout_secs
        while time.time() < deadline:
            if condition():
                return True
        return False

    def start(self, per_test_args, deadline):
        # We override the default start() so that we can call _android_driver_cmd_line()
        # instead of cmd_line().
        new_cmd_line = self._android_driver_cmd_line(per_test_args)

        # Since _android_driver_cmd_line() is different than cmd_line() we need to provide
        # our own mechanism for detecting when the process should be stopped.
        if self._current_cmd_line is None:
            self._current_android_cmd_line = None
        if new_cmd_line != self._current_android_cmd_line:
            self.stop()
        self._current_android_cmd_line = new_cmd_line

        super(ChromiumAndroidDriver, self).start(per_test_args, deadline)

    def _start(self, per_test_args):
        if not self._android_devices.is_device_prepared(self._device.serial):
            raise driver.DeviceFailure('%s is not prepared in _start()' % self._device.serial)

        for retries in range(3):
            try:
                if self._start_once(per_test_args):
                    return
            except ScriptError as error:
                self._abort('ScriptError("%s") in _start()' % error)

            self._log_error('Failed to start the content_shell application. Retries=%d. Log:\n%s' % (retries, self._get_logcat()))
            self.stop()
            time.sleep(2)
        self._abort('Failed to start the content_shell application multiple times. Giving up.')

    def _start_once(self, per_test_args):
        super(ChromiumAndroidDriver, self)._start(per_test_args, wait_for_ready=False)

        self._device.adb.Logcat(clear=True)

        self._create_device_crash_dumps_directory()

        # Read back the shell prompt to ensure adb shell is ready.
        deadline = time.time() + DRIVER_START_STOP_TIMEOUT_SECS
        self._server_process.start()
        self._read_prompt(deadline)
        self._log_debug('Interactive shell started')

        # Start a netcat process to which the test driver will connect to write stdout.
        self._read_stdout_process, stdout_port = self._start_netcat(
            'ReadStdout', read_from_stdin=False)
        self._log_debug('Redirecting stdout to port %d' % stdout_port)

        # Start a netcat process to which the test driver will connect to write stderr.
        self._read_stderr_process, stderr_port = self._start_netcat(
            'ReadStderr', first_port=stdout_port + 1, read_from_stdin=False)
        self._log_debug('Redirecting stderr to port %d' % stderr_port)

        # Start a netcat process to which the test driver will connect to read stdin.
        self._write_stdin_process, stdin_port = self._start_netcat(
            'WriteStdin', first_port=stderr_port + 1)
        self._log_debug('Redirecting stdin to port %d' % stdin_port)

        # Combine the stdin, stdout, and stderr pipes into self._server_process.
        self._replace_server_process_streams()

        # We delay importing forwarder as long as possible because it uses fcntl,
        # which isn't available on windows.
        from devil.android import forwarder

        self._log_debug('Starting forwarder')
        forwarder.Forwarder.Map(
            [(p, p) for p in base.Port.SERVER_PORTS],
            self._device)
        forwarder.Forwarder.Map(
            [(forwarder.DYNAMIC_DEVICE_PORT, p)
             for p in (stdout_port, stderr_port, stdin_port)],
            self._device)

        cmd_line_file_path = self._driver_details.command_line_file()
        original_cmd_line_file_path = cmd_line_file_path + '.orig'
        if (self._device.PathExists(cmd_line_file_path)
                and not self._device.PathExists(original_cmd_line_file_path)):
            # We check for both the normal path and the backup because we do not want to step
            # on the backup. Otherwise, we'd clobber the backup whenever we changed the
            # command line during the run.
            self._device.RunShellCommand(
                ['mv', cmd_line_file_path, original_cmd_line_file_path],
                check_return=True)

        stream_port_args = [
            '--android-stderr-port=%s' % forwarder.Forwarder.DevicePortForHostPort(stderr_port),
            '--android-stdin-port=%s' % forwarder.Forwarder.DevicePortForHostPort(stdin_port),
            '--android-stdout-port=%s' % forwarder.Forwarder.DevicePortForHostPort(stdout_port),
        ]
        cmd_line_contents = self._android_driver_cmd_line(per_test_args + stream_port_args)
        self._device.WriteFile(
            self._driver_details.command_line_file(),
            ' '.join(cmd_line_contents))
        self._log_debug('Command-line file contents: %s' % ' '.join(cmd_line_contents))
        self._created_cmd_line = True

        try:
            self._device.StartActivity(
                intent.Intent(
                    component=self._driver_details.activity_name(),
                    extras={'RunInSubThread': None}))
        except device_errors.CommandFailedError as exc:
            self._log_error('Failed to start the content_shell application. Exception:\n' + str(exc))
            return False

        # The test driver might crash during startup.
        if not self._wait_for_server_process_output(self._server_process, deadline, '#READY'):
            return False

        self._log_debug('content_shell is ready')
        return True

    def _create_device_crash_dumps_directory(self):
        self._device.RunShellCommand(
            ['rm', '-rf', self._driver_details.device_crash_dumps_directory()],
            check_return=True)
        self._device.RunShellCommand(
            ['mkdir', self._driver_details.device_crash_dumps_directory()],
            check_return=True)
        self._device.RunShellCommand(
            ['chmod', '-R', '777', self._driver_details.device_crash_dumps_directory()],
            check_return=True)

    def _start_netcat(self, server_name, first_port=FIRST_NETCAT_PORT, read_from_stdin=True):
        for i in itertools.count(first_port, 65536):
            nc_cmd = ['nc', '-l', str(i)]
            if not read_from_stdin:
                nc_cmd.append('-d')
            proc = self._port.server_process_constructor(self._port, server_name, nc_cmd)
            proc.start()
            self._port.host.executive.wait_limited(proc.pid(), limit_in_seconds=1)
            if self._port.host.executive.check_running_pid(proc.pid()):
                return (proc, i)

        raise Exception(
            'Unable to find a port for netcat process %s' % server_name)

    def _replace_server_process_streams(self):
        # pylint: disable=protected-access
        self._server_process.replace_input(
            self._write_stdin_process._proc.stdin)
        self._server_process.replace_outputs(
            self._read_stdout_process._proc.stdout,
            self._read_stderr_process._proc.stdout)

    def _pid_on_target(self):
        pids = self._device.GetPids(self._driver_details.package_name())
        return pids.get(self._driver_details.package_name())

    def stop(self):
        if not self._device_failed:
            # Do not try to stop the application if there's something wrong with the device; adb may hang.
            # FIXME: crbug.com/305040. Figure out if it's really hanging (and why).
            self._device.ForceStop(self._driver_details.package_name())

        if self._write_stdin_process:
            self._write_stdin_process.kill()
            self._write_stdin_process = None

        if self._read_stdout_process:
            self._read_stdout_process.kill()
            self._read_stdout_process = None

        if self._read_stderr_process:
            self._read_stderr_process.kill()
            self._read_stderr_process = None

        # We delay importing forwarder as long as possible because it uses fcntl,
        # which isn't available on windows.
        from devil.android import forwarder

        forwarder.Forwarder.KillDevice(self._device)
        forwarder.Forwarder.KillHost()

        super(ChromiumAndroidDriver, self).stop()

        self._clean_up_cmd_line()

    def _pull_crash_dumps_from_device(self):
        result = []
        if not self._device.PathExists(self._driver_details.device_crash_dumps_directory()):
            return result
        dumps = self._device.ListDirectory(
            self._driver_details.device_crash_dumps_directory())
        for dump in dumps:
            device_dump = '%s/%s' % (self._driver_details.device_crash_dumps_directory(), dump)
            local_dump = self._port.host.filesystem.join(
                self._port._dump_reader.crash_dumps_directory(), dump)  # pylint: disable=protected-access

            # FIXME: crbug.com/321489. Figure out why these commands would fail ...
            try:
                self._device.RunShellCommand(
                    ['chmod', '777', device_dump], check_return=True)
                self._device.PullFile(device_dump, local_dump)
                self._device.RunShellCommand(
                    ['rm', '-f', device_dump], check_return=True)
            except device_errors.CommandFailedError:
                pass

            if self._port.host.filesystem.exists(local_dump):
                result.append(local_dump)
        return result

    def _clean_up_cmd_line(self):
        if not self._created_cmd_line:
            return

        cmd_line_file_path = self._driver_details.command_line_file()
        original_cmd_line_file_path = cmd_line_file_path + '.orig'
        if self._device.PathExists(original_cmd_line_file_path):
            self._device.RunShellCommand(
                ['mv', original_cmd_line_file_path, cmd_line_file_path],
                check_return=True)
        elif self._device.PathExists(cmd_line_file_path):
            self._device.RunShellCommand(
                ['rm', cmd_line_file_path],
                check_return=True)
        self._created_cmd_line = False

    def _command_from_driver_input(self, driver_input):
        command = super(ChromiumAndroidDriver, self)._command_from_driver_input(driver_input)
        if command.startswith('/'):
            command = 'http://127.0.0.1:8000' + WEB_TESTS_PATH_PREFIX + \
                '/' + self._port.relative_test_filename(command)
        return command

    def _read_prompt(self, deadline):
        last_char = ''
        while True:
            current_char = self._server_process.read_stdout(deadline, 1)
            if current_char == ' ':
                if last_char in ('#', '$'):
                    return
            last_char = current_char

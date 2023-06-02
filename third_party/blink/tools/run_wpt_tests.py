#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run web platform tests for Chromium-related products."""

import argparse
import contextlib
import functools
import glob
import json
import logging
import multiprocessing
import os
import optparse
import re
import signal
import subprocess
import sys
from typing import List, Tuple

from blinkpy.common import exit_codes
from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.w3c.wpt_results_processor import WPTResultsProcessor
from blinkpy.web_tests.port.base import Port
from blinkpy.web_tests.port import factory

path_finder.add_testing_dir_to_sys_path()
path_finder.add_build_android_to_sys_path()
path_finder.add_build_ios_to_sys_path()
path_finder.bootstrap_wpt_imports()

import mozlog
from wptrunner import wptcommandline, wptlogging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger('run_wpt_tests')

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
    _ANDROID_ENABLED = False

try:
    import xcode_util as xcode
    _IOS_ENABLED = True
except ImportError:
    _IOS_ENABLED = False


class GroupingFormatter(mozlog.formatters.GroupingFormatter):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        # Enable informative log messages, which look like:
        #   WARNING Unsupported test type wdspec for product content_shell
        #
        # Activating logs dynamically with:
        #   StructuredLogger.send_message('show_logs', 'on')
        # appears buggy. This default exists as a workaround.
        self.show_logs = True

    def suite_start(self, data) -> str:
        self.completed_tests = 0
        self.running_tests.clear()
        self.test_output.clear()
        self.subtest_failures.clear()
        self.tests_with_failing_subtests.clear()
        for status in self.expected:
            self.expected[status] = 0
        for tests in self.unexpected_tests.values():
            tests.clear()
        return super().suite_start(data)

    def suite_end(self, data) -> str:
        # Do not show test failures again in noninteractive mode. THey are
        # already shown during the run.
        self.test_failure_text = ''
        return super().suite_end(data)


class MachFormatter(mozlog.formatters.MachFormatter):
    def __init__(self, *args, reset_before_suite: bool = True, **kwargs):
        super().__init__(*args, **kwargs)
        self.reset_before_suite = reset_before_suite

    def suite_start(self, data) -> str:
        output = super().suite_start(data)
        if self.reset_before_suite:
            for counts in self.summary.current['counts'].values():
                counts['count'] = 0
                counts['expected'].clear()
                counts['unexpected'].clear()
                counts['known_intermittent'].clear()
            self.summary.current['unexpected_logs'].clear()
            self.summary.current['intermittent_logs'].clear()
            self.summary.current['harness_errors'].clear()
        return output


class StructuredLogAdapter(logging.Handler):
    def __init__(self, logger, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._logger = logger
        self._fallback_handler = logging.StreamHandler()
        self._fallback_handler.setFormatter(
            logging.Formatter('%(name)s %(levelname)s %(message)s'))

    def emit(self, record):
        log = getattr(self._logger, record.levelname.lower(),
                      self._logger.debug)
        try:
            log(record.getMessage(), component=record.name)
        except mozlog.structuredlog.LoggerShutdownError:
            self._fallback_handler.emit(record)


class WPTAdapter:
    def __init__(self, product, port, options, paths):
        self.product = product
        self.port = port
        self.host = port.host
        self.fs = port.host.filesystem
        self.options = options
        self.paths = paths

    def _set_up_runner_options(self):
        """Set up wptrunner options based on run_wpt_tests.py arguments and defaults."""
        parser = wptcommandline.create_parser()
        # Nightly installation is not supported, so just add defaults.
        parser.set_defaults(
            prompt=False,
            install_browser=False,
            install_webdriver=False,
            channel='nightly',
            affected=None,
        )

        # Install customized versions of `mozlog` formatters.
        for name, formatter in [
            ('grouped', GroupingFormatter),
            ('mach', MachFormatter),
        ]:
            mozlog.commandline.log_formatters[name] = (
                formatter,
                mozlog.commandline.log_formatters[name][1],
            )

        runner_options = parser.parse_args(['--product', self.product.name])
        runner_options.include = []
        runner_options.exclude = []

        # TODO(crbug/1316055): Enable tombstone with '--stackwalk-binary' and
        # '--symbols-path'.
        runner_options.no_capture_stdio = True
        runner_options.manifest_download = False
        runner_options.manifest_update = False
        runner_options.headless = True

        # Set up logging as early as possible.
        self._set_up_runner_output_options(runner_options)
        self._set_up_runner_sharding_options(runner_options)
        self._set_up_runner_config_options(runner_options)
        # self._set_up_runner_debugging_options()
        self._set_up_runner_tests(runner_options)

        for name, value in self.product.product_specific_options().items():
            self._ensure_value(runner_options, name, value)

        runner_options.webdriver_args.extend(
            self.product.additional_webdriver_args())
        return runner_options

    def _ensure_value(self, options, name, value):
        if not getattr(options, name, None):
            setattr(options, name, value)

    def _set_up_runner_output_options(self, runner_options):
        verbose_level = int(self.options.verbose)
        if verbose_level >= 1:
            runner_options.log_mach = '-'
            runner_options.log_mach_level = 'info'
            runner_options.log_mach_verbose = True
        if verbose_level >= 2:
            runner_options.log_mach_level = 'debug'
        if verbose_level >= 3:
            runner_options.webdriver_args.extend([
                '--verbose',
                '--log-path=-',
            ])

        if self.options.json_test_results:
            runner_options.log_chromium = self.options.json_test_results
        else:
            runner_options.log_chromium = self.fs.join(
                self.port.results_directory(), 'results.json')
        runner_options.log_wptreport = self.fs.join(
            self.port.artifacts_directory(), 'wpt_reports.json')
        for log_type in ('chromium', 'wptreport'):
            dest = 'log_%s' % log_type
            filename = getattr(runner_options, dest)
            if filename:
                filename = self.fs.abspath(filename)
                setattr(runner_options, dest,
                        [mozlog.commandline.log_file(filename)])
        runner_options.log = wptlogging.setup(dict(vars(runner_options)),
                                              {'grouped': sys.stdout})
        logging.root.handlers.clear()
        logging.root.addHandler(StructuredLogAdapter(runner_options.log))

    def _set_up_runner_sharding_options(self, runner_options):
        if (self.options.total_shards is not None
                or 'GTEST_TOTAL_SHARDS' in self.host.environ):
            runner_options.total_chunks = int(
                self.options.total_shards
                or self.host.environ['GTEST_TOTAL_SHARDS'])

        if (self.options.shard_index is not None
                or 'GTEST_SHARD_INDEX' in self.host.environ):
            runner_options.this_chunk = 1 + int(
                self.options.shard_index
                or self.host.environ['GTEST_SHARD_INDEX'])

        # Override the default sharding strategy, which is to shard by directory
        # (`dir_hash`). Sharding by test ID attempts to maximize shard workload
        # uniformity, as test leaf directories can vary greatly in size.
        runner_options.chunk_type = 'id_hash'

    def _set_up_runner_config_options(self, runner_options):
        runner_options.webdriver_args.extend([
            '--enable-chrome-logs',
        ])
        runner_options.binary_args.extend([
            '--host-resolver-rules='
            'MAP nonexistent.*.test ~NOTFOUND, MAP *.test 127.0.0.1',
            '--enable-experimental-web-platform-features',
            '--enable-blink-features=MojoJS,MojoJSTest',
            '--enable-blink-test-features',
            '--disable-field-trial-config',
        ])
        runner_options.mojojs_path = self.port.generated_sources_directory()

        # TODO: RWT has subtle control on how tests are retried. For example
        # there won't be automatic retry of failed tests when they are specified
        # at command line individually. We need such capability for repeat to
        # work correctly.
        runner_options.repeat = self.options.iterations

        if self.options.run_wpt_internal:
            runner_options.config = self.port._path_finder.path_from_web_tests(
                'wptrunner.blink.ini')

        if self.port.flag_specific_config_name():
            # Enable adding smoke tests later.
            configs = self.port.flag_specific_configs()
            args, _ = configs[self.port.flag_specific_config_name()]
            logger.info('Running with flag-specific arguments: "%s"',
                        ' '.join(args))
            runner_options.binary_args.extend(args)

        if self.options.enable_leak_detection:
            runner_options.binary_args.extend(['--enable-leak-detection'])

        runner_options.binary_args.extend(self.options.additional_driver_flag)

        if self.options.enable_sanitizer:
            runner_options.timeout_multiplier = 2
            logger.info('Defaulting to 2x timeout multiplier because '
                        'sanitizer is enabled')

        if self.options.use_upstream_wpt:
            # when running with upstream, the goal is to get wpt report that can
            # be uploaded to wpt.fyi. We do not really care if tests failed or
            # not. Add '--no-fail-on-unexpected' so that the overall result is
            # success. Add '--no-restart-on-unexpected' to speed up the test. On
            # Android side, we are always running with one emulator or worker,
            # so do not add '--run-by-dir=0'
            runner_options.retry_unexpected = 0
            runner_options.fail_on_unexpected = False
            runner_options.restart_on_unexpected = False
        else:
            # By default, wpt will treat unexpected passes as errors, so we
            # disable that to be consistent with Chromium CI. Add
            # '--run-by-dir=0' so that tests can be more evenly distributed
            # among workers.
            runner_options.retry_unexpected = 3
            runner_options.fail_on_unexpected_pass = False
            runner_options.restart_on_unexpected = False
            runner_options.restart_on_new_group = False
            runner_options.run_by_dir = 0
            runner_options.reuse_window = True

        if self.options.num_retries is not None:
            runner_options.retry_unexpected = self.options.num_retries

        # TODO: repeat_each will restart browsers between tests. Wptrunner's
        # rerun will not restart browsers. Might also need to restart the
        # browser at Wptrunner side.
        runner_options.rerun = self.options.repeat_each

    # TODO: Wptrunner's pause_after_test feature could be useful. Need to figure out
    # the correct CLI for that.
    # def _set_up_runner_debugging_options(self):
    #     self.port.set_option_default('use_xvfb', self.port.get_option('headless'))
    #     if not options.headless and options.processes is None:
    #         logger.info('Not headless; default to 1 worker to avoid '
    #                     'opening too many windows')
    #         options.processes = 1

    def _set_up_runner_tests(self, runner_options):
        if self.options.gtest_filter:
            runner_options.include.extend(
                self._parse_gtest_filter(self.options.gtest_filter))

        if self.options.isolated_script_test_filter:
            include, exclude = self._parse_isolated_script_test_filter(
                self.options.isolated_script_test_filter)
            runner_options.include.extend(include)
            runner_options.exclude.extend(exclude)

        runner_options.exclude.extend(self.options.ignore_tests)

        for path in self.paths:
            runner_options.include.append(
                self.port._path_finder.strip_wpt_path(path))

        if self.port.default_smoke_test_only():
            smoke_file_short_path = self.fs.relpath(
                self.port.path_to_smoke_tests_file(),
                self.port.web_tests_dir())
            if not _has_explicit_tests(runner_options):
                runner_options.include.extend(self._load_smoke_tests())
                logger.info(
                    'Tests not explicitly specified; '
                    'running tests from %s', smoke_file_short_path)
            else:
                logger.warning(
                    'Tests explicitly specified; '
                    'not running tests from %s', smoke_file_short_path)

        runner_options.exclude.extend([
            # Exclude webdriver tests for now. The CI runs them separately.
            'webdriver',
            'infrastructure/webdriver',
        ])

        if self.options.zero_tests_executed_ok and runner_options.include:
            runner_options.default_exclude = True

    def _load_smoke_tests(self):
        """Read the smoke tests file and append its tests to the test list.

        This method handles smoke test files inherited from `run_web_tests.py`
        differently from the native `wpt run --include-file` parameter.
        Specifically, tests are assumed to be relative to `web_tests/`, so a
        line without a recognized `external/wpt/` or `wpt_internal/` prefix is
        assumed to be a legacy layout test that is excluded.
        """
        smoke_file_path = self.port.path_to_smoke_tests_file()
        tests = []
        with self.fs.open_text_file_for_reading(smoke_file_path) as smoke_file:
            for line in smoke_file:
                test, _, _ = line.partition('#')
                test = test.strip()
                for wpt_dir, url_prefix in Port.WPT_DIRS.items():
                    if not wpt_dir.endswith('/'):
                        wpt_dir += '/'
                    if test.startswith(wpt_dir):
                        tests.append(test.replace(wpt_dir, url_prefix, 1))
        return tests

    def run_tests(self) -> int:
        with contextlib.ExitStack() as stack:
            tmp_dir = stack.enter_context(self.fs.mkdtemp())
            # Manually remove the temporary directory's contents recursively
            # after the tests complete. Otherwise, `mkdtemp()` raise an error.
            stack.callback(self.fs.rmtree, tmp_dir)

            if self.options.clobber_old_results:
                self.port.clobber_old_results()
            elif self.fs.exists(self.port.artifacts_directory()):
                self.port.limit_archived_results_count()

                # Rename the existing results folder for archiving.
                self.port.rename_results_folder()

            # Create the output directory if it doesn't already exist.
            self.fs.maybe_make_directory(self.port.artifacts_directory())

            stack.enter_context(self.product.test_env())

            runner_options = self._set_up_runner_options()

            if self.options.use_upstream_wpt:
                tests_root = tools_root = self.fs.join(tmp_dir, 'upstream-wpt')
                logger.info('Using upstream wpt, cloning to %s ...',
                            tests_root)
                self.host.executive.run_command([
                    'git', 'clone', UPSTREAM_GIT_URL, tests_root, '--depth=25'
                ])
                self._checkout_3h_epoch_commit(tools_root)
            else:
                tests_root = self.port._path_finder.path_from_wpt_tests()
                tools_root = path_finder.get_wpt_tools_wpt_dir()

            runner_options.tests_root = tests_root
            runner_options.metadata_root = tests_root
            logger.debug('Using WPT tests (external) from %s', tests_root)
            logger.debug('Using WPT tools from %s', tools_root)

            runner_options.run_info = tmp_dir
            # The filename must be `mozinfo.json` for wptrunner to read it from the
            # `--run-info` directory.
            self._create_extra_run_info(self.fs.join(tmp_dir, 'mozinfo.json'),
                                        tests_root)

            self.port.setup_test_run()  # Start Xvfb, if necessary.
            stack.callback(self.port.clean_up_test_run)
            self.fs.chdir(self.port.web_tests_dir())
            run = _load_entry_point(tools_root)
            stack.enter_context(
                self.process_and_upload_results(runner_options))

            exit_code = run(**vars(runner_options))
            return exit_code


    def _checkout_3h_epoch_commit(self, tools_root: str):
        wpt_executable = self.fs.join(tools_root, 'wpt')
        output = self.host.executive.run_command(
            [wpt_executable, 'rev-list', '--epoch', '3h'])
        commit = output.splitlines()[0]
        logger.info('Running against upstream wpt@%s', commit)
        self.host.executive.run_command(['git', 'checkout', commit],
                                        cwd=tools_root)

    def _create_extra_run_info(self, run_info_path, tests_root):
        run_info = {
            # This property should always be a string so that the metadata
            # updater works, even when wptrunner is not running a flag-specific
            # suite.
            'os': self.port.operating_system(),
            'port': self.port.version(),
            'debug': self.port.get_option('configuration') == 'Debug',
            'flag_specific': self.port.flag_specific_config_name() or '',
            'used_upstream': self.options.use_upstream_wpt,
            'sanitizer_enabled': self.options.enable_sanitizer,
        }
        if self.options.use_upstream_wpt:
            # `run_wpt_tests` does not run in the upstream checkout's git
            # context, so wptrunner cannot infer the latest revision. Manually
            # add the revision here.
            run_info['revision'] = self.host.git(
                path=tests_root).current_revision()

        with self.fs.open_text_file_for_writing(run_info_path) as file_handle:
            json.dump(run_info, file_handle)

    @contextlib.contextmanager
    def process_and_upload_results(self, options):
        artifacts_dir = self.port.artifacts_directory()
        processor = WPTResultsProcessor(self.fs,
                                        self.port,
                                        artifacts_dir=artifacts_dir)
        with processor.stream_results() as events:
            options.log.add_handler(events.put)
            yield

        processor.process_wpt_report(options.log_wptreport[0].name)

        processor.process_results_json(options.log_chromium[0].name)

        if self.port.get_option('show_results') and processor.has_regressions:
            self.port.show_results_html_file(
                self.fs.join(artifacts_dir, 'results.html'))

    def _parse_gtest_filter(self, value: str) -> List[str]:
        return [
            self.path_finder.strip_wpt_path(test_id)
            for test_id in value.split(':')
        ]

    def _resolve_tests(self, test_filter: str) -> Tuple[List[str], List[str]]:
        """Resolve an isolated script-style filter string into lists of tests.

        Arguments:
            test_filter: Glob patterns delimited by double colons ('::'). The
                glob is prefixed with '-' to indicate that tests matching the
                pattern should not run. Assume a valid wpt name cannot start
                with '-'.

        Returns:
            Tests to include and exclude, respectively.
        """
        included_tests, excluded_tests = [], []
        for pattern in test_filter.split('::'):
            test_group = included_tests
            if pattern.startswith('-'):
                test_group, pattern = excluded_tests, pattern[1:]
            if self.port._path_finder.is_wpt_internal_path(pattern):
                pattern_on_disk = self.port._path_finder.path_from_web_tests(
                    pattern)
            else:
                pattern_on_disk = self.port._path_finder.path_from_wpt_tests(
                    pattern)
            test_group.extend(glob.glob(pattern_on_disk))

        return included_tests, excluded_tests

    def _parse_isolated_script_test_filter(self, values):
        include, exclude = [], []
        if isinstance(values, str):
            values = [values]
        if isinstance(values, list):
            for test_filter in values:
                extra_include, extra_exclude = self._resolve_tests(test_filter)
                include.extend(extra_include)
                exclude.extend(extra_exclude)
        return include, exclude


def _has_explicit_tests(options: argparse.Namespace) -> bool:
    return (options.include or options.exclude or options.include_file)


def _load_entry_point(tools_root: str):
    """Import and return a callable that runs wptrunner.

    Arguments:
        tests_root: Path to a directory whose structure corresponds to the WPT
            repository. This will use the tools under `tools/`.

    Returns:
        Callable whose keyword arguments are the namespace corresponding to
        command line options.
    """
    if tools_root not in sys.path:
        sys.path.insert(0, tools_root)
    # Remove current cached modules to force a reload.
    module_pattern = re.compile(r'^(tools|wpt(runner|serve)?)\b')
    for name in list(sys.modules):
        if module_pattern.search(name):
            del sys.modules[name]
    from tools import localpaths
    from tools.wpt import run
    from tools.wpt.virtualenv import Virtualenv
    import wptrunner
    import wptserve
    for module in (run, wptrunner, wptserve):
        assert module.__file__.startswith(tools_root), module.__file__

    # vpython, not virtualenv, vends third-party packages in chromium/src.
    dummy_venv = Virtualenv(path_finder.get_source_dir(),
                            skip_virtualenv_setup=True)
    return functools.partial(run.run, dummy_venv)


def make_product(port, options) -> 'Product':
    name = options.product
    product_cls = make_product_registry()[name]
    return product_cls(port, options)


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
        return {}

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
            'binary': self.default_binary,
            'webdriver_binary': self.default_webdriver_binary,
            'processes': self._port.default_child_processes()
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
        return self._port._build_path(binary_path)

    @property
    def default_webdriver_binary(self):
        if self._host.platform.is_win():
            path = 'chromedriver.exe'
        else:
            path = 'chromedriver'  #linux and mac
        return self._port._build_path(path)


class ContentShell(Product):
    name = 'content_shell'

    def product_specific_options(self):
        """Product-specific wptrunner parameters needed to run tests."""
        return {
            'binary': self.default_binary,
            'processes': self._port.default_child_processes()
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
        return self._port._build_path(binary_path)


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
                    logger.error(
                        'Xcode build version %s failed to install: %s ',
                        self.xcode_build_version, e)
                else:
                    logger.info(
                        'Xcode build version %s successfully installed.',
                        self.xcode_build_version)
            else:
                logger.warning('Skip the Xcode installation, no '
                               '--xcode-build-version')
            yield


class ChromeAndroidBase(Product):
    def __init__(self, port, options):
        super().__init__(port, options)
        if options.browser_apk:
            self.browser_apk = options.browser_apk
        else:
            self.browser_apk = self.default_browser_apk
        self.adb_binary = devil_env.config.FetchPath('adb')
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
        if not _ANDROID_ENABLED:
            raise Exception('Android is not available')
        instances = []
        try:
            if self._options.avd_config:
                logger.info(
                    f'Installing emulator from {self._options.avd_config}')
                config = avd.AvdConfig(self._options.avd_config)
                config.Install()

                # use '--child-processes' to decide how many emulators to launch
                for _ in range(max(self._options.child_processes or 1, 1)):
                    instance = config.CreateInstance()
                    instances.append(instance)

                SyncParallelizer(instances).Start(
                    writable_system=True,
                    window=self._options.emulator_window,
                    require_fast_start=True)

            #TODO(weizhong): when choose device, make sure abi matches with target
            yield device_utils.DeviceUtils.HealthyDevices()
        finally:
            SyncParallelizer(instances).Stop()

    @contextlib.contextmanager
    def test_env(self):
        with super().test_env():
            devil_chromium.Initialize(adb_path=self.adb_binary)
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
                logger.info('Product version: %s %s (package: %r)', self.name,
                            version, version_provider)
                return version
            except CommandFailedError:
                logger.warning(
                    'Failed to retrieve version of %s (package: %r)',
                    self.name, version_provider)
        return None

    @property
    def default_webdriver_binary(self):
        return self._port._build_path('clang_x64', 'chromedriver')

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
        self._tasks.enter_context(SyncParallelizer(contexts))

    @contextlib.contextmanager
    def _provision_device(self, device):
        """Provision a single Android device for a test.

        This method will be executed in parallel on all devices, so
        it is crucial that it is thread safe.
        """
        with contextlib.ExitStack() as exit_stack:
            exit_stack.enter_context(
                self._install_apk(device, self.browser_apk))
            logger.info('Provisioned device (serial: %s)', device.serial)
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
        return self._port._build_path('apks', 'SystemWebViewShell.apk')

    @property
    def default_webview_provider(self):
        return self._port._build_path('apks', 'SystemWebView.apk')

    def _install_webview(self, device):
        # Prioritize local builds.
        return webview_app.UseWebViewProvider(device, self.webview_provider)

    def get_browser_package_name(self):
        return (super().get_browser_package_name()
                or 'org.chromium.webview_shell')

    def get_version_provider_package_name(self):
        # Use the version from the webview provider, not the shell, since the
        # provider is distributed to end users. The shell is developer-facing,
        # so its version is usually not actively updated.
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
        return self._port._build_path('apks', 'ChromePublic.apk')


def handle_interrupt_signals():
    def termination_handler(_signum, _unused_frame):
        raise KeyboardInterrupt()

    if sys.platform == "win32":
        signal.signal(signal.SIGBREAK, termination_handler)
    else:
        signal.signal(signal.SIGTERM, termination_handler)


def parse_arguments(argv):
    parser = argparse.ArgumentParser(
        usage='%(prog)s [options] [tests]',
        description=
        'Runs Blink wpt tests as described in //docs/testing/web_platform_tests_wptrunner.md'
    )
    factory.add_configuration_options_group(parser,
                                            rwt=False,
                                            product_choices=list(
                                                make_product_registry()))
    factory.add_logging_options_group(parser)
    factory.add_results_options_group(parser, rwt=False)
    factory.add_testing_options_group(parser, rwt=False)
    factory.add_android_options_group(parser)
    factory.add_ios_options_group(parser)

    parser.add_argument('tests',
                        nargs='*',
                        help='Paths to test files or directories to run')
    params = vars(parser.parse_args(argv))
    args = params.pop('tests')
    options = optparse.Values(params)
    return (options, args)


def main(argv) -> int:
    # Force log output in utf-8 instead of a locale-dependent encoding. On
    # Windows, this can be cp1252. See: crbug.com/1371195.
    if sys.version_info[:2] >= (3, 7):
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')

    # Also apply utf-8 mode to python subprocesses.
    os.environ['PYTHONUTF8'] = '1'

    # Convert SIGTERM to be handled as KeyboardInterrupt to handle early termination
    # Same handle is declared later on in wptrunner
    # See: https://github.com/web-platform-tests/wpt/blob/25cd6eb086db5977ac51f7dee7faafe6772dc9d7/tools/wptrunner/wptrunner/wptrunner.py
    # This early declaration allow graceful exit when Chromium swarming kill process before wpt starts
    handle_interrupt_signals()

    try:
        host = Host()
        options, args = parse_arguments(argv)
        port = host.port_factory.get(None, options)
        product = make_product(port, options)
        adapter = WPTAdapter(product, port, options, args)
    except (NotImplementedError, ValueError) as error:
        logger.error(error)
        return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

    try:
        return adapter.run_tests()
    except KeyboardInterrupt:
        logger.critical("Harness exited after signal interrupt")
        return exit_codes.INTERRUPTED_EXIT_STATUS


if __name__ == '__main__':
    multiprocessing.set_start_method('spawn')
    sys.exit(main(sys.argv[1:]))

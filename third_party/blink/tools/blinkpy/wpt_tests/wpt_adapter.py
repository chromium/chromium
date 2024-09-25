# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run web platform tests as described in //docs/testing/run_web_platform_tests.md"""

import argparse
import contextlib
import functools
import json
import logging
import os
import optparse
import signal
import subprocess
import sys
import textwrap
from collections import defaultdict
from datetime import datetime
from typing import List, Optional

from blinkpy.common import exit_codes
from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.common.system import command_line
from blinkpy.tool.blink_tool import BlinkTool
from blinkpy.w3c.local_wpt import LocalWPT
from blinkpy.w3c.wpt_results_processor import WPTResultsProcessor
from blinkpy.web_tests.controllers.web_test_finder import WebTestFinder
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.port import factory
from blinkpy.wpt_tests import product
from blinkpy.wpt_tests.test_loader import TestLoader, wpt_url_to_blink_test

path_finder.bootstrap_wpt_imports()
import mozlog
from tools import localpaths
from tools.wpt import run
from tools.wpt.virtualenv import Virtualenv
from wptrunner import wptcommandline, wptlogging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger('run_wpt_tests')


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
        self._start = datetime.now()

    def get_test_name_output(self, subsuite, test_name):
        if not test_name.startswith('/wpt_internal/'):
            test_name = '/external/wpt' + test_name
        return f'virtual/{subsuite}{test_name}' if subsuite else test_name[1:]

    def log(self, data):
        offset = datetime.now() - self._start
        minutes, seconds = divmod(max(0, offset.total_seconds()), 60)
        hours, minutes = divmod(minutes, 60)
        milliseconds, _ = divmod(offset.microseconds, 1000)
        # A relative timestamp is more useful for comparing event timings than
        # an absolute one.
        timestamp = f'{int(hours):02}:{int(minutes):02}:{int(seconds):02}.{int(milliseconds):03}'
        # Place mandatory fields first so that logs are vertically aligned as
        # much as possible.
        message = f'{timestamp} {data["level"]}: {data["message"]}'
        if 'stack' in data:
            message = f'{message}\n{data["stack"]}'
        return self.generate_output(text=message + '\n')

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
        # Do not show test failures or flakes again in noninteractive mode.
        # They are already shown during the run. We also don't need to
        # differentiate between the primary expectation and "known
        # intermittent" statuses.
        self.test_failure_text = ''
        self.known_intermittent_results.clear()
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
            log(record.getMessage(),
                component=record.name,
                exc_info=record.exc_info)
        except mozlog.structuredlog.LoggerShutdownError:
            self._fallback_handler.emit(record)


class WPTAdapter:
    PORT_NAME_BY_PRODUCT = {
        'android_webview': 'webview',
        'chrome_android': 'android',
    }

    def __init__(self, product, port, options, paths):
        self.product = product
        self.port = port
        self.host = port.host
        self.fs = port.host.filesystem
        self.finder = path_finder.PathFinder(self.fs)
        self.options = options
        self.paths = paths
        self._processor = WPTResultsProcessor(
            self.fs,
            self.port,
            artifacts_dir=self.port.artifacts_directory(),
            reset_results=self.options.reset_results,
            repeat_each=self.options.repeat_each,
            processes=product.processes)
        self._expectations = TestExpectations(self.port)

    @classmethod
    def from_args(cls,
                  host: Host,
                  args: List[str],
                  port_name: Optional[str] = None):
        options, tests = parse_arguments(args)
        cls._ensure_value(options, 'wpt_only', True)
        # only run virtual tests for headless shell
        cls._ensure_value(options, 'no_virtual_tests', options.product
                          not in ['headless_shell', 'chrome'])

        env_shard_index = host.environ.get('GTEST_SHARD_INDEX')
        if env_shard_index is not None:
            cls._ensure_value(options, 'shard_index', int(env_shard_index))
        env_total_shards = host.environ.get('GTEST_TOTAL_SHARDS')
        if env_total_shards is not None:
            cls._ensure_value(options, 'total_shards', int(env_total_shards))

        if options.product in cls.PORT_NAME_BY_PRODUCT:
            port_name = cls.PORT_NAME_BY_PRODUCT[options.product]
        port = host.port_factory.get(port_name, options)
        product = make_product(port, options)
        return WPTAdapter(product, port, options, tests)

    def set_up_derived_options(self):
        explicit_tests = self.paths or self.options.test_list
        if (not explicit_tests and self.options.smoke is None
                and not self.using_upstream_wpt):
            self.options.smoke = self.port.default_smoke_test_only()
        if self.options.smoke:
            if not explicit_tests and self.options.num_retries is None:
                # Retry failures 3 times if we're running a smoke test without
                # additional tests. SmokeTests is an explicit list of tests, so we
                # wouldn't retry by default without this special case.
                self.options.num_retries = 3

            if not self.options.test_list:
                self.options.test_list = []
            self.options.test_list.append(self.port.path_to_smoke_tests_file())

        if self.options.gtest_filter:
            self.paths.extend(self.options.gtest_filter.split(':'))

    def log_config(self):
        logger.info(f'Running tests for {self.product.name}')
        logger.info(f'Using port "{self.port.name()}"')
        logger.info(
            f'View the test results at file://{self.port.artifacts_directory()}/results.html'
        )
        logger.info(f'Using {self.port.get_option("configuration")} build')
        flag_specific = self.port.flag_specific_config_name()
        if flag_specific:
            logger.info(f'Running flag-specific suite "{flag_specific}"')

    def _set_up_runner_options(self, tmp_dir):
        """Set up wptrunner options based on run_wpt_tests.py arguments and defaults."""
        parser = wptcommandline.create_parser()
        # Nightly installation is not supported, so just add defaults.
        parser.set_defaults(
            prompt=False,
            install_browser=False,
            install_webdriver=False,
            channel='nightly',
            affected=None,
            logcat_dir=None,
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
        runner_options.pause_after_test = False
        runner_options.headless = self.options.headless

        # Set up logging as early as possible.
        self._set_up_runner_output_options(runner_options)
        self._set_up_runner_config_options(runner_options)
        # TODO(crbug.com/1351820): Find the difference of the host cert ssl set up and make iOS
        # use the same.
        if self.product.name != 'chrome_ios':
            self._set_up_runner_ssl_options(runner_options)
        self._set_up_runner_debugging_options(runner_options)
        self._set_up_runner_tests(runner_options, tmp_dir)
        self.product.update_runner_options(runner_options)
        return runner_options

    @classmethod
    def _ensure_value(cls, options, name, value):
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
            runner_options.webdriver_args.append('--verbose')
        else:
            # Disable all `chromedriver` logs except from `chrome_launcher.cc`,
            # which logs the `chrome` command that `WPTResultsProcessor` will
            # extract.
            runner_options.webdriver_args.extend([
                '--log-level=INFO',
                '--vmodule=chrome_launcher=0,*/chrome/test/chromedriver/*=-1',
            ])

        if self.using_upstream_wpt:
            runner_options.log_wptreport = [
                mozlog.commandline.log_file(
                    self.fs.join(self.port.results_directory(),
                                 'wpt_reports.json'))
            ]

        # Dump `*-{actual,expected}.png` screenshots for all failures like
        # `run_web_tests.py` does. See crbug.com/40947531.
        runner_options.reftest_screenshot = 'fail'
        runner_options.log = wptlogging.setup(dict(vars(runner_options)),
                                              {'grouped': sys.stdout})
        logging.root.handlers.clear()
        logging.root.addHandler(StructuredLogAdapter(runner_options.log))

    def _set_up_runner_sharding_options(self, runner_options):
        # This is now only used for '--use-upstream-wpt'. For other cases
        # sharding is done inside the wrapper, and total_chunks is always
        # set to 1.
        if self.options.total_shards is not None:
            runner_options.total_chunks = int(self.options.total_shards)

        if self.options.shard_index is not None:
            runner_options.this_chunk = 1 + int(self.options.shard_index)

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
            'MAP nonexistent.*.test ^NOTFOUND,'
            'MAP *.test 127.0.0.1, MAP *.test. 127.0.0.1',
            *self.port.additional_driver_flags(),
        ])

        # Implicitly pass `--enable-blink-features=MojoJS,MojoJSTest` and
        # `--enable-experimental-web-platform-features` to the browser binary.
        # The latter is needed in addition to `--enable-blink-test-features`
        # because it enables some Chromium-side `base::Feature()`s:
        # https://chromium.googlesource.com/chromium/src/+/main/content/public/common/content_switch_dependent_feature_overrides.cc
        runner_options.mojojs_path = self.port.generated_sources_directory()
        runner_options.enable_experimental = True

        # TODO: RWT has subtle control on how tests are retried. For example
        # there won't be automatic retry of failed tests when they are specified
        # at command line individually. We need such capability for repeat to
        # work correctly.
        runner_options.repeat = self.options.iterations
        runner_options.fully_parallel = self.options.fully_parallel
        runner_options.leak_check = self.options.enable_leak_detection

        if self.options.run_wpt_internal:
            runner_options.config = self.finder.path_from_web_tests(
                'wptrunner.blink.ini')

        if (self.options.enable_sanitizer
                or self.options.configuration == 'Debug'):
            runner_options.timeout_multiplier = 5
            logger.info('Defaulting to 5x timeout multiplier because '
                        'the build is debug or sanitized')
        elif self.options.timeout_multiplier:
            runner_options.timeout_multiplier = self.options.timeout_multiplier

        if self.using_upstream_wpt:
            # when running with upstream, the goal is to get wpt report that can
            # be uploaded to wpt.fyi. We do not really care if tests failed or
            # not. Add '--no-fail-on-unexpected' so that the overall result is
            # success. Add '--no-restart-on-unexpected' to speed up the test. On
            # Android side, we are always running with one emulator or worker,
            # so do not add '--run-by-dir=0'
            runner_options.fail_on_unexpected = False
            runner_options.restart_on_unexpected = False
        else:
            # Unexpected subtest passes in wptrunner are equivalent to baseline
            # mismatches in `run_web_tests.py`, so don't pass
            # `--no-fail-on-unexpected-pass`. The test loader always adds
            # test-level pass expectations to compensate.
            runner_options.restart_on_unexpected = False
            runner_options.restart_on_new_group = False
            # Add `--run-by-dir=0` so that tests can be more evenly distributed
            # among workers.
            if not runner_options.fully_parallel:
                runner_options.run_by_dir = 0
            runner_options.reuse_window = True

        # TODO: repeat_each will restart browsers between tests. Wptrunner's
        # rerun will not restart browsers. Might also need to restart the
        # browser at Wptrunner side.
        runner_options.rerun = self.options.repeat_each

    def _set_up_runner_ssl_options(self, runner_options):
        # wptrunner doesn't recognize the `pregenerated.*` values in
        # `external/wpt/config.json`, so pass them here.
        #
        # See also: https://github.com/web-platform-tests/wpt/pull/41594
        certs_path = self.finder.path_from_chromium_base(
            'third_party', 'wpt_tools', 'certs')
        runner_options.ca_cert_path = self.fs.join(certs_path, 'cacert.pem')
        runner_options.host_key_path = self.fs.join(certs_path,
                                                    '127.0.0.1.key')
        runner_options.host_cert_path = self.fs.join(certs_path,
                                                     '127.0.0.1.pem')

    def _set_up_runner_debugging_options(self, runner_options):
        if self.options.wrapper:
            runner_options.debugger = self.options.wrapper[0]
            # `wpt run` expects a plain `str`, not a `List[str]`:
            # https://github.com/web-platform-tests/wpt/blob/9593290a/tools/wptrunner/wptrunner/wptcommandline.py#L190
            runner_options.debugger_args = ' '.join(self.options.wrapper[1:])

    def _collect_tests(self):
        finder = WebTestFinder(self.port, self.options)

        try:
            self.paths, all_test_names, _ = finder.find_tests(
                self.paths,
                test_lists=self.options.test_list,
                filter_files=self.options.isolated_script_test_filter_file,
                inverted_filter_files=self.options.
                inverted_test_launcher_filter_file,
                fastest_percentile=None,
                filters=self.options.isolated_script_test_filter)
        except IOError:
            logger.exception('Failed to find tests.')

        if self.options.num_retries is None:
            # If --test-list is passed, or if no test narrowing is specified,
            # default to 3 retries. Otherwise [e.g. if tests are being passed by
            # name], default to 0 retries.
            if self.options.test_list or len(self.paths) < len(all_test_names):
                self.options.num_retries = 3
            else:
                self.options.num_retries = 0

        # sharding the tests the same way as in RWT
        test_names = finder.split_into_chunks(all_test_names)
        # TODO(crbug.com/1426296): Actually log these tests as
        # `test_{start,end}` in `mozlog` so that they're recorded in the results
        # JSON (and shown in `results.html`).
        tests_to_skip = finder.skip_tests(self.paths, test_names,
                                          self._expectations)
        tests_to_run = [
            test for test in test_names if test not in tests_to_skip
        ]

        if not tests_to_run and not self.options.zero_tests_executed_ok:
            logger.error('No tests to run.')
            sys.exit(exit_codes.NO_TESTS_EXIT_STATUS)

        return self._prepare_tests_for_wptrunner(tests_to_run)

    def _lookup_subsuite_args(self, subsuite_name):
        for suite in self.port.virtual_test_suites():
            if suite.full_prefix == f"virtual/{subsuite_name}/":
                return suite.args
        return []

    def _prepare_tests_for_wptrunner(self, tests_to_run):
        """Remove external/wpt from test name, and create subsuite config
        """
        tests_by_subsuite = defaultdict(list)
        include_tests = []
        for test in tests_to_run:
            (subsuite_name,
             base_test) = self.port.get_suite_name_and_base_test(test)
            if subsuite_name:
                base_test = self.finder.strip_wpt_path(base_test)
                tests_by_subsuite[subsuite_name].append(base_test)
            else:
                include_tests.append(self.finder.strip_wpt_path(test))

        subsuite_json = {}
        for subsuite_name, tests in tests_by_subsuite.items():
            subsuite_args = self._lookup_subsuite_args(subsuite_name)
            subsuite = {
                'name': subsuite_name,
                'config': {
                    'binary_args': subsuite_args,
                },
                # The Blink implementation of `TestLoader` needs the
                # `virtual_suite` property to derive virtual test names.
                'run_info': {
                    'virtual_suite': subsuite_name,
                },
                'include': tests,
            }
            subsuite_json[subsuite_name] = subsuite
        return include_tests, subsuite_json

    def _set_up_runner_tests(self, runner_options, tmp_dir):
        if not self.using_upstream_wpt:
            include_tests, subsuite_json = self._collect_tests()
            if subsuite_json:
                config_path = self.fs.join(tmp_dir, 'subsuite.json')
                with self.fs.open_text_file_for_writing(
                        config_path) as outfile:
                    json.dump(subsuite_json, outfile)
                runner_options.subsuite_file = config_path
                runner_options.subsuites = list(subsuite_json)

            runner_options.include.extend(include_tests)
            runner_options.test_types = self.options.test_types
            runner_options.retry_unexpected = self.options.num_retries

            self._processor.failure_threshold = self.port.max_allowed_failures(
                len(include_tests))
            self._processor.crash_timeout_threshold = self.port.max_allowed_crash_or_timeouts(
                len(include_tests))

            # sharding is done inside wrapper
            runner_options.total_chunks = 1
            runner_options.this_chunk = 1
            runner_options.default_exclude = True
        else:
            self._set_up_runner_sharding_options(runner_options)
            runner_options.retry_unexpected = 0
            if self.paths or self.options.test_list:
                logger.warning('`--use-upstream-wpt` will run all tests. '
                               'Explicitly provided tests are ignored.')

    @contextlib.contextmanager
    def test_env(self):
        with contextlib.ExitStack() as stack:
            tmp_dir = stack.enter_context(self.fs.mkdtemp())
            # Manually remove the temporary directory's contents recursively
            # after the tests complete. Otherwise, `mkdtemp()` raise an error.
            stack.callback(self.fs.rmtree, tmp_dir)
            stack.enter_context(self.product.test_env())
            runner_options = self._set_up_runner_options(tmp_dir)
            self.log_config()

            if self.options.clobber_old_results:
                self.port.clobber_old_results()
            elif self.fs.exists(self.port.artifacts_directory()):
                self.port.limit_archived_results_count()
                # Rename the existing results folder for archiving.
                self.port.rename_results_folder()

            # Create the output directory if it doesn't already exist.
            self.fs.maybe_make_directory(self.port.artifacts_directory())
            # Set additional environment for python subprocesses
            string_variables = getattr(self.options, 'additional_env_var', [])
            for string_variable in string_variables:
                name, value = string_variable.split('=', 1)
                logger.info('Setting environment variable %s to %s', name,
                            value)
                os.environ[name] = value

            if self.using_upstream_wpt:
                tests_root = self.tools_root
            else:
                tests_root = self.finder.path_from_wpt_tests()
                TestLoader.install(self.port, self._expectations,
                                   runner_options.include)
            runner_options.tests_root = tests_root
            runner_options.metadata_root = tests_root
            logger.debug('Using WPT tests (external) from %s', tests_root)
            logger.debug('Using WPT tools from %s', self.tools_root)

            runner_options.run_info = tmp_dir
            self._initialize_tmp_dir(tmp_dir, tests_root)

            stack.enter_context(
                self.process_and_upload_results(runner_options))
            self.port.setup_test_run()  # Start Xvfb, if necessary.
            stack.callback(self.port.clean_up_test_run)
            # Restore the original CWD as soon as the call into `wpt run` is
            # over. This ensures relative paths for `--json-test-results` and
            # other options work correctly.
            stack.callback(self.fs.chdir, self.fs.getcwd())
            # Changing the CWD is not ideal, but necessary for `wptserve` to
            # resolve relative paths in `external/wpt/config.json` correctly.
            #
            # TODO(crbug.com/362344569): Replace this workaround. One option is
            # to add a `wpt run` parameter to point to a wptserve config with
            # absolutized paths.
            self.fs.chdir(self.port.web_tests_dir())
            yield runner_options

    @functools.cached_property
    def tools_root(self) -> str:
        """Find the path to the tooling directory under use.

        This is `//third_party/wpt_tools/wpt/` when using Chromium-vended WPT
        tools.
        """
        tools_dir = self.fs.dirname(localpaths.__file__)
        return self.fs.dirname(tools_dir)

    @functools.cached_property
    def using_upstream_wpt(self) -> bool:
        """Dynamically detect whether this test run uses upstream WPT or not."""
        vended_wpt = self.finder.path_from_chromium_base(
            'third_party', 'wpt_tools', 'wpt')
        return self.fs.realpath(
            self.tools_root) != self.fs.realpath(vended_wpt)

    def run_tests(self) -> int:
        exit_code = 0
        try:
            with self.test_env() as runner_options:
                run = _load_entry_point()
                exit_code = 1 if run(**vars(runner_options)) else 0
        except KeyboardInterrupt:
            logger.critical('Harness exited after signal interrupt')
            exit_code = exit_codes.INTERRUPTED_EXIT_STATUS
        # Write the partial results for an interrupted run. This also ensures
        # the results directory is rotated next time.
        self._processor.copy_results_viewer()
        results_json = self._processor.process_results_json(
            self.port.get_option('json_test_results'))
        return exit_code or int(results_json['num_regressions'] > 0)

    def _initialize_tmp_dir(self, tmp_dir: str, tests_root: str):
        assert self.fs.isdir(tmp_dir), tmp_dir
        run_info = {
            # This property should always be a string so that the metadata
            # updater works, even when wptrunner is not running a flag-specific
            # suite.
            'os': self.port.operating_system(),
            'port': self.port.version(),
            'debug': self.port.get_option('configuration') == 'Debug',
            'flag_specific': self.port.flag_specific_config_name() or '',
            'used_upstream': self.using_upstream_wpt,
            'sanitizer_enabled': self.options.enable_sanitizer,
            'virtual_suite': '',  # Needed for non virtual tests
        }
        if self.using_upstream_wpt:
            # `run_wpt_tests` does not run in the upstream checkout's git
            # context, so wptrunner cannot infer the latest revision. Manually
            # add the revision here.
            run_info['revision'] = self.host.git(
                path=tests_root).current_revision()

        # The filename must be `mozinfo.json` for wptrunner to read it from the
        # `--run-info` directory.
        run_info_path = self.fs.join(tmp_dir, 'mozinfo.json')
        with self.fs.open_text_file_for_writing(run_info_path) as file_handle:
            json.dump(run_info, file_handle)

        # Chromium embeds the `//third_party/fontconfig/` library to load fonts.
        # Add a config [0] to discover test fonts copied from
        # `//third_party/test_fonts/`.
        #
        # [0]: https://www.freedesktop.org/software/fontconfig/fontconfig-user.html
        test_fonts_dir = self.port.build_path('test_fonts')
        font_config = textwrap.dedent(f"""\
            <?xml version="1.0"?>
            <!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
            <fontconfig>
              <dir>{test_fonts_dir}</dir>
            </fontconfig>
            """)
        font_config_path = self.fs.join(tmp_dir, 'fontconfig', 'fonts.conf')
        self.fs.maybe_make_directory(self.fs.dirname(font_config_path))
        self.fs.write_text_file(font_config_path, font_config)
        self.host.environ['XDG_CONFIG_HOME'] = tmp_dir

    @contextlib.contextmanager
    def process_and_upload_results(self, runner_options: argparse.Namespace):
        if self.using_upstream_wpt:
            yield
        else:
            with self._processor.stream_results() as events:
                runner_options.log.add_handler(events.put)
                yield
        if runner_options.log_wptreport:
            self._processor.process_wpt_report(
                runner_options.log_wptreport[0].name)
        if (self.port.get_option('show_results')
                and self._processor.num_initial_failures > 0):
            self.port.show_results_html_file(
                self.fs.join(self.port.artifacts_directory(), 'results.html'))
        if self.options.reset_results:
            self._optimize(runner_options)

    def _optimize(self, runner_options: argparse.Namespace):
        blink_tool_path = self.finder.path_from_blink_tools('blink_tool.py')
        command = [
            blink_tool_path,
            'optimize-baselines',
            '--no-manifest-update',
        ]
        if self.options.verbose:
            command.append('--verbose')
        command.extend(
            wpt_url_to_blink_test(f'/{url}') for url in runner_options.include)
        exit_code = BlinkTool(blink_tool_path).main(command)
        if exit_code != exit_codes.OK_EXIT_STATUS:
            logger.error('Failed to optimize baselines during results reset '
                         f'(exit code: {exit_code})')


def _load_entry_point():
    """Import and return a callable that runs wptrunner.

    Returns:
        Callable whose keyword arguments are the namespace corresponding to
        command line options.
    """
    # vpython, not virtualenv, vends third-party packages in chromium/src.
    dummy_venv = Virtualenv(path_finder.get_source_dir(),
                            skip_virtualenv_setup=True)
    return functools.partial(run.run, dummy_venv)


def make_product(port, options):
    name = options.product
    product_cls = product.make_product_registry()[name]
    return product_cls(port, options)


def handle_interrupt_signals():
    def termination_handler(_signum, _unused_frame):
        raise KeyboardInterrupt()

    if sys.platform == 'win32':
        signal.signal(signal.SIGBREAK, termination_handler)
    else:
        signal.signal(signal.SIGTERM, termination_handler)


def parse_arguments(argv):
    parser = command_line.ArgumentParser(usage='%(prog)s [options] [tests]',
                                         description=__doc__.splitlines()[0])
    factory.add_configuration_options_group(
        parser,
        rwt=False,
        product_choices=list(product.make_product_registry()))
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
    # Parameter needed by `WebTestFinder`. TODO(crbug.com/1426296): Port
    # `--no-expectations` to `run_wpt_tests.py`, and skip reporting results when
    # the flag is passed.
    options.no_expectations = False
    return options, args


def _install_xcode(xcode_build_version: str):
    path_finder.add_build_ios_to_sys_path()
    import xcode_util as xcode
    if xcode_build_version:
        try:
            xcode.install_xcode('../../mac_toolchain', xcode_build_version,
                                '../../Xcode.app', '../../Runtime-ios-',
                                product.IOS_VERSION)
        except subprocess.CalledProcessError as e:
            logger.error('Xcode build version %s failed to install: %s ',
                         xcode_build_version, e)
        else:
            logger.info('Xcode build version %s successfully installed.',
                        xcode_build_version)
    else:
        logger.warning('Skip the Xcode installation, no xcode_build_version.')


def _run_with_upstream_wpt(host: Host, argv: List[str]) -> int:
    checkout_path = _checkout_upstream_wpt(host)
    finder = path_finder.PathFinder(host.filesystem)
    command = [
        host.executable,
        finder.path_from_blink_tools('run_wpt_tests.py'),
    ]
    for arg in argv:
        if arg != '--use-upstream-wpt':
            command.append(arg)
    env = {**host.environ, 'PYTHONPATH': checkout_path}
    return host.executive.call(command, env=env)


def _checkout_upstream_wpt(host: Host) -> str:
    # This will leave behind a checkout in `/tmp/wpt` that can be `git fetch`ed
    # later instead of checked out from scratch.
    local_wpt = LocalWPT(host)
    local_wpt.mirror_url = 'https://github.com/web-platform-tests/wpt.git'
    local_wpt.fetch()
    wpt_executable = host.filesystem.join(local_wpt.path, 'wpt')
    rev_list_output = host.executive.run_command(
        [wpt_executable, 'rev-list', '--epoch', '3h'])
    commit = rev_list_output.splitlines()[0]
    host.git(path=local_wpt.path).run(['checkout', commit])
    logger.info('Running against upstream wpt@%s', commit)
    return local_wpt.path


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
    # See: https://github.com/web-platform-tests/wpt/blob/25cd6eb/tools/wptrunner/wptrunner/wptrunner.py
    # This early declaration allow graceful exit when Chromium swarming kill process before wpt starts
    handle_interrupt_signals()

    host = Host()
    exit_code = exit_codes.UNEXPECTED_ERROR_EXIT_STATUS
    try:
        adapter = WPTAdapter.from_args(host, argv)
        if (adapter.product.name == 'chrome_ios'
                and adapter.options.xcode_build_version):
            _install_xcode(adapter.options.xcode_build_version)
        if adapter.options.use_upstream_wpt:
            exit_code = _run_with_upstream_wpt(host, argv)
        else:
            adapter.set_up_derived_options()
            exit_code = adapter.run_tests()
    except KeyboardInterrupt:
        # This clause catches interrupts outside `WPTAdapter.run_tests()`.
        exit_code = exit_codes.INTERRUPTED_EXIT_STATUS
    except Exception as error:
        logger.exception(error)
    logger.info(f'Testing completed. Exit status: {exit_code}')
    return exit_code

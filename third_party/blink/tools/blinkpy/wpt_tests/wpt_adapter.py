# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run web platform tests as described in //docs/testing/run_web_platform_tests.md"""

import argparse
import contextlib
import functools
import json
import logging
import optparse
import signal
import sys
import textwrap
import tempfile
from collections import defaultdict
from typing import List, Optional

from blinkpy.common import exit_codes
from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.tool.blink_tool import BlinkTool
from blinkpy.w3c.local_wpt import LocalWPT
from blinkpy.web_tests import command_line
from blinkpy.web_tests.controllers.web_test_finder import WebTestFinder
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.wpt_tests import product
from blinkpy.wpt_tests.logging import (
    GroupingFormatter,
    MachFormatter,
    StructuredLogAdapter,
)
from blinkpy.wpt_tests.test_loader import TestLoader, wpt_url_to_blink_test
from blinkpy.wpt_tests.wpt_results_processor import WPTResultsProcessor

path_finder.bootstrap_wpt_imports()
import mozlog
from tools.wpt import run
from tools.wpt.virtualenv import Virtualenv
from wptrunner import wptcommandline, wptlogging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger('run_wpt_tests')


class WPTAdapter:
    PORT_NAME_BY_PRODUCT = {
        'android_webview': 'webview',
        'webview': 'webview',
        'chrome_android': 'android',
        'clank': 'android',
    }

    def __init__(self, product, port, options, paths):
        self.product = product
        self.port = port
        self.host = port.host
        self.fs = port.host.filesystem
        self.finder = path_finder.PathFinder(self.fs)
        self.options = options
        self.paths = paths
        self.tests_root = self.fs.join(self.port.web_tests_dir(), 'external',
                                       'wpt')
        self.processor = WPTResultsProcessor(
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
        if options.use_upstream_wpt:
            cls._ensure_value(options, 'no_virtual_tests', True)
            cls._ensure_value(options, 'layout_tests_directory',
                              tempfile.gettempdir())
            options.no_expectations = True
            options.manifest_update = False

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
                and not self.options.use_upstream_wpt):
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

    def create_config_files(self):
        """Create config files based on its template and change the paths to absolute paths.
        """
        if self.options.use_upstream_wpt:
            return
        # create //third_party/blink/web_tests/external/wpt/config.json
        src_config_json = self.finder.path_from_wpt_tests('config.tmpl.json')
        with self.fs.open_text_file_for_reading(src_config_json) as src:
            data = json.load(src)
        data['aliases'] = [{
            **alias,
            'local-dir':
            self.finder.path_from_web_tests(alias['local-dir']),
        } for alias in data['aliases']]
        dst_config_json = self.finder.path_from_wpt_tests('config.json')
        with self.fs.open_text_file_for_writing(dst_config_json) as dst:
            json.dump(data, dst)

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
        runner_options.pause_after_test = False
        runner_options.headless = self.options.headless
        runner_options.trace_categories = self.options.trace_categories

        # Set up logging as early as possible.
        self._set_up_runner_output_options(runner_options)
        self._set_up_runner_config_options(runner_options)
        # TODO(crbug.com/1351820): Find the difference of the host cert ssl set up and make iOS
        # use the same.
        if self.product.name != 'chrome_ios':
            self._set_up_runner_ssl_options(runner_options)
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
        if verbose_level >= 2:
            # Log individual subtest results and `chromedriver` process output.
            runner_options.log_mach_verbose = True
        if verbose_level >= 3:
            # Trace test runner and testdriver events.
            runner_options.log_mach_level = 'debug'
            # Trace individual CDP requests and events.
            runner_options.webdriver_args.append('--verbose')
        else:
            runner_options.webdriver_args.append('--log-level=WARNING')

        if self.options.use_upstream_wpt:
            runner_options.log_wptreport = [
                mozlog.commandline.log_file(
                    self.fs.join(self.port.results_directory(),
                                 'wpt_reports.json'))
            ]
            runner_options.log_wptscreenshot = [
                mozlog.commandline.log_file(
                    self.fs.join(self.port.results_directory(),
                                 'wpt_screenshots.txt'))
            ]

        # Dump `*-{actual,expected}.png` screenshots for all failures like
        # `run_web_tests.py` does. See crbug.com/40947531.
        runner_options.reftest_screenshot = 'fail'
        runner_options.log = wptlogging.setup(dict(vars(runner_options)),
                                              {'grouped': sys.stdout})
        runner_options.log.send_message('show_logs', 'on')
        if self.options.driver_logging:
            runner_options.log.send_message('driver_logging', 'enable')
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

        # Implicitly pass `--enable-blink-features=MojoJS,MojoJSTest`.
        runner_options.mojojs_path = self.port.generated_sources_directory()

        # TODO: RWT has subtle control on how tests are retried. For example
        # there won't be automatic retry of failed tests when they are specified
        # at command line individually. We need such capability for repeat to
        # work correctly.
        runner_options.repeat = self.options.iterations
        runner_options.fully_parallel = self.options.fully_parallel
        runner_options.leak_check = self.options.enable_leak_detection

        if (self.options.enable_sanitizer
                or self.options.configuration == 'Debug'):
            runner_options.sanitizer_enabled = self.options.enable_sanitizer
            runner_options.timeout_multiplier = (
                self.options.timeout_multiplier or 1) * 5
            logger.info(
                f'Using timeout multiplier of {runner_options.timeout_multiplier} '
                + 'because the build is debug or sanitized')
        elif self.options.timeout_multiplier:
            runner_options.timeout_multiplier = self.options.timeout_multiplier

        if self.options.no_expectations:
            # When running with `--no-expectations` or `--use-upstream-wpt`, the
            # goal is to gather data, such as:
            #   * Reports to wpt.fyi (https://github.com/w3c/wptreport)
            #   * Traces
            #   * Code coverage profiles
            #
            # ... not to verify browser code changes. Therefore, test failures
            # should not cause the shard to fail, and there's no need to retry
            # any tests to work around flakiness.
            runner_options.fail_on_unexpected = False
            runner_options.retry_unexpected = 0
            # To speed up testing, don't restart browsers after unexpected
            # failures.
            runner_options.restart_on_unexpected = False
            # Don't add `--run-by-dir=0` because Android currently always uses
            # one emulator or worker.
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

        return tests_to_run, tests_to_skip

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
                'include': sorted(tests),
            }
            subsuite_json[subsuite_name] = subsuite
        return sorted(include_tests), subsuite_json

    def _set_up_runner_tests(self, runner_options, tmp_dir):
        runner_options.manifest_download = False
        runner_options.manifest_update = False
        runner_options.test_types = self.options.test_types
        if not self.options.use_upstream_wpt:
            tests_to_run, tests_to_skip = self._collect_tests()
            include_tests, subsuite_json = self._prepare_tests_for_wptrunner([
                *tests_to_run,
                # Pass skipped tests to wptrunner too so that they're added to
                # the log and test results, but the Blink-side `TestLoader`
                # will generate metadata that disables them.
                *tests_to_skip,
            ])
            if subsuite_json:
                config_path = self.fs.join(tmp_dir, 'subsuite.json')
                with self.fs.open_text_file_for_writing(
                        config_path) as outfile:
                    json.dump(subsuite_json, outfile)
                runner_options.subsuite_file = config_path
                runner_options.subsuites = list(subsuite_json)

            runner_options.include.extend(include_tests)
            runner_options.retry_unexpected = self.options.num_retries

            self.processor.failure_threshold = self.port.max_allowed_failures(
                len(include_tests))
            self.processor.crash_timeout_threshold = self.port.max_allowed_crash_or_timeouts(
                len(include_tests))

            # sharding is done inside wrapper
            runner_options.total_chunks = 1
            runner_options.this_chunk = 1
            runner_options.default_exclude = True

            TestLoader.install(self.port, self._expectations,
                               runner_options.include, tests_to_skip)
        else:
            self._set_up_runner_sharding_options(runner_options)
            # To run tests from command line, prepend the test names with
            # 'external/wpt/'. Other method e.g. test list or test filter
            # is not supported.
            if self.paths:
                for path in self.paths:
                    wpt_dir, path_from_root = self.port.split_wpt_dir(path)
                    if wpt_dir:
                        runner_options.include.append(path_from_root)

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
                self.host.environ[name] = value
            self.host.environ['FONTCONFIG_SYSROOT'] = self.port.build_path()

            runner_options.tests_root = self.tests_root
            runner_options.metadata_root = self.tests_root
            logger.debug(f'Running WPT tests from {self.tests_root}')

            runner_options.run_info = tmp_dir
            runner_options.deps_path = self.fs.join(tmp_dir, 'deps')
            self._initialize_run_info(tmp_dir, self.tests_root)
            if self.options.wrapper:
                runner_options.debug_test = True
                runner_options.binary = self._generate_wrapper_script(
                    tmp_dir, runner_options.binary)

            stack.enter_context(
                self.process_and_upload_results(runner_options))
            self.port.setup_test_run()  # Start Xvfb, if necessary.
            stack.callback(self.port.clean_up_test_run)
            yield runner_options

    def run_tests(self) -> int:
        exit_code = 0
        show_results = self.port.get_option('show_results')
        try:
            with self.test_env() as runner_options:
                run = _load_entry_point(runner_options.deps_path)
                exit_code = 1 if run(**vars(runner_options)) else 0
        except KeyboardInterrupt:
            logger.critical('Harness exited after signal interrupt')
            exit_code = exit_codes.INTERRUPTED_EXIT_STATUS
            show_results = False
        # Write the partial results for an interrupted run. This also ensures
        # the results directory is rotated next time.
        self.processor.copy_results_viewer()
        results_json = self.processor.process_results_json(
            self.port.get_option('json_test_results'))
        if show_results and self.processor.num_initial_failures > 0:
            self.port.show_results_html_file(
                self.fs.join(self.port.artifacts_directory(), 'results.html'))
        return exit_code or int(results_json['num_regressions'] > 0)

    def _initialize_run_info(self, tmp_dir: str, tests_root: str):
        assert self.fs.isdir(tmp_dir), tmp_dir
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
            'virtual_suite': '',  # Needed for non virtual tests
        }
        if self.options.use_upstream_wpt:
            # `run_wpt_tests` does not run in the upstream checkout's git
            # context, so wptrunner cannot infer the latest revision. Manually
            # add the revision here.
            if git := self.host.git(tests_root):
                run_info['revision'] = git.current_revision()

        # The filename must be `mozinfo.json` for wptrunner to read it from the
        # `--run-info` directory.
        run_info_path = self.fs.join(tmp_dir, 'mozinfo.json')
        with self.fs.open_text_file_for_writing(run_info_path) as file_handle:
            json.dump(run_info, file_handle)

    def _generate_wrapper_script(self, tmp_dir: str, binary: str):
        # Generate a temporary script that is substituted for the "binary"
        # passed to `chromedriver`. For example,
        #   --wrapper='rr record' --additional-driver-flag=--switch1
        #
        # is passed as
        #   "binary": "/tmp/.../wrapper.sh",
        #   "args": ["--switch1"],
        #
        # where "wrapper.sh" runs "exec rr record /path/to/chrome $@".
        #
        # This hack exists because passing
        #   "binary": "rr",
        #   "args": ["record", "/path/to/chrome", "--switch1"],
        #
        # won't work. `chromedriver` treats all `args` as switches, which
        # undergo some processing (e.g., normalize args to start with `--`).
        if self.host.platform.is_win():
            args = [*self.options.wrapper, binary, '%*']
            contents = textwrap.dedent(f"""\
                @echo off
                {' '.join(args)}
                """)
            wrapper_path = self.fs.join(tmp_dir, 'wrapper.bat')
        else:
            args = ['exec', *self.options.wrapper, binary, '"$@"']
            contents = textwrap.dedent(f"""\
                #!/bin/sh
                {' '.join(args)}
                """)
            wrapper_path = self.fs.join(tmp_dir, 'wrapper.sh')

        self.fs.write_text_file(wrapper_path, contents)
        self.fs.make_executable(wrapper_path)
        return wrapper_path

    @contextlib.contextmanager
    def process_and_upload_results(self, runner_options: argparse.Namespace):
        with self.processor.stream_results() as events:
            runner_options.log.add_handler(events.put)
            yield
        if runner_options.log_wptreport:
            self.processor.process_wpt_report(
                runner_options.log_wptreport[0].name)
        if runner_options.log_wptscreenshot:
            self.processor.upload_wpt_screenshots(
                runner_options.log_wptscreenshot[0].name)
        if self.options.reset_results:
            self._optimize(runner_options)

    def _optimize(self, runner_options: argparse.Namespace):
        logger.info('Cleaning up redundant baselines...')
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

    def checkout_upstream_wpt(self) -> str:
        """Check out a recent epoch-aligned revision of WPT.

        WPT commits are periodically tagged at different frequencies (e.g.,
        daily, weekly). When generating wpt.fyi results, we should use one of
        these tagged revisions instead of the WPT copy under `web_tests/`, which
        can be at an arbitrary revision. This produces comparable test runs more
        frequently when other wpt.fyi uploaders target the same tagged
        revisions.
        """
        # This will leave behind a checkout in `/tmp/external/wpt` that can be
        # `git fetch`ed later instead of checked out from scratch.
        local_wpt = LocalWPT(self.host, path=self.tests_root)
        local_wpt.mirror_url = 'https://github.com/web-platform-tests/wpt.git'
        # Shallow clone the last `depth` revisions instead of the entire
        # history. The `depth` is an estimated upper bound on the number of
        # merged commits in any 3h window.
        local_wpt.fetch(depth=100)
        wpt_executable = self.host.filesystem.join(local_wpt.path, 'wpt')
        rev_list_output = self.host.executive.run_command(
            [wpt_executable, 'rev-list', '--epoch', '3h'])
        commit = rev_list_output.splitlines()[0]
        git = self.host.git(local_wpt.path)
        assert git, 'cloned repo should have a `git` environment'
        git.run(['checkout', commit])
        # Update wpt manifest immediately after checkout.
        self.port.wpt_manifest('external/wpt')
        logger.info('Running against upstream wpt@%s', commit)


def _load_entry_point(deps_path: str):
    """Import and return a callable that runs wptrunner.

    Arguments:
        deps_path: Scratch directory for installing dependencies. Third-party
            Python packages are not installed here because vpython already
            installs them elsewhere. However, in `--stable` mode, other
            dependencies like `chromedriver` may be downloaded here from Chrome
            for Testing. Therefore, this directory should not be tracked by
            `git`.

    Returns:
        Callable whose keyword arguments are the namespace corresponding to
        command line options.
    """
    dummy_venv = Virtualenv(deps_path, skip_virtualenv_setup=True)
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
    command_line.add_configuration_options_group(
        parser,
        rwt=False,
        product_choices=list(product.make_product_registry()))
    command_line.add_logging_options_group(parser)
    command_line.add_results_options_group(parser, rwt=False)
    command_line.add_testing_options_group(parser, rwt=False)
    command_line.add_android_options_group(parser)
    command_line.add_ios_options_group(parser)

    parser.add_argument('tests',
                        nargs='*',
                        help='Paths to test files or directories to run')
    params = vars(parser.parse_args(argv))
    args = params.pop('tests')
    options = optparse.Values(params)
    return options, args


def _maybe_install_xcode(build_version: str | None):
    if not build_version:
        logger.warning('Skipping Xcode installation (no build version given)')
        return

    path_finder.add_build_ios_to_sys_path()
    import xcode_util
    xcode_util.install_xcode('../../mac_toolchain', build_version,
                             '../../Xcode.app', '../../Runtime-ios-',
                             product.IOS_DEVICE, product.IOS_VERSION)
    logger.info('Xcode build version %s successfully installed.',
                build_version)


def main(argv) -> int:
    # Force log output in utf-8 instead of a locale-dependent encoding. On
    # Windows, this can be cp1252. See: crbug.com/1371195.
    if sys.version_info[:2] >= (3, 7):
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')

    host = Host()
    # Also apply utf-8 mode to python subprocesses.
    host.environ['PYTHONUTF8'] = '1'

    # Convert SIGTERM to be handled as KeyboardInterrupt to handle early termination
    # Same handle is declared later on in wptrunner
    # See: https://github.com/web-platform-tests/wpt/blob/25cd6eb/tools/wptrunner/wptrunner/wptrunner.py
    # This early declaration allow graceful exit when Chromium swarming kill process before wpt starts
    handle_interrupt_signals()

    exit_code = exit_codes.UNEXPECTED_ERROR_EXIT_STATUS
    try:
        adapter = WPTAdapter.from_args(host, argv)
        if adapter.product.name == 'chrome_ios':
            # Xcode needs to be installed early so that `git clone` works.
            _maybe_install_xcode(adapter.options.xcode_build_version)
        if adapter.options.use_upstream_wpt:
            adapter.checkout_upstream_wpt()
        adapter.set_up_derived_options()
        adapter.create_config_files()
        exit_code = adapter.run_tests()
    except KeyboardInterrupt:
        # This clause catches interrupts outside `WPTAdapter.run_tests()`.
        exit_code = exit_codes.INTERRUPTED_EXIT_STATUS
    except Exception as error:
        logger.exception(error)
    logger.info(f'Testing completed. Exit status: {exit_code}')
    return exit_code

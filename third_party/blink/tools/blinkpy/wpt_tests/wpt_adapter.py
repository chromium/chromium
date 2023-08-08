# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run web platform tests for Chromium-related products."""

import argparse
import contextlib
import functools
import json
import logging
import os
import optparse
import re
import signal
import sys
from collections import defaultdict
from datetime import datetime
from typing import List, Optional

from blinkpy.common import exit_codes
from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.w3c.wpt_results_processor import WPTResultsProcessor
from blinkpy.web_tests.controllers.web_test_finder import WebTestFinder
from blinkpy.web_tests.port.base import Port
from blinkpy.web_tests.port import factory
from blinkpy.wpt_tests.product import make_product_registry

path_finder.bootstrap_wpt_imports()

import mozlog
from wptrunner import wptcommandline, wptlogging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger('run_wpt_tests')

UPSTREAM_GIT_URL = 'https://github.com/web-platform-tests/wpt.git'


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

    def log(self, data):
        offset = datetime.now() - self._start
        minutes, seconds = divmod(max(0, offset.total_seconds()), 60)
        hours, minutes = divmod(minutes, 60)
        # A relative timestamp is more useful for comparing event timings than
        # an absolute one.
        timestamp = f'{int(hours):02}:{int(minutes):02}:{int(seconds):02}'
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
        # Do not show test failures again in noninteractive mode. They are
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
        self.finder = path_finder.PathFinder(self.fs)
        self.options = options
        self.paths = paths

    @classmethod
    def from_args(cls,
                  host: Host,
                  args: List[str],
                  port_name: Optional[str] = None):
        options, tests = parse_arguments(args)
        cls._ensure_value(options, 'wpt_only', True)
        cls._ensure_value(options, 'no_virtual_tests', True)
        port = host.port_factory.get(port_name, options)
        product = make_product(port, options)
        return WPTAdapter(product, port, options, tests)

    def set_up_derived_options(self):
        if not self.paths and not self.options.test_list and self.options.smoke is None:
            self.options.smoke = self.port.default_smoke_test_only()
        if self.options.smoke:
            if not self.paths and not self.options.test_list and self.options.num_retries is None:
                # Retry failures 3 times if we're running a smoke test without
                # additional tests. SmokeTests is an explicit list of tests, so we
                # wouldn't retry by default without this special case.
                self.options.num_retries = 3

            if not self.options.test_list:
                self.options.test_list = []
            self.options.test_list.append(self.port.path_to_smoke_tests_file())

        if self.options.gtest_filter:
            self.paths.extend(self.options.gtest_filter.split(':'))

        if not self.options.total_shards and 'GTEST_TOTAL_SHARDS' in self.host.environ:
            self.options.total_shards = int(
                self.port.host.environ['GTEST_TOTAL_SHARDS'])
        if not self.options.shard_index and 'GTEST_SHARD_INDEX' in self.port.host.environ:
            self.options.shard_index = int(
                self.port.host.environ['GTEST_SHARD_INDEX'])

    def log_config(self):
        logger.info(f'Running tests for {self.product.name}')
        logger.info(f'Using port "{self.port.name()}"')
        logger.info(
            f'View the test results at file://{self.port.artifacts_directory()}/results.html'
        )
        logger.info(f'Using {self.port.get_option("configuration")} build')

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
        self._set_up_runner_config_options(runner_options)
        self._set_up_runner_debugging_options(runner_options)
        self._set_up_runner_tests(runner_options, tmp_dir)

        for name, value in self.product.product_specific_options().items():
            self._ensure_value(runner_options, name, value)

        runner_options.webdriver_args.extend(
            self.product.additional_webdriver_args())
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
            runner_options.webdriver_args.extend([
                '--verbose',
                '--log-path=-',
            ])

        runner_options.log_wptreport = [
            mozlog.commandline.log_file(
                self.fs.join(self.port.results_directory(),
                             'wpt_reports.json'))
        ]
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
            runner_options.config = self.finder.path_from_web_tests(
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

        if (self.options.enable_sanitizer
                or self.options.configuration == 'Debug'):
            runner_options.timeout_multiplier = 2
            logger.info('Defaulting to 2x timeout multiplier because '
                        'the build is debug or sanitized')

        if self.options.use_upstream_wpt:
            # when running with upstream, the goal is to get wpt report that can
            # be uploaded to wpt.fyi. We do not really care if tests failed or
            # not. Add '--no-fail-on-unexpected' so that the overall result is
            # success. Add '--no-restart-on-unexpected' to speed up the test. On
            # Android side, we are always running with one emulator or worker,
            # so do not add '--run-by-dir=0'
            runner_options.fail_on_unexpected = False
            runner_options.restart_on_unexpected = False
        else:
            # By default, wpt will treat unexpected passes as errors, so we
            # disable that to be consistent with Chromium CI. Add
            # '--run-by-dir=0' so that tests can be more evenly distributed
            # among workers.
            runner_options.fail_on_unexpected_pass = False
            runner_options.restart_on_unexpected = False
            runner_options.restart_on_new_group = False
            runner_options.run_by_dir = 0
            runner_options.reuse_window = True

        # TODO: repeat_each will restart browsers between tests. Wptrunner's
        # rerun will not restart browsers. Might also need to restart the
        # browser at Wptrunner side.
        runner_options.rerun = self.options.repeat_each

    def _set_up_runner_debugging_options(self, runner_options):
        self.port.set_option_default('use_xvfb',
                                     self.port.get_option('headless'))
        if not self.options.headless:
            logger.info('Not headless; default to 1 worker to avoid '
                        'opening too many windows')
            runner_options.headless = False
            # Force `--pause-after-test`, since it doesn't make sense to run
            # tests headfully without giving a chance for interaction.
            runner_options.pause_after_test = True

    def _prepare_lists(self, test_names):
        tests_to_skip = set()
        for test in test_names:
            if (self.port.virtual_test_skipped_due_to_platform_config(test)
                    or self.port.skipped_due_to_exclusive_virtual_tests(test)):
                tests_to_skip.add(test)
                continue

            if self.options.enable_sanitizer and Port.is_wpt_idlharness_test(
                    test):
                tests_to_skip.update({test})

        tests_to_run = [
            test for test in test_names if test not in tests_to_skip
        ]

        return tests_to_run, tests_to_skip

    def _collect_tests(self):
        finder = WebTestFinder(self.port, self.options)

        try:
            self.paths, all_test_names, _ = finder.find_tests(
                self.paths,
                test_lists=self.options.test_list,
                filter_files=self.options.isolated_script_test_filter_file,
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

        tests_to_run, _ = self._prepare_lists(test_names)

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
                    'binary_args': subsuite_args
                },
                'run_info': {
                    'virtual_suite': subsuite_name
                },
                'include': tests
            }
            subsuite_json[subsuite_name] = subsuite
        return include_tests, subsuite_json

    def _set_up_runner_tests(self, runner_options, tmp_dir):
        if not self.options.use_upstream_wpt:
            include_tests, subsuite_json = self._collect_tests()
            if subsuite_json:
                config_path = self.fs.join(tmp_dir, "subsuite.json")
                with self.fs.open_text_file_for_writing(
                        config_path) as outfile:
                    json.dump(subsuite_json, outfile)
                runner_options.subsuite_file = config_path
                runner_options.subsuites = list(subsuite_json)

            runner_options.include.extend(include_tests)
            runner_options.test_types = self.options.test_types
            runner_options.retry_unexpected = self.options.num_retries

            # sharding is done inside wrapper
            runner_options.total_chunks = 1
            runner_options.this_chunk = 1
            runner_options.default_exclude = True
        else:
            self._set_up_runner_sharding_options(runner_options)
            runner_options.retry_unexpected = 0

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
            string_variables = getattr(self.options, "additional_env_var", [])
            for string_variable in string_variables:
                [name, value] = string_variable.split('=', 1)
                logger.info('Setting environment variable %s to %s', name,
                            value)
                os.environ[name] = value
            if self.options.use_upstream_wpt:
                tests_root = tools_root = self.fs.join(tmp_dir, 'upstream-wpt')
                logger.info('Using upstream wpt, cloning to %s ...',
                            tests_root)
                self.host.executive.run_command([
                    'git', 'clone', UPSTREAM_GIT_URL, tests_root, '--depth=25'
                ])
                self._checkout_3h_epoch_commit(tools_root)
            else:
                tests_root = self.finder.path_from_wpt_tests()
                tools_root = path_finder.get_wpt_tools_wpt_dir()

            runner_options.tests_root = tests_root
            runner_options.tools_root = tools_root
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
            yield runner_options

    def run_tests(self) -> int:
        with self.test_env() as runner_options:
            run = _load_entry_point(runner_options.tools_root)
            with self.process_and_upload_results(runner_options):
                exit_code = run(**vars(runner_options))
                return 1 if exit_code else 0

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
    def process_and_upload_results(self, runner_options):
        artifacts_dir = self.port.artifacts_directory()
        processor = WPTResultsProcessor(
            self.fs,
            self.port,
            artifacts_dir=artifacts_dir,
            failure_threshold=self.port.get_option('exit_after_n_failures'),
            crash_timeout_threshold=self.port.get_option(
                'exit_after_n_crashes_or_timeouts'))
        with processor.stream_results() as events:
            runner_options.log.add_handler(events.put)
            yield
        processor.process_wpt_report(runner_options.log_wptreport[0].name)
        processor.process_results_json(
            self.port.get_option('json_test_results'))
        processor.copy_results_viewer()
        if self.port.get_option(
                'show_results') and processor.num_regressions > 0:
            self.port.show_results_html_file(
                self.fs.join(artifacts_dir, 'results.html'))


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


def make_product(port, options):
    name = options.product
    product_cls = make_product_registry()[name]
    return product_cls(port, options)


def handle_interrupt_signals():
    def termination_handler(_signum, _unused_frame):
        raise KeyboardInterrupt()

    if sys.platform == 'win32':
        signal.signal(signal.SIGBREAK, termination_handler)
    else:
        signal.signal(signal.SIGTERM, termination_handler)


def parse_arguments(argv):
    parser = argparse.ArgumentParser(
        usage='%(prog)s [options] [tests]',
        description=('Runs WPT tests as described in '
                     '//docs/testing/web_platform_tests_wptrunner.md'))
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
    # Directly tie Xvfb usage to headless mode. Xvfb can supercede a real X
    # server and therefore should never be started in `--no-headless` mode.
    # Conversely, the default headless mode should always start Xvfb.
    options.use_xvfb = options.headless
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
    # See: https://github.com/web-platform-tests/wpt/blob/25cd6eb/tools/wptrunner/wptrunner/wptrunner.py
    # This early declaration allow graceful exit when Chromium swarming kill process before wpt starts
    handle_interrupt_signals()

    exit_code = exit_codes.UNEXPECTED_ERROR_EXIT_STATUS
    try:
        host = Host()
        adapter = WPTAdapter.from_args(host, argv)
        adapter.set_up_derived_options()
        exit_code = adapter.run_tests()
    except KeyboardInterrupt:
        logger.critical('Harness exited after signal interrupt')
        exit_code = exit_codes.INTERRUPTED_EXIT_STATUS
    except Exception as error:
        logger.exception(error)
    logger.info(f'Testing completed. Exit status: {exit_code}')
    return exit_code

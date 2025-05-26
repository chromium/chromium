# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Logging helpers for rendering test results and other events to stdio.

wptrunner uses `mozlog` (a structured logging framework) for reporting test
results. See the docs [0] and source code [1] for details.

[0]: https://firefox-source-docs.mozilla.org/mozbase/mozlog.html
[1]: https://github.com/web-platform-tests/wpt/tree/master/tools/third_party_modified/mozlog/mozlog
"""

from datetime import datetime
import logging

from blinkpy.common import path_finder

path_finder.bootstrap_wpt_imports()
import mozlog


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
        timestamp = datetime.now().isoformat(sep=' ', timespec='milliseconds')
        # Place mandatory fields first so that logs are vertically aligned as
        # much as possible.
        message = f'{timestamp} {data["level"]} {data["message"]}'
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
            logging.Formatter(
                fmt='%(asctime)s.%(msecs)03d %(levelname)s %(message)s',
                datefmt='%Y-%m-%d %H:%M:%S'))

    def emit(self, record):
        log = getattr(self._logger, record.levelname.lower(),
                      self._logger.debug)
        try:
            log(record.getMessage(),
                component=record.name,
                exc_info=record.exc_info)
        except mozlog.structuredlog.LoggerShutdownError:
            self._fallback_handler.emit(record)

# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Process WPT results for upload to ResultDB."""

import argparse
import json
import logging
import os

from blinkpy.common.system.log_utils import configure_logging

_log = logging.getLogger(__name__)


class WPTResultsProcessor(object):
    def __init__(self, host, web_tests_dir='', artifacts_dir=''):
        self.host = host
        self.fs = self.host.filesystem
        self.web_tests_dir = web_tests_dir
        self.artifacts_dir = artifacts_dir

    def main(self, argv=None):
        options = self.parse_args(argv)

        logging_level = logging.DEBUG if options.verbose else logging.INFO
        configure_logging(logging_level=logging_level, include_time=True)

        self.web_tests_dir = options.web_tests_dir
        self.artifacts_dir = options.artifacts_dir

        if options.wpt_report:
            self.process_wpt_report(options.wpt_report)
        else:
            _log.debug('No wpt report to process')

    def parse_args(self, argv):
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument(
            '-v',
            '--verbose',
            action='store_true',
            help='log extra details helpful for debugging',
        )
        parser.add_argument(
            '--web-tests-dir',
            required=True,
            help='path to the web tests root',
        )
        parser.add_argument(
            '--artifacts-dir',
            required=True,
            help='path to a directory to write artifacts to',
        )
        parser.add_argument(
            '--wpt-report',
            help=('path to the wptreport file '
                  '(created with `wpt run --log-wptreport=...`)'),
        )
        return parser.parse_args(argv)

    def _get_wpt_revision(self):
        version_path = os.path.join(self.web_tests_dir, 'external', 'Version')
        target = 'Version:'
        with self.fs.open_text_file_for_reading(version_path) as version_file:
            for line in version_file:
                if line.startswith(target):
                    rev = line[len(target):].strip()
                    return rev
        return None

    def process_wpt_report(self, report_path):
        """Process a wpt report for possible eventual upload to wpt.fyi."""
        with self.fs.open_text_file_for_reading(report_path) as report_file:
            report = json.load(report_file)
        rev = self._get_wpt_revision()
        # Update with upstream revision
        if rev:
            report['run_info']['revision'] = rev
        report_filename = os.path.basename(report_path)
        output_path = os.path.join(self.artifacts_dir, report_filename)
        with self.fs.open_text_file_for_writing(output_path) as report_file:
            json.dump(report, report_file)
        _log.info('Processed wpt report %s and wrote to %s', report_path,
                  output_path)
        return output_path

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sends notifications for expired Virtual Test Suites in
web_tests/VirtualTestSuites
"""

import argparse
import datetime
import logging
from typing import Optional

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.buganizer import (
    BuganizerClient,
    BuganizerError,
    BuganizerIssue,
    Priority,
)
from blinkpy.web_tests.port.base import Port, VirtualTestSuite

_log = logging.getLogger(__name__)

BLINK_INFRA_COMPONENT_ID = '1456928'

BUGAINZER_DESCRIPTION_TEMPLATE = '''\
This virtual test suite '{prefix}' expired on {expires}.

If the virtual test suite is no longer needed, please remove the entry from
//third_party/blink/web_tests/VirtualTestSuites.
Otherwise, update the expiration date.

Note: Test suite owners are automatically added to CC.
'''


class VTSNotifier:

    def __init__(self,
                 host,
                 buganizer_client: Optional[BuganizerClient] = None):
        self.host = host
        self.port: Port = host.port_factory.get()
        self.buganizer_client = buganizer_client or BuganizerClient()
        self._dry_run = False

    def run(self, argv=None):
        options = self.parse_args(argv)
        configure_logging(
            logging_level=logging.DEBUG if options.verbose else logging.INFO,
            include_time=True)
        self._dry_run = options.dry_run
        virtual_test_suites = self.port.virtual_test_suites()
        _log.info(
            f'Processing {len(virtual_test_suites)} virtual test suites.')
        expired_virtual_test_suites = list(
            filter(self.check_expired_vts, virtual_test_suites))
        _log.info(f'Processing {len(expired_virtual_test_suites)} '
                  'expired virtual test suites.')
        for expired_virtual_test_suite in expired_virtual_test_suites:
            bug = self.create_draft_bug(expired_virtual_test_suite)
            self.process_draft_bug(bug)

    def parse_args(self, argv):
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument(
            '-v',
            '--verbose',
            action='store_true',
            help='Log extra details that may be helpful when debugging.')
        parser.add_argument(
            '--dry-run',
            action='store_true',
            help='See what would be done without actually creating bug.')
        return parser.parse_args(argv)

    def process_draft_bug(self, bug):
        """Issue new bug if bug with the same title does not already exist"""
        try:
            # Checks if a bug with the same title exists on Buganizer
            existing_bugs = self.buganizer_client.GetIssueList(
                f'title:"{bug.title}"')
            if existing_bugs:
                _log.info(f'Bug already created: {existing_bugs[0].link}')
            elif self._dry_run:
                _log.info('[dry_run] would have created the following bug:\n'
                          f'{bug}')
            else:
                bug = self.buganizer_client.NewIssue(bug)
                _log.info(f'Filed bug: {bug.link}')
        except BuganizerError as e:
            _log.exception(f'Failed to process bug: {e}')

    def create_draft_bug(self, vts: VirtualTestSuite) -> BuganizerIssue:
        """Construct a bug for taking action on an expired VirtualTestSuite."""
        _log.debug(f'Expired VTS found: {vts.prefix=}.')
        title = ('[VT Expire Notice] Virtual test suite '
                 f'{vts.prefix} expired on {vts.expires}, '
                 'action required')
        description = BUGAINZER_DESCRIPTION_TEMPLATE.format(
            prefix=vts.prefix, expires=vts.expires)
        bug = BuganizerIssue(title=title,
                             description=description,
                             component_id=BLINK_INFRA_COMPONENT_ID,
                             cc=vts.owners or [],
                             priority=Priority.P1)
        return bug

    def check_expired_vts(self, vts: VirtualTestSuite):
        """Check if the virtual test suite is expired

        Returns:
            True if the VTS has expired, False otherwise.
        """
        if not vts.expires or vts.expires == 'never':
            return False
        for date_format in ['%b %d, %Y', '%B %d, %Y']:
            try:
                expiration_date = datetime.datetime.strptime(
                    vts.expires, date_format)
                if datetime.datetime.today() >= expiration_date:
                    return True
            except ValueError:
                continue
        return False

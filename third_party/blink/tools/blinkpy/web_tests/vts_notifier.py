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

from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.buganizer import (
    BuganizerClient,
    BuganizerError,
    BuganizerIssue,
    Priority,
)
from blinkpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor
from blinkpy.web_tests.port.base import Port, VirtualTestSuite

_log = logging.getLogger(__name__)

# component ID for Default DIR_METADTA of wpt_internal/* and blink/*
BLINK_COMPONENT_ID = '1456407'
# component ID for Default DIR_METADTA of external/wpt/*
BLINK_INFRA_ECOSYSTEM_COMPONENT_ID = '1456176'
DEFAULT_COMPONENT_IDS = [
    BLINK_INFRA_ECOSYSTEM_COMPONENT_ID, BLINK_COMPONENT_ID
]

BUGAINZER_DESCRIPTION_TEMPLATE = '''\
This is a reminder that the virtual test suite, **{prefix}**, expired on **{expires}**.

Expired virtual test suites that don't add meaningful coverage unnecessarily slow down web tests on CQ.

Please review this virtual test suite, [**{prefix}**](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/VirtualTestSuites?q=%22{prefix}%22%20file:third_party%2Fblink%2Fweb_tests%2FVirtualTestSuites), and take one of the following actions:

1. Delete: If the virtual test suite is no longer needed, please remove the entry from [//third_party/blink/web_tests/VirtualTestSuites](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/VirtualTestSuites) ([Example CL](https://crrev.com/c/5741322))

2. Update: If the virtual test suite is still in use, update the expiration date at **{prefix}**. ([Example CL] ((https://crrev.com/c/5104889)))

Note:
* Test suite owners are automatically added to CC.
* If you do not own this test suite, please reassign it to the appropriate team or to `Blink>Infra` for triage.
'''
WATCHERS = ['ansung@google.com']


class VTSNotifier:

    def __init__(self,
                 host,
                 buganizer_client: Optional[BuganizerClient] = None):
        self.host = host
        self.port: Port = host.port_factory.get()
        self.buganizer_client = buganizer_client or BuganizerClient()
        self.path_finder = PathFinder(self.host.filesystem)
        self.owners_extractor = DirectoryOwnersExtractor(host)
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
        component_id = self.resolve_component_id(vts)
        bug = BuganizerIssue(title=title,
                             description=description,
                             component_id=component_id,
                             cc=(vts.owners or []) + WATCHERS,
                             priority=Priority.P1)
        return bug

    def resolve_component_id(self, vts: VirtualTestSuite) -> str:
        """Find a component ID to file a bug to for an expired virtual suite.

        `VTSNotifier` will try the following in order to find a relevant Buganizer
        component:
        1. The component in `virtual/x/DIR_METADATA`.
        2. The component in the `DIR_METADATA` that the suite's bases roll up to
           that is not `Blink>Infra>Ecosystem` or `Blink`
        3. The component for `Blink>Infra>Ecosystem`.
        """
        vts_dir = self.host.filesystem.join(self.port.web_tests_dir(),
                                            vts.full_prefix)
        _log.info(f'Resolving component ID for {vts.full_prefix}.')
        component_id = self.read_component_from_metadata(vts_dir)
        if component_id and component_id not in DEFAULT_COMPONENT_IDS:
            return component_id
        return self.find_first_base_component_id(vts.bases)

    def read_component_from_metadata(self, dir_path) -> str:
        dir_metadata = self.owners_extractor.read_dir_metadata(dir_path)
        if dir_metadata and dir_metadata.buganizer_public_component:
            return dir_metadata.buganizer_public_component
        return ''

    def find_first_base_component_id(self, bases) -> str:
        """Find a component ID from the bases of a virtual test suite

        This method iterates through the bases of a virtual test suite and
        returns the component ID from the `DIR_METADATA` file that is not one
        of the default component IDs.
        """
        for base in bases:
            test_path = self.path_finder.path_from_web_tests(base)
            test_directory = (test_path
                              if self.host.filesystem.isdir(test_path) else
                              self.host.filesystem.dirname(test_path))
            component_id = self.read_component_from_metadata(test_directory)
            if component_id and component_id not in DEFAULT_COMPONENT_IDS:
                _log.info(f'Using base, {base}, to resolve component ID to '
                          f'{component_id}.')
                return component_id
        _log.info('No DIR_METADATA found. Using Blink component.')
        return BLINK_COMPONENT_ID

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

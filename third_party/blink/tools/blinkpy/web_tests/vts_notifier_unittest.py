# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import unittest
from unittest import mock

from blinkpy.common.host_mock import MockHost
from blinkpy.w3c.buganizer import (
    BuganizerClient,
    BuganizerError,
    BuganizerIssue,
    Priority,
    Severity,
    Status,
)
from blinkpy.w3c.directory_owners_extractor import WPTDirMetadata
from blinkpy.web_tests.port.base import VirtualTestSuite
from blinkpy.web_tests.vts_notifier import (BLINK_COMPONENT_ID, VTSNotifier,
                                            WATCHERS)


class VTSNotifierTest(unittest.TestCase):

    def setUp(self):
        super().setUp()
        self.host = MockHost()
        self.host.filesystem.maybe_make_directory(
            '/mock-checkout/third_party/blink/web_tests/wpt_internal')
        mock_buganizer_client = mock.create_autospec(BuganizerClient)
        self.notifier = VTSNotifier(self.host,
                                    buganizer_client=mock_buganizer_client)
        fake_datetime = mock.Mock(wraps=datetime.datetime)
        fake_datetime.now.return_value = datetime.datetime(2024, 1, 1)
        self.patcher = mock.patch(
            'blinkpy.web_tests.vts_notifier.datetime.datetime', fake_datetime)
        self.patcher.start()

    def tearDown(self):
        self.patcher.stop()

    def create_mock_vts(self,
                        prefix='test-vts',
                        expires=None,
                        owners=[],
                        bases=['wpt_internal']):
        return VirtualTestSuite(prefix=prefix,
                                platforms=['Linux'],
                                bases=bases,
                                args=['--enable-features=FakeFeature'],
                                expires=expires,
                                owners=owners)

    def test_run_vts(self):
        # Expired (call GetIssueList and NewIssue)
        vts1 = self.create_mock_vts(prefix='fake-vts-1',
                                    expires='Jan 01, 2000',
                                    owners=[])
        # Not expired
        vts2 = self.create_mock_vts(prefix='fake-vts-2',
                                    expires='Jan 01, 3000',
                                    owners=[])
        # Expired + Duplicate (call GetIssueList and NewIssue)
        vts3 = self.create_mock_vts(prefix='fake-vts-3',
                                    expires='Jan 01, 2000',
                                    owners=[])
        # Expired + Owner defined (call GetIssueList)
        vts4 = self.create_mock_vts(prefix='fake-vts-4',
                                    expires='Jan 01, 2000',
                                    owners=['abc@mail.com'])

        self.notifier.port = mock.Mock(wraps=self.notifier.port)
        self.notifier.port.virtual_test_suites.return_value = [
            vts1, vts2, vts3, vts4
        ]
        self.notifier.buganizer_client.GetIssueList.side_effect = [
            [],
            [],
            [mock.Mock(BuganizerIssue, link='123')],
            [],
        ]

        self.notifier.run(argv=[])
        self.notifier.buganizer_client.GetIssueList.assert_has_calls([
            mock.call('title:"[VT Expire Notice] Virtual test suite '
                      'fake-vts-1 expired on Jan 01, 2000, action required"'),
            mock.call('title:"[VT Expire Notice] Virtual test suite '
                      'fake-vts-3 expired on Jan 01, 2000, action required"'),
            mock.call('title:"[VT Expire Notice] Virtual test suite '
                      'fake-vts-4 expired on Jan 01, 2000, action required"')
        ])
        self.assertEqual(self.notifier.buganizer_client.NewIssue.call_count, 2)

    def test_create_draft_bug_without_owner(self):
        mock_vts = self.create_mock_vts(expires='Jan 01, 2000')
        draft_bug = self.notifier.create_draft_bug(mock_vts)
        self.assertEqual(
            draft_bug.title,
            '[VT Expire Notice] Virtual test suite '
            'test-vts expired on Jan 01, 2000, action required',
        )
        self.assertIn(mock_vts.prefix, draft_bug.description)
        self.assertIn(mock_vts.expires, draft_bug.description)
        self.assertEqual(draft_bug.status, Status.NEW)
        self.assertEqual(draft_bug.component_id, BLINK_COMPONENT_ID)
        self.assertEqual(draft_bug.cc, WATCHERS)
        self.assertEqual(draft_bug.priority, Priority.P1)
        self.assertEqual(draft_bug.severity, Severity.S4)

    def test_create_draft_bug_with_owner(self):
        test_owners = ['testA@mail.com', 'testB@mail.com']
        mock_vts = self.create_mock_vts(expires='Jan 01, 2000',
                                        owners=test_owners)
        draft_bug = self.notifier.create_draft_bug(mock_vts)
        self.assertEqual(
            draft_bug.title,
            '[VT Expire Notice] Virtual test suite '
            'test-vts expired on Jan 01, 2000, action required',
        )
        self.assertIn(mock_vts.prefix, draft_bug.description)
        self.assertIn(mock_vts.expires, draft_bug.description)
        self.assertEqual(draft_bug.status, Status.NEW)
        self.assertEqual(draft_bug.component_id, BLINK_COMPONENT_ID)
        self.assertEqual(draft_bug.cc, test_owners + WATCHERS)
        self.assertEqual(draft_bug.priority, Priority.P1)
        self.assertEqual(draft_bug.severity, Severity.S4)

    def test_process_draft_bug_with_no_existing_bug(self):
        draft_bug = self.notifier.create_draft_bug(
            self.create_mock_vts(expires='Jan 01, 2000'))
        self.notifier.buganizer_client.GetIssueList.return_value = []
        self.notifier.process_draft_bug(draft_bug)
        self.notifier.buganizer_client.NewIssue.assert_called_once_with(
            draft_bug)

    def test_process_draft_bug_with_existing_bug(self):
        draft_bug = self.notifier.create_draft_bug(
            self.create_mock_vts(expires='Jan 01, 2000'))
        self.notifier.buganizer_client.GetIssueList.return_value = [
            draft_bug,
            draft_bug,
        ]
        self.notifier.process_draft_bug(draft_bug)
        self.notifier.buganizer_client.NewIssue.assert_not_called()

    def test_process_draft_bug_with_exception(self):
        draft_bug = self.notifier.create_draft_bug(
            self.create_mock_vts(expires='Jan 01, 2000'))
        self.notifier.buganizer_client.GetIssueList.side_effect = (
            BuganizerError('timeout'))
        self.notifier.process_draft_bug(draft_bug)
        self.notifier.buganizer_client.NewIssue.assert_not_called()

    def test_check_expired_vts_expired(self):
        vts = self.create_mock_vts(expires='Jan 1, 2000')
        self.assertTrue(self.notifier.check_expired_vts(vts))

    def test_check_expired_vts_not_expired(self):
        vts = self.create_mock_vts(expires='Jan 1, 3000')
        self.assertFalse(self.notifier.check_expired_vts(vts))

    def test_check_expired_vts_alternate_dateformat(self):
        vts = self.create_mock_vts(expires='January 1, 2000')
        self.assertTrue(self.notifier.check_expired_vts(vts))

    def test_check_expired_vts_invalid_dateformat(self):
        vts = self.create_mock_vts(expires='3000/01/01')
        self.assertFalse(self.notifier.check_expired_vts(vts))

    def test_resolve_component_id_with_no_dir_metadata(self):
        bases = ['1', '2', '3', '4']
        for base in bases:
            base_path = self.notifier.path_finder.path_from_web_tests(base)
            self.notifier.host.filesystem.maybe_make_directory(base_path)
        mock_vts = self.create_mock_vts(bases=bases)
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata',
                               return_value=None) as mock_owner_extractor:
            self.assertEqual(self.notifier.resolve_component_id(mock_vts),
                             BLINK_COMPONENT_ID)
            # check virtual/{prefix} and all bases before defaulting to
            # BLINK_COMPONENT_ID
            mock_calls = [
                mock.call(self.notifier.path_finder.path_from_web_tests(path))
                for path in ['virtual/test-vts/', *bases]
            ]
            mock_owner_extractor.assert_has_calls(mock_calls)

    def test_resolve_component_id_with_directory_as_base(self):
        base_path = self.notifier.path_finder.path_from_web_tests(
            'wpt_internal/a')
        self.notifier.host.filesystem.maybe_make_directory(base_path)
        mock_vts = self.create_mock_vts(bases=['wpt_internal/a'])
        dir_metadata = {
            base_path: WPTDirMetadata(buganizer_public_component='123')
        }
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata', dir_metadata.get):
            self.assertEqual(self.notifier.resolve_component_id(mock_vts),
                             '123')

    def test_resolve_component_id_with_file_as_base(self):
        base_path = self.notifier.path_finder.path_from_web_tests(
            'wpt_internal/a')
        self.notifier.host.filesystem.maybe_make_directory(base_path)
        mock_vts = self.create_mock_vts(bases=['wpt_internal/a/file'])
        dir_metadata = {
            base_path: WPTDirMetadata(buganizer_public_component='123')
        }
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata', dir_metadata.get):
            self.assertEqual(self.notifier.resolve_component_id(mock_vts),
                             '123')

    def test_resolve_component_id_with_dir_metadata_from_second_base(self):
        mock_vts = self.create_mock_vts(bases=['foo', 'bar'])
        dir_metadata = {
            self.notifier.path_finder.path_from_web_tests('foo'):
            WPTDirMetadata(buganizer_public_component=BLINK_COMPONENT_ID),
            self.notifier.path_finder.path_from_web_tests('bar'):
            WPTDirMetadata(buganizer_public_component='123'),
        }
        self.notifier.host.filesystem.write_text_file(
            self.notifier.path_finder.path_from_web_tests(
                'foo', 'DIR_METADATA'), '')
        self.notifier.host.filesystem.write_text_file(
            self.notifier.path_finder.path_from_web_tests(
                'bar', 'DIR_METADATA'), '')
        # bar/DIR_METADATA will be used since foo uses default component ID
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata', dir_metadata.get):
            self.assertEqual(self.notifier.resolve_component_id(mock_vts),
                             '123')

    def test_resolve_component_id_with_dir_metadata_from_vts_dir(self):
        mock_vts = self.create_mock_vts(
            bases=['wpt_internal', 'wpt_internal/nested'])
        dir_metadata = {
            self.notifier.path_finder.path_from_web_tests('virtual/test-vts/'):
            WPTDirMetadata(buganizer_public_component='123')
        }
        self.notifier.host.filesystem.write_text_file(
            self.notifier.path_finder.path_from_web_tests(
                'virtual', 'test-vts', 'DIR_METADATA'), '')
        self.notifier.host.filesystem.write_text_file(
            self.notifier.path_finder.path_from_web_tests(
                'wpt_internal/nested', 'DIR_METADATA'), '')
        with mock.patch.object(self.notifier.owners_extractor,
                               'read_dir_metadata', dir_metadata.get):
            self.assertEqual(self.notifier.resolve_component_id(mock_vts),
                             '123')

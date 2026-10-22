# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import textwrap
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import RELATIVE_WPT_TESTS
from blinkpy.w3c.common import (
    CHANGE_ID_KEY,
    LINK_KEY,
    REVIEWED_ON_KEY,
    is_basename_skipped,
    is_file_exportable,
    is_testharness_baseline,
    parse_footers,
    read_credentials,
)


class CommonTest(unittest.TestCase):
    def setUp(self):
        host = MockHost()
        self.project_config = host.project_config

    def test_get_credentials_empty(self):
        host = MockHost()
        host.filesystem.write_text_file('/tmp/credentials.json', '{}')
        self.assertEqual(read_credentials(host, '/tmp/credentials.json'), {})

    def test_get_credentials_none(self):
        self.assertEqual(read_credentials(MockHost(), None), {})

    def test_get_credentials_gets_values_from_environment(self):
        host = MockHost()
        host.environ.update(
            {
                'GH_USER': 'user-github',
                'GH_TOKEN': 'pass-github',
                'GERRIT_USER': 'user-gerrit',
                'GERRIT_TOKEN': 'pass-gerrit',
                'UNUSED_VALUE': 'foo',
            }
        )
        self.assertEqual(
            read_credentials(host, None),
            {
                'GH_USER': 'user-github',
                'GH_TOKEN': 'pass-github',
                'GERRIT_USER': 'user-gerrit',
                'GERRIT_TOKEN': 'pass-gerrit',
            },
        )

    def test_get_credentials_gets_values_from_file(self):
        host = MockHost()
        host.filesystem.write_text_file(
            '/tmp/credentials.json',
            json.dumps(
                {
                    'GH_USER': 'user-github',
                    'GH_TOKEN': 'pass-github',
                    'GERRIT_USER': 'user-gerrit',
                    'GERRIT_TOKEN': 'pass-gerrit',
                }
            ),
        )
        self.assertEqual(
            read_credentials(host, '/tmp/credentials.json'),
            {
                'GH_USER': 'user-github',
                'GH_TOKEN': 'pass-github',
                'GERRIT_USER': 'user-gerrit',
                'GERRIT_TOKEN': 'pass-gerrit',
            },
        )

    def test_get_credentials_choose_file_over_environment(self):
        host = MockHost()
        host.environ.update(
            {
                'GH_USER': 'user-github-from-env',
                'GH_TOKEN': 'pass-github-from-env',
                'GERRIT_USER': 'user-gerrit-from-env',
                'GERRIT_TOKEN': 'pass-gerrit-from-env',
            }
        )
        host.filesystem.write_text_file(
            '/tmp/credentials.json',
            json.dumps(
                {
                    'GH_USER': 'user-github-from-json',
                    'GH_TOKEN': 'pass-github-from-json',
                }
            ),
        )
        self.assertEqual(
            read_credentials(host, '/tmp/credentials.json'),
            {
                'GH_USER': 'user-github-from-json',
                'GH_TOKEN': 'pass-github-from-json',
            },
        )

    def test_is_testharness_baseline(self):
        self.assertTrue(is_testharness_baseline('fake-test-expected.txt'))
        self.assertTrue(
            is_testharness_baseline('external/wpt/fake-test-expected.txt')
        )
        self.assertTrue(
            is_testharness_baseline('/tmp/wpt/fake-test-expected.txt')
        )
        self.assertFalse(is_testharness_baseline('fake-test-expected.html'))
        self.assertFalse(
            is_testharness_baseline('external/wpt/fake-test-expected.html')
        )

    def test_is_basename_skipped(self):
        self.assertTrue(is_basename_skipped('MANIFEST.json'))
        self.assertTrue(is_basename_skipped('DIR_METADATA'))
        self.assertTrue(is_basename_skipped('OWNERS'))
        self.assertTrue(is_basename_skipped('reftest.list'))
        self.assertTrue(is_basename_skipped('.gitignore'))
        self.assertFalse(is_basename_skipped('something.json'))

    def test_is_basename_skipped_asserts_basename(self):
        with self.assertRaises(AssertionError):
            is_basename_skipped('third_party/fake/OWNERS')

    def test_is_file_exportable(self):
        self.assertTrue(
            is_file_exportable(
                RELATIVE_WPT_TESTS + 'html/fake-test.html', self.project_config
            )
        )
        self.assertFalse(
            is_file_exportable(
                RELATIVE_WPT_TESTS + 'html/fake-test-expected.txt',
                self.project_config,
            )
        )
        self.assertFalse(
            is_file_exportable(
                RELATIVE_WPT_TESTS + 'MANIFEST.json', self.project_config
            )
        )
        self.assertFalse(
            is_file_exportable(
                RELATIVE_WPT_TESTS + 'dom/OWNERS', self.project_config
            )
        )
        self.assertFalse(
            is_file_exportable(
                RELATIVE_WPT_TESTS + 'dom/DIR_METADATA', self.project_config
            )
        )
        self.assertTrue(
            is_file_exportable(
                RELATIVE_WPT_TESTS + 'webdriver/tests/pytest.ini',
                self.project_config,
            )
        )
        self.assertFalse(
            is_file_exportable(
                RELATIVE_WPT_TESTS + 'html/tools/PRESUBMIT.py',
                self.project_config,
            )
        )

    def test_is_file_exportable_asserts_path(self):
        # Rejects basenames.
        with self.assertRaises(AssertionError):
            is_file_exportable('MANIFEST.json', self.project_config)
        # Rejects files not in Chromium WPT.
        with self.assertRaises(AssertionError):
            is_file_exportable('third_party/fake/OWNERS', self.project_config)
        # Rejects absolute paths.
        with self.assertRaises(AssertionError):
            is_file_exportable(
                '/mock-checkout/' + RELATIVE_WPT_TESTS + 'OWNERS',
                self.project_config,
            )

    def test_parse_footers(self):
        message = textwrap.dedent("""\
            hashtag: Subject

            Body with some text.
            https://example.com/standalone-url

            Change-Id: I00c0ffee
            Change-Id: I00decade

            Reviewed-on: https://chromium-review.googlesource.com/123
            Link: https://example.com/1
            """)
        footers = parse_footers(message)
        self.assertEqual(
            footers.get(CHANGE_ID_KEY),
            'I00decade',
        )
        # Verify case-insensitive lookups.
        self.assertEqual(
            footers.get('change-id'),
            'I00decade',
        )
        self.assertEqual(
            footers['CHANGE-ID'],
            'I00decade',
        )
        self.assertEqual(
            footers.get(REVIEWED_ON_KEY),
            'https://chromium-review.googlesource.com/123',
        )
        self.assertEqual(
            footers.get(LINK_KEY),
            'https://example.com/1',
        )
        # Standalone URLs and subject line Gerrit hashtags are excluded.
        self.assertNotIn('https', footers)
        self.assertNotIn('hashtag', footers)
        self.assertIsNone(footers.get('Nonexistent'))

    def test_parse_footers_empty(self):
        self.assertEqual(dict(parse_footers('')), {})
        self.assertEqual(
            parse_footers('Change-Id: 88').get(CHANGE_ID_KEY),
            '88',
        )

# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.w3c.common import (
    read_credentials,
    is_testharness_baseline,
    is_basename_skipped,
    is_file_exportable,
    CHROMIUM_WPT_DIR
)


class CommonTest(unittest.TestCase):

    def test_get_credentials_empty(self):
        host = MockHost()
        host.filesystem.write_text_file('/tmp/credentials.json', '{}')
        self.assertEqual(read_credentials(host, '/tmp/credentials.json'), {})

    def test_get_credentials_none(self):
        self.assertEqual(read_credentials(MockHost(), None), {})

    def test_get_credentials_gets_values_from_environment(self):
        host = MockHost()
        host.environ.update({
            'GH_USER': 'user-github',
            'GH_TOKEN': 'pass-github',
            'GERRIT_USER': 'user-gerrit',
            'GERRIT_TOKEN': 'pass-gerrit',
            'UNUSED_VALUE': 'foo',
        })
        self.assertEqual(
            read_credentials(host, None),
            {
                'GH_USER': 'user-github',
                'GH_TOKEN': 'pass-github',
                'GERRIT_USER': 'user-gerrit',
                'GERRIT_TOKEN': 'pass-gerrit',
            })

    def test_get_credentials_gets_values_from_file(self):
        host = MockHost()
        host.filesystem.write_text_file(
            '/tmp/credentials.json',
            json.dumps({
                'GH_USER': 'user-github',
                'GH_TOKEN': 'pass-github',
                'GERRIT_USER': 'user-gerrit',
                'GERRIT_TOKEN': 'pass-gerrit',
            }))
        self.assertEqual(
            read_credentials(host, '/tmp/credentials.json'),
            {
                'GH_USER': 'user-github',
                'GH_TOKEN': 'pass-github',
                'GERRIT_USER': 'user-gerrit',
                'GERRIT_TOKEN': 'pass-gerrit',
            })

    def test_get_credentials_choose_file_over_environment(self):
        host = MockHost()
        host.environ.update({
            'GH_USER': 'user-github-from-env',
            'GH_TOKEN': 'pass-github-from-env',
            'GERRIT_USER': 'user-gerrit-from-env',
            'GERRIT_TOKEN': 'pass-gerrit-from-env',
        })
        host.filesystem.write_text_file(
            '/tmp/credentials.json',
            json.dumps({
                'GH_USER': 'user-github-from-json',
                'GH_TOKEN': 'pass-github-from-json',
            }))
        self.assertEqual(
            read_credentials(host, '/tmp/credentials.json'),
            {
                'GH_USER': 'user-github-from-json',
                'GH_TOKEN': 'pass-github-from-json',
            })

    def test_is_testharness_baseline(self):
        self.assertTrue(is_testharness_baseline('fake-test-expected.txt'))
        self.assertTrue(is_testharness_baseline('external/wpt/fake-test-expected.txt'))
        self.assertTrue(is_testharness_baseline('/tmp/wpt/fake-test-expected.txt'))
        self.assertFalse(is_testharness_baseline('fake-test-expected.html'))
        self.assertFalse(is_testharness_baseline('external/wpt/fake-test-expected.html'))

    def test_is_basename_skipped(self):
        self.assertTrue(is_basename_skipped('MANIFEST.json'))
        self.assertTrue(is_basename_skipped('OWNERS'))
        self.assertTrue(is_basename_skipped('reftest.list'))
        self.assertTrue(is_basename_skipped('.gitignore'))
        self.assertFalse(is_basename_skipped('something.json'))

    def test_is_basename_skipped_asserts_basename(self):
        with self.assertRaises(AssertionError):
            is_basename_skipped('third_party/fake/OWNERS')

    def test_is_file_exportable(self):
        self.assertTrue(is_file_exportable(CHROMIUM_WPT_DIR + 'html/fake-test.html'))
        self.assertFalse(is_file_exportable(CHROMIUM_WPT_DIR + 'html/fake-test-expected.txt'))
        self.assertFalse(is_file_exportable(CHROMIUM_WPT_DIR + 'MANIFEST.json'))
        self.assertFalse(is_file_exportable(CHROMIUM_WPT_DIR + 'dom/OWNERS'))

    def test_is_file_exportable_asserts_path(self):
        # Rejects basenames.
        with self.assertRaises(AssertionError):
            is_file_exportable('MANIFEST.json')
        # Rejects files not in Chromium WPT.
        with self.assertRaises(AssertionError):
            is_file_exportable('third_party/fake/OWNERS')
        # Rejects absolute paths.
        with self.assertRaises(AssertionError):
            is_file_exportable('/mock-checkout/' + RELATIVE_WEB_TESTS + 'external/wpt/OWNERS')

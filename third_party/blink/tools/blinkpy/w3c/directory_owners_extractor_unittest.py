# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor


MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS
ABS_WPT_BASE = MOCK_WEB_TESTS + 'external/wpt'
REL_WPT_BASE = RELATIVE_WEB_TESTS + 'external/wpt'

class DirectoryOwnersExtractorTest(unittest.TestCase):

    def setUp(self):
        # We always have an OWNERS file at LayoutTests/external.
        self.filesystem = MockFileSystem(files={
            MOCK_WEB_TESTS + 'external/OWNERS': 'ecosystem-infra@chromium.org'
        })
        self.extractor = DirectoryOwnersExtractor(self.filesystem)

    def _write_files(self, files):
        # Use write_text_file instead of directly assigning to filesystem.files
        # so that intermediary directories are correctly created, too.
        for path, contents in files.iteritems():
            self.filesystem.write_text_file(path, contents)

    def test_list_owners_combines_same_owners(self):
        self._write_files({
            ABS_WPT_BASE + '/foo/x.html': '',
            ABS_WPT_BASE + '/foo/OWNERS': 'a@chromium.org\nc@chromium.org\n',
            ABS_WPT_BASE + '/bar/x/y.html': '',
            ABS_WPT_BASE + '/bar/OWNERS': 'a@chromium.org\nc@chromium.org\n',
        })
        changed_files = [
            REL_WPT_BASE + '/foo/x.html',
            REL_WPT_BASE + '/bar/x/y.html',
        ]
        self.assertEqual(
            self.extractor.list_owners(changed_files),
            {('a@chromium.org', 'c@chromium.org'): ['external/wpt/bar', 'external/wpt/foo']}
        )

    def test_list_owners_combines_same_directory(self):
        self._write_files({
            ABS_WPT_BASE + '/baz/x/y.html': '',
            ABS_WPT_BASE + '/baz/x/y/z.html': '',
            ABS_WPT_BASE + '/baz/x/OWNERS': 'foo@chromium.org\n',
        })
        changed_files = [
            REL_WPT_BASE + '/baz/x/y.html',
            REL_WPT_BASE + '/baz/x/y/z.html',
        ]
        self.assertEqual(
            self.extractor.list_owners(changed_files),
            {('foo@chromium.org',): ['external/wpt/baz/x']}
        )

    def test_list_owners_skips_empty_owners(self):
        self._write_files({
            ABS_WPT_BASE + '/baz/x/y/z.html': '',
            ABS_WPT_BASE + '/baz/x/y/OWNERS': '# Some comments\n',
            ABS_WPT_BASE + '/baz/x/OWNERS': 'foo@chromium.org\n',
        })
        changed_files = [
            REL_WPT_BASE + '/baz/x/y/z.html',
        ]
        self.assertEqual(
            self.extractor.list_owners(changed_files),
            {('foo@chromium.org',): ['external/wpt/baz/x']}
        )

    def test_list_owners_not_found(self):
        self._write_files({
            # Although LayoutTests/external/OWNERS exists, it should not be listed.
            ABS_WPT_BASE + '/foo/bar.html': '',
            # Files out of external.
            '/mock-checkout/' + RELATIVE_WEB_TESTS + 'TestExpectations': '',
            '/mock-checkout/' + RELATIVE_WEB_TESTS + 'OWNERS': 'foo@chromium.org',
        })
        changed_files = [
            REL_WPT_BASE + '/foo/bar.html',
            RELATIVE_WEB_TESTS + 'TestExpectations',
        ]
        self.assertEqual(self.extractor.list_owners(changed_files), {})

    def test_find_owners_file_at_current_dir(self):
        self._write_files({
            ABS_WPT_BASE + '/foo/OWNERS': 'a@chromium.org'
        })
        self.assertEqual(self.extractor.find_owners_file(REL_WPT_BASE + '/foo'), ABS_WPT_BASE + '/foo/OWNERS')

    def test_find_owners_file_at_ancestor(self):
        self._write_files({
            ABS_WPT_BASE + '/x/OWNERS': 'a@chromium.org',
            ABS_WPT_BASE + '/x/y/z.html': '',
        })
        self.assertEqual(self.extractor.find_owners_file(REL_WPT_BASE + '/x/y'), ABS_WPT_BASE + '/x/OWNERS')

    def test_find_owners_file_stops_at_external_root(self):
        self._write_files({
            ABS_WPT_BASE + '/x/y/z.html': '',
        })
        self.assertEqual(self.extractor.find_owners_file(REL_WPT_BASE + '/x/y'),
                         MOCK_WEB_TESTS + 'external/OWNERS')

    def test_find_owners_file_takes_four_kinds_of_paths(self):
        owners_path = ABS_WPT_BASE + '/foo/OWNERS'
        self._write_files({
            owners_path: 'a@chromium.org',
            ABS_WPT_BASE + '/foo/bar.html': '',
        })
        # Absolute paths of directories.
        self.assertEqual(self.extractor.find_owners_file(ABS_WPT_BASE + '/foo'), owners_path)
        # Relative paths of directories.
        self.assertEqual(self.extractor.find_owners_file(REL_WPT_BASE + '/foo'), owners_path)
        # Absolute paths of files.
        self.assertEqual(self.extractor.find_owners_file(ABS_WPT_BASE + '/foo/bar.html'), owners_path)
        # Relative paths of files.
        self.assertEqual(self.extractor.find_owners_file(REL_WPT_BASE + '/foo/bar.html'), owners_path)

    def test_find_owners_file_out_of_external(self):
        self._write_files({
            '/mock-checkout/' + RELATIVE_WEB_TESTS + 'OWNERS': 'foo@chromium.org',
            '/mock-checkout/' + RELATIVE_WEB_TESTS + 'other/some_file': '',
        })
        self.assertIsNone(self.extractor.find_owners_file(RELATIVE_WEB_TESTS[:-1]))
        self.assertIsNone(self.extractor.find_owners_file(RELATIVE_WEB_TESTS + 'other'))
        self.assertIsNone(self.extractor.find_owners_file('third_party'))

    def test_extract_owners(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS':
            '#This is a comment\n'
            '*\n'
            'foo@chromium.org\n'
            'bar@chromium.org\n'
            'foobar\n'
            '#foobar@chromium.org\n'
            '# TEAM: some-team@chromium.org\n'
            '# COMPONENT: Blink>Layout\n'
        }
        self.assertEqual(self.extractor.extract_owners(ABS_WPT_BASE + '/foo/OWNERS'),
                         ['foo@chromium.org', 'bar@chromium.org'])

    def test_extract_component(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS':
            '# TEAM: some-team@chromium.org\n'
            '# COMPONENT: Blink>Layout\n'
        }
        self.assertEqual(self.extractor.extract_component(ABS_WPT_BASE + '/foo/OWNERS'), 'Blink>Layout')

    def test_is_wpt_notify_enabled_true(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS':
            '# COMPONENT: Blink>Layout\n'
            '# WPT-NOTIFY: true\n'
        }
        self.assertTrue(self.extractor.is_wpt_notify_enabled(ABS_WPT_BASE + '/foo/OWNERS'))

    def test_is_wpt_notify_enabled_false(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS':
            '# COMPONENT: Blink>Layout\n'
            '# WPT-NOTIFY: false\n'
        }
        self.assertFalse(self.extractor.is_wpt_notify_enabled(ABS_WPT_BASE + '/foo/OWNERS'))

    def test_is_wpt_notify_enabled_absence_is_false(self):
        self.filesystem.files = {
            ABS_WPT_BASE + '/foo/OWNERS':
            '# TEAM: some-team@chromium.org\n'
            '# COMPONENT: Blink>Layout\n'
        }
        self.assertFalse(self.extractor.is_wpt_notify_enabled(ABS_WPT_BASE + '/foo/OWNERS'))

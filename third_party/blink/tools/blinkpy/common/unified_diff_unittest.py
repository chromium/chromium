# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.unified_diff import unified_diff


class TestUnifiedDiff(unittest.TestCase):
    def test_unified_diff(self):
        self.assertEqual(
            unified_diff('foo\n', 'bar\n', 'exp.txt', 'act.txt'),
            '--- exp.txt\n+++ act.txt\n@@ -1 +1 @@\n-foo\n+bar\n')

    def test_unified_diff_missing_newline(self):
        self.assertEqual(
            unified_diff('Hello\n\nWorld', 'Hello\n\nWorld\n\n\n', 'exp.txt',
                         'act.txt'),
            '--- exp.txt\n+++ act.txt\n@@ -1,3 +1,5 @@\n Hello\n \n-World\n\\ No newline at end of file\n+World\n+\n+\n'
        )

    def test_unified_diff_handles_unicode_file_names(self):
        # Make sure that we don't run into decoding exceptions when the
        # filenames are unicode, with regular or malformed input (expected or
        # actual input is always raw bytes, not unicode).
        unified_diff('exp', 'act', 'exp.txt', 'act.txt')
        unified_diff('exp', 'act', u'exp.txt', 'act.txt')
        unified_diff('exp', 'act', u'a\xac\u1234\u20ac\U00008000', 'act.txt')

    def test_unified_diff_handles_non_ascii_chars(self):
        unified_diff('exp' + chr(255), 'act', 'exp.txt', 'act.txt')
        unified_diff('exp' + chr(255), 'act', u'exp.txt', 'act.txt')

    def test_unified_diff_handles_unicode_inputs(self):
        # Though expected and actual files should always be read in with no
        # encoding (and be stored as str objects), test unicode inputs just to
        # be safe.
        unified_diff(u'exp', 'act', 'exp.txt', 'act.txt')
        unified_diff(u'a\xac\u1234\u20ac\U00008000', 'act', 'exp.txt',
                     'act.txt')

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import unittest

from blinkpy.common.pretty_diff import BinaryHunk, DiffFile, DiffHunk

# This test contains tests for protected methods.
# pylint: disable=protected-access


class TestFileDiff(unittest.TestCase):
    def _assert_file_status(self, diff, status):
        html = diff.prettify()
        match = re.search(r'\b([A-Z]) ', html)
        self.assertTrue(match and match.group(1) == status)

    def test_empty_input(self):
        lines = []
        diff, remaining_lines = DiffFile.parse(lines)
        self.assertIsNone(diff)
        self.assertEquals(remaining_lines, [])

    def test_100percent_similarity(self):
        # crrev.com/c576df77d72abe47154ff2489bb035aa20892f7f
        lines = [
            'diff --git a/platform/modules/offscreencanvas/OWNERS b/platform/modules/frame_sinks/OWNERS',
            'similarity index 100%',
            'rename from platform/modules/offscreencanvas/OWNERS',
            'rename to platform/modules/frame_sinks/OWNERS',
            'diff --git a/mojom/frame_sinks/embedded_frame_sink.mojom ' +
            'b/mojom/frame_sinks/embedded_frame_sink.mojom'
        ]
        diff, remaining_lines = DiffFile.parse(lines)
        self.assertIsNotNone(diff)
        self.assertEquals(remaining_lines[0], lines[4])

    def test_emptify_text(self):
        lines = [
            'diff --git a/third_party/blink/text-to-zero.txt b/third_party/blink/text-to-zero.txt',
            'index 2262de0..e69de29 100644',
            '--- a/third_party/blink/text-to-zero.txt',
            '+++ b/third_party/blink/text-to-zero.txt', '@@ -1 +0,0 @@',
            '-hoge'
        ]
        diff, remaining_lines = DiffFile.parse(lines)
        self.assertIsNotNone(diff)
        self.assertEquals(remaining_lines, [])
        self._assert_file_status(diff, 'M')

    def test_remove_text(self):
        lines = [
            'diff --git a/text-to-be-removed.txt b/text-to-be-removed.txt',
            'deleted file mode 100644', 'index 2262de0..0000000',
            '--- a/text-to-be-removed.txt', '+++ /dev/null', '@@ -1 +0,0 @@',
            '-hoge'
        ]
        diff, remaining_lines = DiffFile.parse(lines)
        self.assertIsNotNone(diff)
        self.assertEquals(remaining_lines, [])
        self._assert_file_status(diff, 'D')

    def test_remove_zero_byte_text(self):
        lines = [
            'diff --git a/text-zero.txt b/text-zero.txt',
            'deleted file mode 100644', 'index e69de29..0000000'
        ]
        diff, remaining_lines = DiffFile.parse(lines)
        self.assertIsNotNone(diff)
        self.assertEquals(remaining_lines, [])
        self._assert_file_status(diff, 'D')

    def test_add_empty_text(self):
        lines = [
            'diff --git a/text-zero.txt b/text-zero.txt',
            'new file mode 100644', 'index 0000000..e69de29'
        ]
        diff, remaining_lines = DiffFile.parse(lines)
        self.assertIsNotNone(diff)
        self.assertEquals(remaining_lines, [])
        self._assert_file_status(diff, 'A')

    def test_emptify_binary(self):
        lines = [
            'diff --git a/binary-to-zero.png b/binary-to-zero.png',
            'index 9b56f1c6942441578b0585d8b9688fdfcb2aa3fd..e69de29bb2d1d6434b8b29ae775ad8c2e48c5391 100644',
            'GIT binary patch', 'literal 0', 'HcmV?d00001', '', 'literal 6',
            'NcmZSh&&2%iKL7{~0|Ed5', ''
        ]
        diff, remaining_lines = DiffFile.parse(lines)
        self.assertIsNotNone(diff)
        self.assertEquals(remaining_lines, [])
        self._assert_file_status(diff, 'M')

    def test_remove_binary(self):
        lines = [
            'diff --git a/binary-to-be-removed.png b/binary-to-be-removed.png',
            'deleted file mode 100644',
            'index 9b56f1c6942441578b0585d8b9688fdfcb2aa3fd..0000000000000000000000000000000000000000',
            'GIT binary patch', 'literal 0', 'HcmV?d00001', '', 'literal 6',
            'NcmZSh&&2%iKL7{~0|Ed5', ''
        ]
        diff, remaining_lines = DiffFile.parse(lines)
        self.assertIsNotNone(diff)
        self.assertEquals(remaining_lines, [])
        self._assert_file_status(diff, 'D')

    def test_add_binary(self):
        lines = [
            'diff --git a/binary-to-zero.png b/binary-to-zero.png',
            'new file mode 100644',
            'index 0000000000000000000000000000000000000000..9b56f1c6942441578b0585d8b9688fdfcb2aa3fd',
            'GIT binary patch', 'literal 6', 'NcmZSh&&2%iKL7{~0|Ed5', '',
            'literal 0', 'HcmV?d00001', ''
        ]
        diff, remaining_lines = DiffFile.parse(lines)
        self.assertIsNotNone(diff)
        self.assertEquals(remaining_lines, [])
        self._assert_file_status(diff, 'A')


class TestDiffHunk(unittest.TestCase):
    def test_find_operations(self):
        self.assertEquals(DiffHunk._find_operations([]), [])
        self.assertEquals(DiffHunk._find_operations([' ']), [])

        self.assertEquals(DiffHunk._find_operations(['-']), [([0], [])])
        self.assertEquals(
            DiffHunk._find_operations(['-', '-']), [([0, 1], [])])
        self.assertEquals(
            DiffHunk._find_operations([' ', '-', '-']), [([1, 2], [])])
        self.assertEquals(
            DiffHunk._find_operations(['-', '-', ' ']), [([0, 1], [])])

        self.assertEquals(DiffHunk._find_operations(['+']), [([], [0])])
        self.assertEquals(
            DiffHunk._find_operations(['+', '+']), [([], [0, 1])])
        self.assertEquals(
            DiffHunk._find_operations([' ', '+', '+']), [([], [1, 2])])
        self.assertEquals(
            DiffHunk._find_operations(['+', '+', ' ']), [([], [0, 1])])

        self.assertEquals(DiffHunk._find_operations(['-', '+']), [([0], [1])])
        self.assertEquals(
            DiffHunk._find_operations(['-', '-', '+', '+']),
            [([0, 1], [2, 3])])
        self.assertEquals(
            DiffHunk._find_operations([' ', '-', '-', '+']), [([1, 2], [3])])
        self.assertEquals(
            DiffHunk._find_operations(['-', '-', '+', '+', ' ']),
            [([0, 1], [2, 3])])
        self.assertEquals(
            DiffHunk._find_operations(['-', '-', '+', '+', '-']),
            [([0, 1], [2, 3]), ([4], [])])
        self.assertEquals(
            DiffHunk._find_operations(['-', '+', '-', '+']), [([0], [1]),
                                                              ([2], [3])])

    def _annotate(self, lines, index, start, end):
        annotations = [None for _ in lines]
        DiffHunk._annotate(lines, index, start, end, annotations)
        return annotations

    def test_annotate(self):
        self.assertEquals(self._annotate(['-abcdef'], 0, 2, 4), [[(2, 4)]])
        self.assertEquals(
            self._annotate(['-abcdef', '-ghi'], 0, 2, 6), [[(2, 6)], None])
        self.assertEquals(
            self._annotate(['-abcdef', '-ghi'], 0, 2, 7), [[(2, 6)], [(0, 1)]])
        self.assertEquals(
            self._annotate(['-abcdef', '-ghi', '-jkl'], 0, 2, 11),
            [[(2, 6)], [(0, 3)], [(0, 2)]])
        self.assertEquals(
            self._annotate(['+', '+abc', ' de'], 0, 0, 2),
            [[(0, 0)], [(0, 2)], None])

    def test_prettify_header_context_escape(self):
        hunk = DiffHunk(2, 2, '<h3>Constructing form data set</h3>', [])
        self.assertNotIn('<h3>', hunk.prettify())
        self.assertIn('&lt;h3&gt;', hunk.prettify())


class TestBinaryHunk(unittest.TestCase):
    def test_literal_image(self):
        lines = ['literal 6', 'NcmZSh&&2%iKL7{~0|Ed5', '', 'literal 0...']
        binary, remaining_lines = BinaryHunk.parse(lines)
        self.assertIsNotNone(binary)
        self.assertEquals(remaining_lines[0], lines[3])
        self.assertTrue(
            'data:image/png;base64,' in binary.prettify('image/png', 'add'))

    def test_literal_non_image(self):
        lines = ['literal 6', 'NcmZSh&&2%iKL7{~0|Ed5', '']
        binary, remaining_lines = BinaryHunk.parse(lines)
        self.assertIsNotNone(binary)
        self.assertEquals(remaining_lines, [])
        self.assertTrue(
            '<img ' not in binary.prettify('application/octet-stream', 'del'))

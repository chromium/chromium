# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import re
import unittest

from blinkpy.common.checkout.diff_parser import DiffParser
from blinkpy.common.checkout.diff_test_data import DIFF_TEST_DATA


class DiffParserTest(unittest.TestCase):
    def test_diff_parser(self, parser=None):
        if not parser:
            parser = DiffParser(DIFF_TEST_DATA.splitlines())
        self.assertEqual(3, len(parser.files))

        self.assertIn('WebCore/style/StyleFlexibleBoxData.h', parser.files)
        diff = parser.files['WebCore/style/StyleFlexibleBoxData.h']
        self.assertEqual(7, len(diff.lines))

        # The first two unchanged lines.
        self.assertEqual((47, 47), diff.lines[0][0:2])
        self.assertEqual('', diff.lines[0][2])
        self.assertEqual((48, 48), diff.lines[1][0:2])
        self.assertEqual('    unsigned align : 3; // EBoxAlignment',
                         diff.lines[1][2])

        # The deleted line.
        self.assertEqual((50, 0), diff.lines[3][0:2])
        self.assertEqual('    unsigned orient: 1; // EBoxOrient',
                         diff.lines[3][2])

        # The first file looks OK. Let's check the next, more complicated file.
        self.assertIn('WebCore/style/StyleRareInheritedData.cpp', parser.files)
        diff = parser.files['WebCore/style/StyleRareInheritedData.cpp']

        # There are 3 chunks.
        self.assertEqual(7 + 7 + 9, len(diff.lines))

        # Around an added line.
        self.assertEqual((60, 61), diff.lines[9][0:2])
        self.assertEqual((0, 62), diff.lines[10][0:2])
        self.assertEqual((61, 63), diff.lines[11][0:2])

        # Look through the last chunk, which contains both adds and deletes.
        self.assertEqual((81, 83), diff.lines[14][0:2])
        self.assertEqual((82, 84), diff.lines[15][0:2])
        self.assertEqual((83, 85), diff.lines[16][0:2])
        self.assertEqual((84, 0), diff.lines[17][0:2])
        self.assertEqual((0, 86), diff.lines[18][0:2])
        self.assertEqual((0, 87), diff.lines[19][0:2])
        self.assertEqual((85, 88), diff.lines[20][0:2])
        self.assertEqual((86, 89), diff.lines[21][0:2])
        self.assertEqual((87, 90), diff.lines[22][0:2])

        # Check if a newly added file is correctly handled.
        diff = parser.files[
            'web_tests/platform/mac/fast/flexbox/box-orient-button-expected.checksum']
        self.assertEqual(1, len(diff.lines))
        self.assertEqual((0, 1), diff.lines[0][0:2])

    def test_diff_parser_with_different_mnemonic_prefixes(self):
        # This repeats test_diff_parser but with different versions
        # of DIFF_TEST_DATA that use other prefixes instead of a/b.
        prefixes = (
            # git-diff (compares the (i)ndex and the (w)ork tree)
            ('i', 'w'),
            # git-diff HEAD (compares a (c)ommit and the (w)ork tree)
            ('c', 'w'),
            # git diff --cached (compares a (c)ommit and the (i)ndex)
            ('c', 'i'),
            # git-diff HEAD:file1 file2 (compares an (o)bject and a (w)ork tree entity)
            ('o', 'w'),
            # git diff --no-index a b (compares two non-git things (1) and (2))
            ('1', '2'),
        )
        for a_replacement, b_replacement in prefixes:
            patch = self._patch(a_replacement, b_replacement)
            self.test_diff_parser(DiffParser(patch.splitlines()))

    @staticmethod
    def _patch(a_replacement='a', b_replacement='b'):
        """Returns a version of the example patch with mnemonic prefixes a/b changed."""
        patch = re.sub(r' a/', ' %s/' % a_replacement, DIFF_TEST_DATA)
        return re.sub(r' b/', ' %s/' % b_replacement, patch)

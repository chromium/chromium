#! /usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import pathlib
import sys
import textwrap

sys.path.append(str(pathlib.Path(__file__).parent.parent))
from lib import comments
from lib import pyl
from lib import values


class CommentsTest(unittest.TestCase):

  def test_comment_with_multiple_comments_and_string(self):
    pyl_value = pyl.Str(
        value='foo',
        start=pyl.Loc(file='', line=0, column=0),
        end=pyl.Loc(file='', line=0, column=0),
        comments=(
            pyl.Comment(comment='# comment 1',
                        start=pyl.Loc(file='', line=0, column=0),
                        end=pyl.Loc(file='', line=0, column=0)),
            pyl.Comment(comment='# comment 2',
                        start=pyl.Loc(file='', line=0, column=0),
                        end=pyl.Loc(file='', line=0, column=0)),
        ),
    )
    converted = 'bar'
    commented = comments.comment(pyl_value, converted)
    self.assertIsInstance(commented, values.CommentedValue)
    self.assertEqual(
        values.to_output(commented),
        textwrap.dedent("""\
            # comment 1
            # comment 2
            bar
            """)[:-1])

  def test_comment_with_no_comments_and_string(self):
    pyl_value = pyl.Str(value='foo',
                        start=pyl.Loc(file='', line=0, column=0),
                        end=pyl.Loc(file='', line=0, column=0))
    converted = 'bar'
    commented = comments.comment(pyl_value, converted)
    self.assertNotIsInstance(commented, values.CommentedValue)
    self.assertEqual(values.to_output(commented), 'bar')


if __name__ == '__main__':
  unittest.main()  # pragma: no cover

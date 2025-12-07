#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for pyl module."""

import pathlib
import sys
import tokenize
import textwrap
import unittest

sys.path.append(str(pathlib.Path(__file__).parent.parent))
from lib import pyl


class TokenTest(unittest.TestCase):

  def test_repr(self):
    file = 'test.pyl'
    start = pyl.Loc(file=file, line=1, column=2)
    end = pyl.Loc(file=file, line=3, column=4)
    token = pyl._Token(
        type=tokenize.COMMENT,
        start=start,
        end=end,
        string='# a comment',
        line='# a comment\n',
    )
    expected_repr = ("_Token(type=tokenize.COMMENT, "
                     "start=Loc(file='test.pyl', line=1, column=2), "
                     "end=Loc(file='test.pyl', line=3, column=4), "
                     "string='# a comment', "
                     "line='# a comment\\n')")
    self.assertEqual(repr(token), expected_repr)


class TokenStreamTest(unittest.TestCase):

  def test_advance_past_end_raises_error(self):
    stream = pyl._TokenStream.create('test.pyl', '')
    self.assertEqual(stream.current().type, tokenize.ENDMARKER)
    with self.assertRaises(ValueError) as caught:
      stream.advance()
    self.assertEqual(str(caught.exception), 'no more tokens available')

  def test_require_single_mismatch_raises_error(self):
    stream = pyl._TokenStream.create('test.pyl', 'a')
    self.assertEqual(stream.current().type, tokenize.NAME)
    with self.assertRaises(pyl.UnhandledParseError) as caught:
      stream.require(tokenize.STRING)
    self.assertEqual(
        str(caught.exception),
        textwrap.dedent("""\
            test.pyl:1:0: expected token of type STRING, got NAME
            a
            ^
            """)[:-1])

  def test_require_multiple_mismatch_raises_error(self):
    stream = pyl._TokenStream.create('test.pyl', 'a')
    self.assertEqual(stream.current().type, tokenize.NAME)
    with self.assertRaises(pyl.UnhandledParseError) as caught:
      stream.require((tokenize.STRING, tokenize.NUMBER))
    self.assertEqual(
        str(caught.exception),
        textwrap.dedent("""\
            test.pyl:1:0: expected token of one of types STRING or NUMBER, got NAME
            a
            ^
            """)[:-1])

  def test_token_error_message_middle_of_line_multichar_tokens(self):
    stream = pyl._TokenStream.create('test.pyl', 'aaa bbb ccc')
    stream.advance()  # advance to 'bbb'
    self.assertEqual(stream.current().type, tokenize.NAME)
    self.assertEqual(stream.current().string, 'bbb')
    message = stream.token_error_message('an error')
    expected_message = textwrap.dedent("""\
          test.pyl:1:4: an error, got NAME
          aaa bbb ccc
              ^^^
          """)[:-1]
    self.assertEqual(message, expected_message)


class ParseTest(unittest.TestCase):

  def test_parse_invalid_literal_raises_value_error(self):
    with self.assertRaises(ValueError):
      list(pyl.parse('test.pyl', '1 2'))

  def test_parse_unexpected_token_at_file_scope_raises_error(self):
    with self.assertRaises(pyl.UnhandledParseError) as caught:
      list(pyl.parse('test.pyl', '()'))
    self.assertEqual(
        str(caught.exception),
        textwrap.dedent("""\
            test.pyl:1:0: unexpected token type at file scope, got LPAR
            ()
            ^
            """)[:-1])

  def test_parse_constants(self):
    cases = [
        ("'hello'", 'hello', 7, pyl.Str),
        ('123', 123, 3, pyl.Int),
        ('True', True, 4, pyl.Bool),
        ('None', None, 4, pyl.None_),
    ]
    file = 'test.pyl'
    for source, value, end_col, cls in cases:
      with self.subTest(source=source):
        nodes = list(pyl.parse(file, source))
        self.assertEqual(
            nodes,
            [
                cls(
                    value=value,
                    start=pyl.Loc(file=file, line=1, column=0),
                    end=pyl.Loc(file=file, line=1, column=end_col),
                ),
            ],
        )

  def test_parse_unexpected_token_in_list_raises_error(self):
    with self.assertRaises(pyl.UnhandledParseError) as caught:
      list(pyl.parse('test.pyl', '[()]'))
    self.assertEqual(
        str(caught.exception),
        textwrap.dedent("""\
            test.pyl:1:1: unexpected token type while parsing list, got LPAR
            [()]
             ^
            """)[:-1])

  def test_parse_empty_list(self):
    file = 'test.pyl'
    source = '[]'
    nodes = list(pyl.parse(file, source))
    self.assertEqual(
        nodes,
        [
            pyl.List(
                elements=(),
                start=pyl.Loc(file=file, line=1, column=0),
                end=pyl.Loc(file=file, line=1, column=2),
            ),
        ],
    )

  def test_parse_list(self):
    file = 'test.pyl'
    source = "['a', 1]"
    nodes = list(pyl.parse(file, source))
    self.assertEqual(
        nodes,
        [
            pyl.List(
                elements=(
                    pyl.Str(
                        value='a',
                        start=pyl.Loc(file=file, line=1, column=1),
                        end=pyl.Loc(file=file, line=1, column=4),
                    ),
                    pyl.Int(
                        value=1,
                        start=pyl.Loc(file=file, line=1, column=6),
                        end=pyl.Loc(file=file, line=1, column=7),
                    ),
                ),
                start=pyl.Loc(file=file, line=1, column=0),
                end=pyl.Loc(file=file, line=1, column=8),
            ),
        ],
    )

  def test_parse_unexpected_token_in_dict_raises_error(self):
    with self.assertRaises(pyl.UnhandledParseError) as caught:
      list(pyl.parse('test.pyl', '{()}'))
    self.assertEqual(
        str(caught.exception),
        textwrap.dedent("""\
            test.pyl:1:1: unexpected token type while parsing dict, got LPAR
            {()}
             ^
            """)[:-1])

  def test_parse_empty_dict(self):
    file = 'test.pyl'
    source = '{}'
    nodes = list(pyl.parse(file, source))
    self.assertEqual(
        nodes,
        [
            pyl.Dict(
                items=(),
                start=pyl.Loc(file=file, line=1, column=0),
                end=pyl.Loc(file=file, line=1, column=2),
            ),
        ],
    )

  def test_parse_dict(self):
    file = 'test.pyl'
    source = "{'a': 1}"
    nodes = list(pyl.parse(file, source))
    self.assertEqual(
        nodes,
        [
            pyl.Dict(
                items=((
                    pyl.Str(
                        value='a',
                        start=pyl.Loc(file=file, line=1, column=1),
                        end=pyl.Loc(file=file, line=1, column=4),
                    ),
                    pyl.Int(
                        value=1,
                        start=pyl.Loc(file=file, line=1, column=6),
                        end=pyl.Loc(file=file, line=1, column=7),
                    ),
                ), ),
                start=pyl.Loc(file=file, line=1, column=0),
                end=pyl.Loc(file=file, line=1, column=8),
            ),
        ],
    )

  def test_parse_nested_objects(self):
    file = 'test.pyl'
    source = textwrap.dedent("""\
        {
          'a': [
            'b',
            'c'
          ],
          'd': {
            0: None,
            1: True
          }
        }
        """)
    nodes = list(pyl.parse(file, source))
    self.assertEqual(
        nodes,
        [
            pyl.Dict(
                items=(
                    (
                        pyl.Str(
                            value='a',
                            start=pyl.Loc(file=file, line=2, column=2),
                            end=pyl.Loc(file=file, line=2, column=5),
                        ),
                        pyl.List(
                            elements=(
                                pyl.Str(
                                    value='b',
                                    start=pyl.Loc(file=file, line=3, column=4),
                                    end=pyl.Loc(file=file, line=3, column=7),
                                ),
                                pyl.Str(
                                    value='c',
                                    start=pyl.Loc(file=file, line=4, column=4),
                                    end=pyl.Loc(file=file, line=4, column=7),
                                ),
                            ),
                            start=pyl.Loc(file=file, line=2, column=7),
                            end=pyl.Loc(file=file, line=5, column=3),
                        ),
                    ),
                    (
                        pyl.Str(
                            value='d',
                            start=pyl.Loc(file=file, line=6, column=2),
                            end=pyl.Loc(file=file, line=6, column=5),
                        ),
                        pyl.Dict(
                            items=(
                                (
                                    pyl.Int(
                                        value=0,
                                        start=pyl.Loc(
                                            file=file, line=7, column=4),
                                        end=pyl.Loc(file=file, line=7,
                                                    column=5),
                                    ),
                                    pyl.None_(
                                        value=None,
                                        start=pyl.Loc(
                                            file=file, line=7, column=7),
                                        end=pyl.Loc(
                                            file=file, line=7, column=11),
                                    ),
                                ),
                                (
                                    pyl.Int(
                                        value=1,
                                        start=pyl.Loc(
                                            file=file, line=8, column=4),
                                        end=pyl.Loc(file=file, line=8,
                                                    column=5),
                                    ),
                                    pyl.Bool(
                                        value=True,
                                        start=pyl.Loc(
                                            file=file, line=8, column=7),
                                        end=pyl.Loc(
                                            file=file, line=8, column=11),
                                    ),
                                ),
                            ),
                            start=pyl.Loc(file=file, line=6, column=7),
                            end=pyl.Loc(file=file, line=9, column=3),
                        ),
                    ),
                ),
                start=pyl.Loc(file=file, line=1, column=0),
                end=pyl.Loc(file=file, line=10, column=1),
            ),
        ],
    )

  def test_parse_file_comments(self):
    file = 'test.pyl'
    source = textwrap.dedent("""\
        # file comment
        1
        # another file comment
        """)
    nodes = list(pyl.parse(file, source))
    self.assertEqual(
        nodes,
        [
            pyl.Comment(
                comment='# file comment',
                start=pyl.Loc(file=file, line=1, column=0),
                end=pyl.Loc(file=file, line=1, column=14),
            ),
            pyl.Int(
                value=1,
                start=pyl.Loc(file=file, line=2, column=0),
                end=pyl.Loc(file=file, line=2, column=1),
            ),
            pyl.Comment(
                comment='# another file comment',
                start=pyl.Loc(file=file, line=3, column=0),
                end=pyl.Loc(file=file, line=3, column=22),
            ),
        ],
    )

  def test_parse_end_of_line_comment(self):
    source = '1  # comment'
    file = 'test.pyl'
    nodes = list(pyl.parse(file, source))
    self.assertEqual(
        nodes,
        [
            pyl.Int(
                value=1,
                start=pyl.Loc(file=file, line=1, column=0),
                end=pyl.Loc(file=file, line=1, column=1),
                end_of_line_comment=pyl.Comment(
                    comment='# comment',
                    start=pyl.Loc(file=file, line=1, column=3),
                    end=pyl.Loc(file=file, line=1, column=12),
                ),
            ),
        ],
    )

  def test_parse_list_comments(self):
    source = textwrap.dedent("""\
        [  # opening comment
            # element comment
            # element comment 2
            'a',  # end-of-line comment
            # element comment 3
            'b'  # end-of-line comment 2
            # trailing comment
            # trailing comment 2
        ]
        """)
    file = 'test.pyl'
    nodes = list(pyl.parse(file, source))
    self.assertEqual(
        nodes,
        [
            pyl.List(
                elements=(
                    pyl.Str(
                        value='a',
                        start=pyl.Loc(file=file, line=4, column=4),
                        end=pyl.Loc(file=file, line=4, column=7),
                        comments=(
                            pyl.Comment(
                                comment='# element comment',
                                start=pyl.Loc(file=file, line=2, column=4),
                                end=pyl.Loc(file=file, line=2, column=21),
                            ),
                            pyl.Comment(
                                comment='# element comment 2',
                                start=pyl.Loc(file=file, line=3, column=4),
                                end=pyl.Loc(file=file, line=3, column=23),
                            ),
                        ),
                        end_of_line_comment=pyl.Comment(
                            comment='# end-of-line comment',
                            start=pyl.Loc(file=file, line=4, column=10),
                            end=pyl.Loc(file=file, line=4, column=31),
                        ),
                    ),
                    pyl.Str(
                        value='b',
                        start=pyl.Loc(file=file, line=6, column=4),
                        end=pyl.Loc(file=file, line=6, column=7),
                        comments=(pyl.Comment(
                            comment='# element comment 3',
                            start=pyl.Loc(file=file, line=5, column=4),
                            end=pyl.Loc(file=file, line=5, column=23),
                        ), ),
                        end_of_line_comment=pyl.Comment(
                            comment='# end-of-line comment 2',
                            start=pyl.Loc(file=file, line=6, column=9),
                            end=pyl.Loc(file=file, line=6, column=32),
                        ),
                    ),
                ),
                start=pyl.Loc(file=file, line=1, column=0),
                end=pyl.Loc(file=file, line=9, column=1),
                opening_comment=pyl.Comment(
                    comment='# opening comment',
                    start=pyl.Loc(file=file, line=1, column=3),
                    end=pyl.Loc(file=file, line=1, column=20),
                ),
                trailing_comments=(
                    pyl.Comment(
                        comment='# trailing comment',
                        start=pyl.Loc(file=file, line=7, column=4),
                        end=pyl.Loc(file=file, line=7, column=22),
                    ),
                    pyl.Comment(
                        comment='# trailing comment 2',
                        start=pyl.Loc(file=file, line=8, column=4),
                        end=pyl.Loc(file=file, line=8, column=24),
                    ),
                ),
            ),
        ],
    )

  def test_parse_dict_comments(self):
    source = textwrap.dedent("""\
        {  # opening comment
            # key comment
            # key comment 2
            'a': 1,  # end-of-line comment
            # key comment 3
            'b': 2  # end-of-line comment 2
            # trailing comment
            # trailing comment 2
        }
        """)
    file = 'test.pyl'
    nodes = list(pyl.parse(file, source))
    self.maxDiff = None
    self.assertEqual(
        nodes,
        [
            pyl.Dict(
                items=(
                    (
                        pyl.Str(
                            value='a',
                            start=pyl.Loc(file=file, line=4, column=4),
                            end=pyl.Loc(file=file, line=4, column=7),
                            comments=(
                                pyl.Comment(
                                    comment='# key comment',
                                    start=pyl.Loc(file=file, line=2, column=4),
                                    end=pyl.Loc(file=file, line=2, column=17),
                                ),
                                pyl.Comment(
                                    comment='# key comment 2',
                                    start=pyl.Loc(file=file, line=3, column=4),
                                    end=pyl.Loc(file=file, line=3, column=19),
                                ),
                            ),
                        ),
                        pyl.Int(
                            value=1,
                            start=pyl.Loc(file=file, line=4, column=9),
                            end=pyl.Loc(file=file, line=4, column=10),
                            end_of_line_comment=pyl.Comment(
                                comment='# end-of-line comment',
                                start=pyl.Loc(file=file, line=4, column=13),
                                end=pyl.Loc(file=file, line=4, column=34),
                            ),
                        ),
                    ),
                    (
                        pyl.Str(
                            value='b',
                            start=pyl.Loc(file=file, line=6, column=4),
                            end=pyl.Loc(file=file, line=6, column=7),
                            comments=(pyl.Comment(
                                comment='# key comment 3',
                                start=pyl.Loc(file=file, line=5, column=4),
                                end=pyl.Loc(file=file, line=5, column=19),
                            ), ),
                        ),
                        pyl.Int(
                            value=2,
                            start=pyl.Loc(file=file, line=6, column=9),
                            end=pyl.Loc(file=file, line=6, column=10),
                            end_of_line_comment=pyl.Comment(
                                comment='# end-of-line comment 2',
                                start=pyl.Loc(file=file, line=6, column=12),
                                end=pyl.Loc(file=file, line=6, column=35),
                            ),
                        ),
                    ),
                ),
                start=pyl.Loc(file=file, line=1, column=0),
                end=pyl.Loc(file=file, line=9, column=1),
                opening_comment=pyl.Comment(
                    comment='# opening comment',
                    start=pyl.Loc(file=file, line=1, column=3),
                    end=pyl.Loc(file=file, line=1, column=20),
                ),
                trailing_comments=(
                    pyl.Comment(
                        comment='# trailing comment',
                        start=pyl.Loc(file=file, line=7, column=4),
                        end=pyl.Loc(file=file, line=7, column=22),
                    ),
                    pyl.Comment(
                        comment='# trailing comment 2',
                        start=pyl.Loc(file=file, line=8, column=4),
                        end=pyl.Loc(file=file, line=8, column=24),
                    ),
                ),
            ),
        ],
    )


if __name__ == '__main__':
  unittest.main()  # pragma: no cover

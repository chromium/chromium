#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit test for conversions module."""

import dataclasses
import pathlib
import sys
import textwrap
import typing
import unittest

sys.path.append(str(pathlib.Path(__file__).parent.parent))
from lib import pyl
from lib import starlark_conversions
from lib import values


# return typing.Any to prevent type checkers from complaining about the general
# typing.Value type being passed where a more specific type is expected without
# having to specify a type or cast for each call
def _to_pyl_value(value: str) -> typing.Any:
  nodes = list(pyl.parse('test', value))
  assert (all(isinstance(n, pyl.Comment) for n in nodes[:-1])
          and isinstance(nodes[-1], pyl.Value)
          ), f'{value!r} does not parse to a single pyl.Value, got {nodes}'
  node = nodes[-1]
  return dataclasses.replace(node, comments=tuple(nodes[:-1]))


class StarlarkConversionsTest(unittest.TestCase):

  def assertOutputEquals(self, value: values.MaybeCommentedValue, output: str):
    self.assertEqual(values.to_output(value), output)

  def test_convert_arg_simple_string(self):
    self.assertOutputEquals(
        starlark_conversions.convert_arg(_to_pyl_value(repr('foo'))), '"foo"')

  def test_convert_arg_magic_string(self):
    self.assertOutputEquals(
        starlark_conversions.convert_arg(
            _to_pyl_value(
                repr('$$MAGIC_SUBSTITUTION_AndroidDesktopTelemetryRemote'))),
        'targets.magic_args.ANDROID_DESKTOP_TELEMETRY_REMOTE')

  def test_convert_args(self):
    self.assertOutputEquals(
        starlark_conversions.convert_args(
            _to_pyl_value(
                repr([
                    'foo', '$$MAGIC_SUBSTITUTION_AndroidDesktopTelemetryRemote'
                ]))),
        textwrap.dedent("""\
            [
              "foo",
              targets.magic_args.ANDROID_DESKTOP_TELEMETRY_REMOTE,
            ]"""))

  def test_convert_direct_none(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(_to_pyl_value(repr(None))), 'None')

  def test_convert_direct_int(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(_to_pyl_value(repr(1))), '1')

  def test_convert_direct_bool(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(_to_pyl_value(repr(True))), 'True')

  def test_convert_direct_string(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(_to_pyl_value(repr('foo'))),
        '"foo"')

  def test_convert_direct_string_with_embedded_quote(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(_to_pyl_value(repr('"foo"'))),
        '"\\"foo\\""')

  def test_convert_direct_list(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(_to_pyl_value(repr(['foo',
                                                                'bar']))),
        textwrap.dedent("""\
            [
              "foo",
              "bar",
            ]"""))

  def test_convert_direct_dict(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(_to_pyl_value(repr({'foo':
                                                                'bar'}))),
        textwrap.dedent("""\
            {
              "foo": "bar",
            }"""))

  def test_convert_resultdb_unhandled_key(self):
    with self.assertRaises(Exception) as caught:
      starlark_conversions.convert_resultdb(
          _to_pyl_value(repr({'unhandled': True})))
    self.assertEqual(str(caught.exception),
                     'test:1:1: unhandled key in resultdb: "unhandled"')

  def test_convert_resultdb(self):
    self.assertOutputEquals(
        starlark_conversions.convert_resultdb(
            _to_pyl_value(repr({'enable': True}))),
        textwrap.dedent("""\
            targets.resultdb(
              enable = True,
            )"""))

  def test_convert_swarming_unhandled_key(self):
    with self.assertRaises(Exception) as caught:
      starlark_conversions.convert_swarming(
          _to_pyl_value(repr({'unhandled': True})))
    self.assertEqual(str(caught.exception),
                     'test:1:1: unhandled key in swarming: "unhandled"')

  def test_convert_swarming(self):
    swarming = {
        'shards': 2,
        'hard_timeout': 60,
        'io_timeout': 30,
        'expiration': 120,
        'idempotent': True,
        'service_account': 'account',
        'dimensions': {
            'pool': 'default',
        },
    }
    self.assertOutputEquals(
        starlark_conversions.convert_swarming(_to_pyl_value(repr(swarming))),
        textwrap.dedent("""\
            targets.swarming(
              shards = 2,
              hard_timeout_sec = 60,
              io_timeout_sec = 30,
              expiration_sec = 120,
              idempotent = True,
              service_account = "account",
              dimensions = {
                "pool": "default",
              },
            )"""))

  def test_convert_skylab_unhandled_key(self):
    with self.assertRaises(Exception) as caught:
      starlark_conversions.convert_skylab(
          _to_pyl_value(repr({'unhandled': True})))
    self.assertEqual(str(caught.exception),
                     'test:1:1: unhandled key in skylab: "unhandled"')

  def test_convert_skylab(self):
    skylab = {
        'shards': 2,
        'timeout_sec': 60,
    }
    self.assertOutputEquals(
        starlark_conversions.convert_skylab(_to_pyl_value(repr(skylab))),
        textwrap.dedent("""\
            targets.skylab(
              shards = 2,
              timeout_sec = 60,
            )"""))

  def test_convert_direct_with_comment_on_int(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(
            _to_pyl_value(
                textwrap.dedent("""\
                    # comment
                    1
                    """)[:-1])),
        textwrap.dedent("""\
            # comment
            1
            """)[:-1])

  def test_convert_direct_with_comment_on_list(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(
            _to_pyl_value(
                textwrap.dedent("""\
                    # comment
                    [1]
                    """)[:-1])),
        textwrap.dedent("""\
            # comment
            [
              1,
            ]
            """)[:-1])

  def test_convert_direct_with_comment_on_dict(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(
            _to_pyl_value(
                textwrap.dedent("""\
                    # comment
                    {"foo": "bar"}
                    """)[:-1])),
        textwrap.dedent("""\
            # comment
            {
              "foo": "bar",
            }
            """)[:-1])

  def test_convert_direct_with_comment_on_str(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(
            _to_pyl_value(
                textwrap.dedent("""\
                    # comment
                    'foo'
                    """)[:-1])),
        textwrap.dedent("""\
            # comment
            "foo"
            """)[:-1])

  def test_convert_direct_with_comment_and_no_include_comments(self):
    self.assertEqual(
        starlark_conversions.convert_direct(
            _to_pyl_value(
                textwrap.dedent("""\
                    # comment
                    1
                    """)[:-1]),
            include_comments=False,
        ),
        '1',
    )


if __name__ == '__main__':
  unittest.main()  # pragma: no cover

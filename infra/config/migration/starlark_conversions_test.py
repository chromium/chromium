#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit test for conversions module."""

import textwrap
import unittest

import starlark_conversions
import values


class StarlarkConversionsTest(unittest.TestCase):

  def assertOutputEquals(self, value: values.Value, output: str):
    self.assertEqual(values.to_output(value), output)

  def test_convert_arg_simple_string(self):
    self.assertOutputEquals(starlark_conversions.convert_arg('foo'), '"foo"')

  def test_convert_arg_magic_string(self):
    self.assertOutputEquals(
        starlark_conversions.convert_arg(
            '$$MAGIC_SUBSTITUTION_AndroidDesktopTelemetryRemote'),
        'targets.magic_args.ANDROID_DESKTOP_TELEMETRY_REMOTE')

  def test_convert_args(self):
    self.assertOutputEquals(
        starlark_conversions.convert_args(
            ['foo', '$$MAGIC_SUBSTITUTION_AndroidDesktopTelemetryRemote']),
        textwrap.dedent("""\
            [
              "foo",
              targets.magic_args.ANDROID_DESKTOP_TELEMETRY_REMOTE,
            ]"""))

  def test_convert_direct_unhandled_type(self):
    with self.assertRaises(Exception) as caught:
      starlark_conversions.convert_direct(set())
    self.assertEqual(str(caught.exception), "unhandled python value: set()")

  def test_convert_direct_none(self):
    self.assertOutputEquals(starlark_conversions.convert_direct(None), 'None')

  def test_convert_direct_int(self):
    self.assertOutputEquals(starlark_conversions.convert_direct(1), '1')

  def test_convert_direct_bool(self):
    self.assertOutputEquals(starlark_conversions.convert_direct(True), 'True')

  def test_convert_direct_string(self):
    self.assertOutputEquals(starlark_conversions.convert_direct('foo'), '"foo"')

  def test_convert_direct_string_with_embedded_quote(self):
    self.assertOutputEquals(starlark_conversions.convert_direct('"foo"'),
                            '"\\"foo\\""')

  def test_convert_direct_list(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct(['foo', 'bar']),
        textwrap.dedent("""\
            [
              "foo",
              "bar",
            ]"""))

  def test_convert_direct_dict(self):
    self.assertOutputEquals(
        starlark_conversions.convert_direct({'foo': 'bar'}),
        textwrap.dedent("""\
            {
              "foo": "bar",
            }"""))

  def test_convert_resultdb_unhandled_key(self):
    with self.assertRaises(Exception) as caught:
      starlark_conversions.convert_resultdb({'unhandled': True})
    self.assertEqual(str(caught.exception),
                     'unhandled key in resultdb: "unhandled"')

  def test_convert_resultdb(self):
    self.assertOutputEquals(
        starlark_conversions.convert_resultdb({'enable': True}),
        textwrap.dedent("""\
            targets.resultdb(
              enable = True,
            )"""))

  def test_convert_swarming_unhandled_key(self):
    with self.assertRaises(Exception) as caught:
      starlark_conversions.convert_swarming({'unhandled': True})
    self.assertEqual(str(caught.exception),
                     'unhandled key in swarming: "unhandled"')

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
        starlark_conversions.convert_swarming(swarming),
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
      starlark_conversions.convert_skylab({'unhandled': True})
    self.assertEqual(str(caught.exception),
                     'unhandled key in skylab: "unhandled"')

  def test_convert_skylab(self):
    skylab = {
        'shards': 2,
        'timeout_sec': 60,
    }
    self.assertOutputEquals(
        starlark_conversions.convert_skylab(skylab),
        textwrap.dedent("""\
            targets.skylab(
              shards = 2,
              timeout_sec = 60,
            )"""))


if __name__ == '__main__':
  unittest.main()  # pragma: no cover

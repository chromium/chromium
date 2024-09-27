#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit test for value_builders module."""

import dataclasses
import textwrap
import typing
import unittest

import value_builders


@dataclasses.dataclass
class TestValueBuilder(value_builders.ValueBuilder):

  output: str | None = None

  def _output_stream(self, indent: str) -> typing.Iterable[str] | None:
    del indent
    if self.output is None:
      return None
    return [self.output]


class ValueBuildersTest(unittest.TestCase):

  def test_empty_call_builder(self):
    builder = value_builders.CallValueBuilder('func')

    self.assertIsNone(builder.output())

  def test_empty_call_builder_output_empty(self):
    builder = value_builders.CallValueBuilder('func', output_empty=True)

    self.assertEqual('func()', builder.output())

  def test_call_builder(self):
    builder = value_builders.CallValueBuilder('func')
    builder['foo'] = 'x'
    builder['bar'] = 'y'

    self.assertEqual(
        textwrap.dedent("""\
            func(
              foo = x,
              bar = y,
            )"""),
        builder.output(),
    )

  def test_call_builder_with_initial_params(self):
    builder0 = value_builders.CallValueBuilder('func')
    builder0['foo'] = 'x'
    builder1 = value_builders.CallValueBuilder('func', {'foo': 'x'})

    self.assertEqual(builder0.output(), builder1.output())

    builder0['bar'] = 'y'
    builder1['bar'] = 'y'
    builder2 = value_builders.CallValueBuilder('func', {'foo': 'x', 'bar': 'y'})

    self.assertEqual(builder0.output(), builder2.output())
    self.assertEqual(builder1.output(), builder2.output())

  def test_call_builder_with_nested_value_builder(self):
    test_value_builder = TestValueBuilder()
    builder = value_builders.CallValueBuilder('func',
                                              {'foo': test_value_builder})

    self.assertIsNone(builder.output())

    builder['bar'] = 'y'

    self.assertEqual(
        textwrap.dedent("""\
            func(
              bar = y,
            )"""),
        builder.output(),
    )

    test_value_builder.output = 'x'

    self.assertEqual(
        textwrap.dedent("""\
            func(
              foo = x,
              bar = y,
            )"""),
        builder.output(),
    )

  def test_empty_dict_builder(self):
    builder = value_builders.DictValueBuilder()

    self.assertIsNone(builder.output())

  def test_empty_dict_builder_output_empty(self):
    builder = value_builders.DictValueBuilder(output_empty=True)

    self.assertEqual('{}', builder.output())

  def test_dict_builder(self):
    builder = value_builders.DictValueBuilder()
    builder['foo'] = 'x'
    builder['bar'] = 'y'

    self.assertEqual(
        textwrap.dedent("""\
            {
              "foo": x,
              "bar": y,
            }"""),
        builder.output(),
    )

  def test_dict_builder_with_initial_items(self):
    builder0 = value_builders.DictValueBuilder()
    builder0['foo'] = 'x'
    builder1 = value_builders.DictValueBuilder({'foo': 'x'})

    self.assertEqual(builder0.output(), builder1.output())

    builder0['bar'] = 'y'
    builder1['bar'] = 'y'
    builder2 = value_builders.DictValueBuilder({'foo': 'x', 'bar': 'y'})

    self.assertEqual(builder0.output(), builder2.output())
    self.assertEqual(builder1.output(), builder2.output())

  def test_dict_builder_with_nested_value_builder(self):
    test_value_builder = TestValueBuilder()
    builder = value_builders.DictValueBuilder({'foo': test_value_builder})

    self.assertIsNone(builder.output())

    builder['bar'] = 'y'

    self.assertEqual(
        textwrap.dedent("""\
            {
              "bar": y,
            }"""),
        builder.output(),
    )

    test_value_builder.output = 'x'

    self.assertEqual(
        textwrap.dedent("""\
            {
              "foo": x,
              "bar": y,
            }"""),
        builder.output(),
    )

  def test_empty_list_builder(self):
    builder = value_builders.ListValueBuilder()

    self.assertIsNone(builder.output())

  def test_empty_list_builder_output_empty(self):
    builder = value_builders.ListValueBuilder(output_empty=True)

    self.assertEqual('[]', builder.output())

  def test_list_builder(self):
    builder = value_builders.ListValueBuilder()
    builder.append('foo')
    builder.append('bar')

    self.assertEqual(
        textwrap.dedent("""\
            [
              foo,
              bar,
            ]"""),
        builder.output(),
    )

  def test_list_builder_with_initial_elements(self):
    builder0 = value_builders.ListValueBuilder()
    builder0.append('foo')
    builder1 = value_builders.ListValueBuilder(['foo'])

    self.assertEqual(builder0.output(), builder1.output())

    builder0.append('bar')
    builder1.append('bar')
    builder2 = value_builders.ListValueBuilder(['foo', 'bar'])

    self.assertEqual(builder0.output(), builder2.output())
    self.assertEqual(builder1.output(), builder2.output())

  def test_list_builder_with_nested_value_builder(self):
    test_value_builder = TestValueBuilder()
    builder = value_builders.ListValueBuilder([test_value_builder])

    self.assertIsNone(builder.output())

    builder.append('bar')

    self.assertEqual(
        textwrap.dedent("""\
            [
              bar,
            ]"""),
        builder.output(),
    )

    test_value_builder.output = 'foo'

    self.assertEqual(
        textwrap.dedent("""\
            [
              foo,
              bar,
            ]"""),
        builder.output(),
    )


if __name__ == '__main__':
  unittest.main()

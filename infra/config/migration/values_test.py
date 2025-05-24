#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit test for value_builders module."""

import textwrap
import typing
import unittest

import values


class TestValueBuilder(values._CompoundValueBuilder):

  def __init__(self):
    super().__init__()
    self.entries: list[str] | None = None

  @property
  def _prefix(self):
    return '<'

  @property
  def _suffix(self):
    return '>'

  def _entries(self, indent: str) -> typing.Iterable[str] | None:
    if not self.entries:
      return None
    return [f'{indent}{e},\n' for e in self.entries]


class ValueBuildersTest(unittest.TestCase):

  def test_empty_call_builder(self):
    builder = values.CallValueBuilder('func')

    self.assertIsNone(builder.output())

  def test_empty_call_builder_output_empty(self):
    builder = values.CallValueBuilder('func', output_empty=True)

    self.assertEqual('func()', builder.output())

  def test_call_builder(self):
    builder = values.CallValueBuilder('func')
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
    builder0 = values.CallValueBuilder('func')
    builder0['foo'] = 'x'
    builder1 = values.CallValueBuilder('func', {'foo': 'x'})

    self.assertEqual(builder0.output(), builder1.output())

    builder0['bar'] = 'y'
    builder1['bar'] = 'y'
    builder2 = values.CallValueBuilder('func', {'foo': 'x', 'bar': 'y'})

    self.assertEqual(builder0.output(), builder2.output())
    self.assertEqual(builder1.output(), builder2.output())

  def test_call_builder_with_nested_value_builder(self):
    test_value_builder = TestValueBuilder()
    builder = values.CallValueBuilder('func', {'foo': test_value_builder})

    self.assertIsNone(builder.output())

    builder['bar'] = 'y'

    self.assertEqual(
        textwrap.dedent("""\
            func(
              bar = y,
            )"""),
        builder.output(),
    )

    test_value_builder.entries = ['x', 'z']

    self.assertEqual(
        textwrap.dedent("""\
            func(
              foo = <
                x,
                z,
              >,
              bar = y,
            )"""),
        builder.output(),
    )

  def test_call_builder_elide_param(self):
    test_value_builder = TestValueBuilder()
    test_value_builder.entries = ['x', 'y']
    builder = values.CallValueBuilder(
        'func',
        {'foo': test_value_builder},
        elide_param='foo',
    )

    self.assertEqual(
        textwrap.dedent("""\
            <
              x,
              y,
            >"""),
        builder.output(),
    )

  def test_empty_dict_builder(self):
    builder = values.DictValueBuilder()

    self.assertIsNone(builder.output())

  def test_empty_dict_builder_output_empty(self):
    builder = values.DictValueBuilder(output_empty=True)

    self.assertEqual('{}', builder.output())

  def test_dict_builder(self):
    builder = values.DictValueBuilder()
    builder['foo'] = 'x'
    builder['bar'] = 'y'

    self.assertEqual(
        textwrap.dedent("""\
            {
              foo: x,
              bar: y,
            }"""),
        builder.output(),
    )

  def test_dict_builder_with_initial_items(self):
    builder0 = values.DictValueBuilder()
    builder0['foo'] = 'x'
    builder1 = values.DictValueBuilder({'foo': 'x'})

    self.assertEqual(builder0.output(), builder1.output())

    builder0['bar'] = 'y'
    builder1['bar'] = 'y'
    builder2 = values.DictValueBuilder({'foo': 'x', 'bar': 'y'})

    self.assertEqual(builder0.output(), builder2.output())
    self.assertEqual(builder1.output(), builder2.output())

  def test_dict_builder_with_nested_value_builder(self):
    test_value_builder = TestValueBuilder()
    builder = values.DictValueBuilder({'foo': test_value_builder})

    self.assertIsNone(builder.output())

    builder['bar'] = 'y'

    self.assertEqual(
        textwrap.dedent("""\
            {
              bar: y,
            }"""),
        builder.output(),
    )

    test_value_builder.entries = ['x', 'z']

    self.assertEqual(
        textwrap.dedent("""\
            {
              foo: <
                x,
                z,
              >,
              bar: y,
            }"""),
        builder.output(),
    )

  def test_empty_list_builder(self):
    builder = values.ListValueBuilder()

    self.assertIsNone(builder.output())

  def test_empty_list_builder_output_empty(self):
    builder = values.ListValueBuilder(output_empty=True)

    self.assertEqual('[]', builder.output())

  def test_list_builder(self):
    builder = values.ListValueBuilder()
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
    builder0 = values.ListValueBuilder()
    builder0.append('foo')
    builder1 = values.ListValueBuilder(['foo'])

    self.assertEqual(builder0.output(), builder1.output())

    builder0.append('bar')
    builder1.append('bar')
    builder2 = values.ListValueBuilder(['foo', 'bar'])

    self.assertEqual(builder0.output(), builder2.output())
    self.assertEqual(builder1.output(), builder2.output())

  def test_list_builder_with_nested_value_builder(self):
    test_value_builder = TestValueBuilder()
    builder = values.ListValueBuilder([test_value_builder])

    self.assertIsNone(builder.output())

    builder.append('bar')

    self.assertEqual(
        textwrap.dedent("""\
            [
              bar,
            ]"""),
        builder.output(),
    )

    test_value_builder.entries = ['foo', 'baz']

    self.assertEqual(
        textwrap.dedent("""\
            [
              <
                foo,
                baz,
              >,
              bar,
            ]"""),
        builder.output(),
    )


if __name__ == '__main__':
  unittest.main()

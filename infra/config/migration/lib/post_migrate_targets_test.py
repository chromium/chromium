#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import sys
import textwrap
import typing
import unittest

sys.path.append(str(pathlib.Path(__file__).parent.parent))
from lib import post_migrate_targets
from lib import pyl


# return typing.Any to prevent type checkers from complaining about the general
# typing.Value type being passed where a more specific type is expected without
# having to specify a type or cast for each call
def _to_pyl_value(value: object) -> typing.Any:
  nodes = list(pyl.parse('test', repr(value)))
  assert len(nodes) == 1 and isinstance(nodes[0], pyl.Value), (
      f'{object!r} does not parse to a single pyl.Value, got {nodes}')
  return nodes[0]


class PostMigrateTargetsLibTest(unittest.TestCase):

  def test_convert_basic_suite_unhandled_key(self):
    suite = {
        'test1': {
            'unhandled_key': 'value',
        },
    }
    with self.assertRaises(Exception) as caught:
      post_migrate_targets.convert_basic_suite(_to_pyl_value(suite))
    self.assertEqual(
        str(caught.exception),
        'test:1:11: unhandled key in basic suite test definition: "unhandled_key"'
    )

  def test_convert_basic_suite(self):
    suite = {
        'test1': {
            'script': 'test1.py',
            'args': ['--arg1'],
            'swarming': {
                'shards': 2,
            },
        },
        'test2': {
            'test': 'test2_test',
            'mixins': ['mixin1'],
            'remove_mixins': ['mixin2'],
        },
        'test3': {
            'ci_only': True,
            'experiment_percentage': 50,
            'use_isolated_scripts_api': False,
            'android_args': ['--android-arg'],
            'resultdb': {
                'enable': False
            },
            'android_swarming': {
                'shards': 4
            },
            'skylab': {
                'shards': 4,
            },
            'telemetry_test_name': 'telemetry_test',
        },
    }
    result = post_migrate_targets.convert_basic_suite(_to_pyl_value(suite))
    self.maxDiff = None
    self.assertEqual(
        result,
        {
            'targets':
            textwrap.dedent("""\
                [
                  "test1",
                  "test2",
                  "test3",
                ]
                """)[:-1],
            # f-string so that {""} can be inserted to avoid the string being
            # caught by the presubmit, actual curly braces must be doubled to
            # avoid them being interpreted as a replacement
            'per_test_modifications':
            textwrap.dedent(f"""\
                {{
                  "test1": targets.mixin(
                    args = [
                      "--arg1",
                    ],
                    swarming = targets.swarming(
                      shards = 2,
                    ),
                  ),
                  "test2": targets.per_test_modification(
                    remove_mixins = [
                      "DO{""} NOT SUBMIT ensure all remove mixins values are present",
                      "mixin2",
                    ],
                    mixins = [
                      "mixin1",
                    ],
                  ),
                  "test3": targets.mixin(
                    ci_only = True,
                    experiment_percentage = 50,
                    use_isolated_scripts_api = False,
                    android_args = [
                      "--android-arg",
                    ],
                    resultdb = targets.resultdb(
                      enable = False,
                    ),
                    android_swarming = targets.swarming(
                      shards = 4,
                    ),
                    skylab = targets.skylab(
                      shards = 4,
                    ),
                  ),
                }}
                """)[:-1],
        },
    )

  def test_convert_compound_suite(self):
    suite = ['suite1', 'suite2']
    result = post_migrate_targets.convert_compound_suite(_to_pyl_value(suite))
    self.assertEqual(
        result,
        {
            'targets':
            textwrap.dedent("""\
                [
                  "suite1",
                  "suite2",
                ]
                """)[:-1],
        },
    )

  def test_convert_matrix_compound_suite_unhandled_key(self):
    suite = {
        'suite1': {
            'unhandled_key': 'value',
        },
    }
    with self.assertRaises(Exception) as caught:
      post_migrate_targets.convert_matrix_compound_suite(_to_pyl_value(suite))
    self.assertEqual(
        str(caught.exception),
        'test:1:12: unhandled key in matrix config: "unhandled_key"')

  def test_convert_matrix_compound_suite(self):
    suite = {
        'suite1': {},
        'suite2': {
            'mixins': ['mixin1'],
            'variants': ['variant1'],
        },
    }
    result = post_migrate_targets.convert_matrix_compound_suite(
        _to_pyl_value(suite))
    self.assertEqual(
        result,
        {
            'targets':
            textwrap.dedent("""\
                [
                  "suite1",
                  targets.bundle(
                    targets = "suite2",
                    mixins = [
                      "mixin1",
                    ],
                    variants = [
                      "variant1",
                    ],
                  ),
                ]
                """)[:-1],
        },
    )


if __name__ == '__main__':
  unittest.main()  # pragma: no cover

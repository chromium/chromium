#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import textwrap
import unittest

import post_migrate_targets_lib


class PostMigrateTargetsLibTest(unittest.TestCase):

  def test_convert_basic_suite_unhandled_key(self):
    suite = {
        'test1': {
            'unhandled_key': 'value',
        },
    }
    with self.assertRaises(Exception) as caught:
      post_migrate_targets_lib.convert_basic_suite(suite)
    self.assertEqual(
        str(caught.exception),
        'unhandled key in basic suite test definition: "unhandled_key"')

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
    result = post_migrate_targets_lib.convert_basic_suite(suite)
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
    result = post_migrate_targets_lib.convert_compound_suite(suite)
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
      post_migrate_targets_lib.convert_matrix_compound_suite(suite)
    self.assertEqual(str(caught.exception),
                     'unhandled key in matrix config: "unhandled_key"')

  def test_convert_matrix_compound_suite(self):
    suite = {
        'suite1': {},
        'suite2': {
            'mixins': ['mixin1'],
            'variants': ['variant1'],
        },
    }
    result = post_migrate_targets_lib.convert_matrix_compound_suite(suite)
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

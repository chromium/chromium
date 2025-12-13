#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import sys
import textwrap
import typing
import unittest
from unittest import mock

sys.path.append(str(pathlib.Path(__file__).parent.parent))
from lib import migrate_targets
from lib import pyl


# return typing.Any to prevent type checkers from complaining about the general
# typing.Value type being passed where a more specific type is expected without
# having to specify a type or cast for each call
def _to_pyl_value(value: object) -> typing.Any:
  nodes = list(pyl.parse('test', repr(value)))
  assert len(nodes) == 1 and isinstance(nodes[0], pyl.Value), (
      f'{object!r} does not parse to a single pyl.Value, got {nodes}')
  return nodes[0]


class MigrateTargetsTest(unittest.TestCase):

  def test_process_waterfall_builder_group_not_found(self):
    with self.assertRaises(migrate_targets.WaterfallError) as caught:
      migrate_targets.process_waterfall(
          'non-existent',
          None,
          _to_pyl_value([]),
          _to_pyl_value({}),
      )
    self.assertEqual(str(caught.exception),
                     'builder_group "non-existent" not found')

  def test_process_waterfall_builders_not_found(self):
    waterfalls = [
        {
            'name': 'test-group',
            'machines': {
                'builder-1': {},
            },
        },
    ]
    with self.assertRaises(migrate_targets.WaterfallError) as caught:
      migrate_targets.process_waterfall(
          'test-group',
          {'builder-1', 'builder-2'},
          _to_pyl_value(waterfalls),
          _to_pyl_value({}),
      )
    self.assertEqual(
        str(caught.exception),
        'the following builders don\'t exist in builder group "test-group": "builder-2"'
    )

  def test_process_waterfall_unhandled_key(self):
    waterfalls = [
        {
            'name': 'test-group',
            'machines': {},
            'unknown_key': 'value',
        },
    ]
    with self.assertRaises(Exception) as caught:
      migrate_targets.process_waterfall(
          'test-group',
          None,
          _to_pyl_value(waterfalls),
          _to_pyl_value({}),
      )
    self.assertEqual(str(caught.exception),
                     'test:1:40: unhandled key in waterfall: "unknown_key"')

  def test_process_waterfall_unhandled_suite_type(self):
    waterfalls = [
        {
            'name': 'test-group',
            'machines': {
                'builder-1': {
                    'test_suites': {
                        'unhandled_suite': 'suite-1',
                    },
                },
            },
        },
    ]
    with self.assertRaises(Exception) as caught:
      migrate_targets.process_waterfall(
          'test-group',
          None,
          _to_pyl_value(waterfalls),
          _to_pyl_value({}),
      )
    self.assertEqual(str(caught.exception),
                     'test:1:67: unhandled suite type: "unhandled_suite"')

  def test_process_waterfall_unhandled_builder_config_key(self):
    waterfalls = [
        {
            'name': 'test-group',
            'machines': {
                'builder-1': {
                    'unknown_key': 'value',
                },
            },
        },
    ]
    with self.assertRaises(Exception) as caught:
      migrate_targets.process_waterfall(
          'test-group',
          None,
          _to_pyl_value(waterfalls),
          _to_pyl_value({}),
      )
    self.assertEqual(
        str(caught.exception),
        'test:1:51: unhandled key in builder config: "unknown_key"')

  def test_process_waterfall_success(self):
    waterfalls = [
        {
            'name': 'some-other-group',
        },
        {
            'name': 'test-group',
            'mixins': ['mixin1'],
            'forbid_script_tests': True,
            'machines': {
                'builder-1': {
                    'test_suites': {
                        'android_webview_gpu_telemetry_tests': 'suite1',
                        'cast_streaming_tests': 'suite2',
                        'gpu_telemetry_tests': 'suite3',
                        'gtest_tests': 'suite4',
                        'isolated_scripts': 'suite5',
                        'scripts': 'suite6',
                        'skylab_tests': 'suite7',
                        'skylab_gpu_telemetry_tests': 'suite8',
                    },
                    'additional_compile_targets': ['target1'],
                    'args': ['--arg1'],
                    'mixins': ['mixin2'],
                    'cros_board': 'board1',
                    'browser_config': 'debug',
                    'os_type': 'linux',
                    'skip_merge_script': True,
                    'swarming': {
                        'shards': 2,
                    },
                    'use_swarming': False,
                },
            },
        },
    ]
    test_suite_exceptions = {}

    edits = migrate_targets.process_waterfall(
        'test-group',
        None,
        _to_pyl_value(waterfalls),
        _to_pyl_value(test_suite_exceptions),
    )

    self.assertEqual(
        edits,
        migrate_targets.StarlarkEdits(
            targets_builder_defaults={
                'mixins':
                textwrap.dedent("""\
                    [
                      "mixin1",
                    ]
                    """)[:-1]
            },
            targets_settings_defaults={
                'allow_script_tests': 'False',
            },
            edits_by_builder={
                'builder-1': {
                    'targets':
                    textwrap.dedent("""\
                        targets.bundle(
                          mixins = [
                            targets.mixin(
                              args = [
                                "--arg1",
                              ],
                              swarming = targets.swarming(
                                shards = 2,
                              ),
                            ),
                            "mixin2",
                          ],
                          targets = [
                            targets.bundle(
                              mixins = targets.mixin(
                                skylab = targets.skylab(
                                  cros_board = "board1",
                                ),
                              ),
                              targets = [
                                "suite7",
                                "suite8",
                              ],
                            ),
                            "suite1",
                            "suite2",
                            "suite3",
                            "suite4",
                            "suite5",
                            "suite6",
                          ],
                          additional_compile_targets = [
                            "target1",
                          ],
                        )
                        """)[:-1],
                    'targets_settings':
                    textwrap.dedent("""\
                        targets.settings(
                          use_swarming = False,
                          browser_config = targets.browser_config.DEBUG,
                          os_type = targets.os_type.LINUX,
                          use_android_merge_script_by_default = False,
                        )
                        """)[:-1],
                },
            },
        ),
    )

  def test_process_waterfall_unhandled_exception(self):
    waterfalls = [
        {
            'name': 'test-group',
            'machines': {
                'builder-1': {
                    'test_suites': {
                        'gtest_tests': 'suite-1',
                    },
                },
            },
        },
    ]
    test_suite_exceptions = {'test-1': {'unknown_key': 'value'}}
    with self.assertRaises(Exception) as caught:
      migrate_targets.process_waterfall(
          'test-group',
          None,
          _to_pyl_value(waterfalls),
          _to_pyl_value(test_suite_exceptions),
      )
    self.assertEqual(
        str(caught.exception),
        'test:1:12: unhandled key in test_suite_exceptions: "unknown_key"')

  def test_process_waterfall_unhandled_mod_key(self):
    waterfalls = [
        {
            'name': 'test-group',
            'machines': {
                'builder-1': {
                    'test_suites': {
                        'gtest_tests': 'suite-1',
                    },
                },
            },
        },
    ]
    test_suite_exceptions = {
        'test-1': {
            'modifications': {
                'builder-1': {
                    'unknown_key': 'value',
                },
            },
        },
    }
    with self.assertRaises(Exception) as caught:
      migrate_targets.process_waterfall(
          'test-group',
          None,
          _to_pyl_value(waterfalls),
          _to_pyl_value(test_suite_exceptions),
      )
    self.assertEqual(
        str(caught.exception),
        'test:1:44: unhandled key in modifications: "unknown_key"')

  def test_process_waterfall_unhandled_replace_key(self):
    waterfalls = [{
        'name': 'test-group',
        'machines': {
            'builder-1': {
                'test_suites': {
                    'gtest_tests': 'suite-1',
                },
            },
        },
    }]
    test_suite_exceptions = {
        'test-1': {
            'replacements': {
                'builder-1': {
                    'unknown_key': 'value',
                },
            },
        },
    }
    with self.assertRaises(Exception) as caught:
      migrate_targets.process_waterfall(
          'test-group',
          None,
          _to_pyl_value(waterfalls),
          _to_pyl_value(test_suite_exceptions),
      )
    self.assertEqual(str(caught.exception),
                     'test:1:43: unhandled key in replacements: "unknown_key"')

  def test_process_waterfall_with_exceptions(self):
    waterfalls = [{
        'name': 'test-group',
        'machines': {
            'builder-1': {
                'test_suites': {
                    'gtest_tests': 'suite',
                },
            },
            'builder-2': {
                'test_suites': {
                    'gtest_tests': 'suite',
                },
            },
            'builder-3': {
                'test_suites': {
                    'gtest_tests': 'suite',
                },
            },
        }
    }]
    test_suite_exceptions = {
        'test': {
            'remove_from': ['builder-1', 'non-matching-builder'],
            'modifications': {
                'builder-2': {
                    'ci_only': True,
                    'experiment_percentage': 10,
                    'isolate_profile_data': False,
                    'retry_only_failed_tests': True,
                    'args': ['--mod-arg'],
                    'swarming': {
                        'shards': 8
                    },
                },
                'non-matching-builder': {},
            },
            'replacements': {
                'builder-3': {
                    'args': {
                        '--arg1': 'v1',
                    },
                    'precommit_args': {
                        '--arg2': 'v2',
                    },
                    'non_precommit_args': {
                        '--arg3': 'v3',
                    },
                },
                'non-matching-builder': {},
            },
        },
    }

    edits = migrate_targets.process_waterfall(
        'test-group',
        None,
        _to_pyl_value(waterfalls),
        _to_pyl_value(test_suite_exceptions),
    )

    self.maxDiff = None
    self.assertEqual(
        edits,
        migrate_targets.StarlarkEdits(
            targets_builder_defaults={},
            targets_settings_defaults={},
            edits_by_builder={
                'builder-1': {
                    # f-string so that {""} can be inserted to avoid the string
                    # being caught by the presubmit, actual curly braces must be
                    # doubled to avoid them being interpreted as a replacement
                    'targets':
                    textwrap.dedent(f"""\
                        targets.bundle(
                          targets = [
                            "suite",
                          ],
                          per_test_modifications = {{
                            "test": targets.remove(
                              reason = "DO{""} NOT SUBMIT provide an actual reason",
                            ),
                          }},
                        )
                        """)[:-1],
                },
                'builder-2': {
                    'targets':
                    textwrap.dedent("""\
                        targets.bundle(
                          targets = [
                            "suite",
                          ],
                          per_test_modifications = {
                            "test": targets.mixin(
                              ci_only = True,
                              experiment_percentage = 10,
                              isolate_profile_data = False,
                              retry_only_failed_tests = True,
                              args = [
                                "--mod-arg",
                              ],
                              swarming = targets.swarming(
                                shards = 8,
                              ),
                            ),
                          },
                        )
                        """)[:-1],
                },
                'builder-3': {
                    'targets':
                    textwrap.dedent("""\
                        targets.bundle(
                          targets = [
                            "suite",
                          ],
                          per_test_modifications = {
                            "test": targets.per_test_modification(
                              replacements = targets.replacements(
                                args = {
                                  "--arg1": "v1",
                                },
                                precommit_args = {
                                  "--arg2": "v2",
                                },
                                non_precommit_args = {
                                  "--arg3": "v3",
                                },
                              ),
                            ),
                          },
                        )
                        """)[:-1],
                },
            },
        ),
    )

  def test_process_waterfall_subset_of_builders(self):
    waterfalls = [
        {
            'name': 'test-group',
            'forbid_script_tests': False,
            'machines': {
                'builder-1': {
                    'os_type': 'linux',
                    'skip_merge_script': False,
                },
                'builder-3': {
                    'os_type': 'win',
                },
                'builder-2': {
                    'os_type': 'mac',
                },
            },
        },
    ]

    edits = migrate_targets.process_waterfall(
        'test-group',
        {'builder-1', 'builder-2'},
        _to_pyl_value(waterfalls),
        _to_pyl_value({}),
    )

    self.assertCountEqual(edits.edits_by_builder.keys(),
                          ['builder-1', 'builder-2'])

  @mock.patch('lib.migrate_targets.buildozer.run')
  def test_update_starlark(self, mock_buildozer_run):
    builder_group = 'test-group'
    star_file = pathlib.Path('path/to/file.star')
    edits = migrate_targets.StarlarkEdits(
        targets_builder_defaults={'mixins': '["mixin1"]'},
        targets_settings_defaults={'allow_script_tests': 'False'},
        edits_by_builder={
            'builder-1': {
                'targets': 'targets.bundle()',
                'targets_settings': 'targets.settings()'
            }
        },
    )

    # Mock return value for checking if defaults exist.
    # An empty string means it doesn't exist.
    mock_buildozer_run.return_value = ''

    migrate_targets.update_starlark(builder_group, star_file, edits)

    file_target = f'{star_file}:__pkg__'
    builder_target = f'{star_file}:builder-1'

    mock_buildozer_run.assert_any_call('new_load //lib/targets.star targets',
                                       file_target)

    temp_name = 'NO_DECLARATION_SHOULD_EXIST_WITH_THIS_NAME'
    temp_target = f'{star_file}:{temp_name}'
    builder_defaults_kind = 'targets.builder_defaults.set'
    builder_defaults_target = f'{star_file}:%{builder_defaults_kind}'
    settings_defaults_kind = 'targets.settings_defaults.set'
    settings_defaults_target = f'{star_file}:%{settings_defaults_kind}'

    # Check creation and setting of defaults
    mock_buildozer_run.assert_has_calls(
        [
            mock.call('print kind', builder_defaults_target),
            mock.call(
                f'new {builder_defaults_kind} {temp_name} before {builder_group}',
                file_target),
            mock.call('remove name', temp_target),
            mock.call('set mixins ["mixin1"]', builder_defaults_target),
        ],
        any_order=False,
    )

    mock_buildozer_run.assert_has_calls(
        [
            mock.call('print kind', settings_defaults_target),
            mock.call(
                f'new {settings_defaults_kind} {temp_name} before {builder_group}',
                file_target),
            mock.call('remove name', temp_target),
            mock.call('set allow_script_tests False', settings_defaults_target),
        ],
        any_order=False,
    )

    # Check setting builder edits
    mock_buildozer_run.assert_any_call('set targets targets.bundle()',
                                       builder_target)
    mock_buildozer_run.assert_any_call(
        'set targets_settings targets.settings()', builder_target)

  @mock.patch('lib.migrate_targets.buildozer.run')
  def test_update_starlark_defaults_exist(self, mock_buildozer_run):
    builder_group = 'test-group'
    star_file = pathlib.Path('path/to/file.star')
    edits = migrate_targets.StarlarkEdits(
        targets_builder_defaults={'mixins': '["mixin1"]'},
        targets_settings_defaults={'allow_script_tests': 'False'},
        edits_by_builder={},
    )

    # Mock return value for checking if defaults exist.
    # A non-empty string means it does exist.
    mock_buildozer_run.return_value = 'exists'

    migrate_targets.update_starlark(builder_group, star_file, edits)

    builder_defaults_target = f'{star_file}:%targets.builder_defaults.set'
    settings_defaults_target = f'{star_file}:%targets.settings_defaults.set'

    mock_buildozer_run.assert_any_call('print kind', builder_defaults_target)
    mock_buildozer_run.assert_any_call('set mixins ["mixin1"]',
                                       builder_defaults_target)
    mock_buildozer_run.assert_any_call('print kind', settings_defaults_target)
    mock_buildozer_run.assert_any_call('set allow_script_tests False',
                                       settings_defaults_target)

    # Check that `new` is not called
    new_call_present = any(c.args[0].startswith('new ')
                           for c in mock_buildozer_run.mock_calls)
    self.assertFalse(new_call_present)

  @mock.patch('lib.migrate_targets.buildozer.run')
  def test_update_starlark_no_edits(self, mock_buildozer_run):
    builder_group = 'test-group'
    star_file = pathlib.Path('path/to/file.star')
    edits = migrate_targets.StarlarkEdits(
        targets_builder_defaults={},
        targets_settings_defaults={},
        edits_by_builder={'builder-1': {}},
    )

    migrate_targets.update_starlark(builder_group, star_file, edits)

    # load is always called
    self.assertEqual(mock_buildozer_run.call_count, 1)
    mock_buildozer_run.assert_called_once_with(
        'new_load //lib/targets.star targets', f'{star_file}:__pkg__')


if __name__ == '__main__':
  unittest.main()  # pragma: no cover

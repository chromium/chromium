#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for query_optimal_shard_counts.py."""

import datetime
import os
import json
import platform
import subprocess
import tempfile
import unittest
from unittest import mock

import query_optimal_shard_counts

# Protected access is allowed for unittests.
# pylint: disable=protected-access

SUITE_DURATIONS_DEFAULT_DICT = {
    'shard_count': 10,
    'p50_pending_time_sec': 1,
    'p90_pending_time_sec': 231,
    'avg_pending_time_sec': 65,
    'avg_task_setup_overhead_sec': 27,
    'percentile_duration_minutes': 20,
    'sample_size': 9508,
}

OVERHEADS_DEFAULT_DICT = {
    'p50_task_setup_duration_sec': 70,
    'p50_test_harness_overhead_sec': 100,
    'normally_assigned_shard_count': 10,
    'experimental_shard_count': 11,
}


def query_suite_durations_dict(
    waterfall_builder_group,
    waterfall_builder_name,
    try_builder,
    test_suite,
    **kwargs,
):
  return_dict = SUITE_DURATIONS_DEFAULT_DICT.copy()
  return_dict.update({
      'test_suite': test_suite,
      'try_builder': try_builder,
      'waterfall_builder_group': waterfall_builder_group,
      'waterfall_builder_name': waterfall_builder_name,
      **kwargs,
  })
  return return_dict


def query_test_overheads_dict(
    waterfall_builder_group,
    waterfall_builder_name,
    try_builder,
    test_suite,
    **kwargs,
):
  return_dict = OVERHEADS_DEFAULT_DICT.copy()
  return_dict.update({
      'test_suite': test_suite,
      'try_builder': try_builder,
      'waterfall_builder_group': waterfall_builder_group,
      'waterfall_builder_name': waterfall_builder_name,
      **kwargs,
  })
  return return_dict


def query_average_number_builds_per_hour(try_builder, avg_count=80):
  return {
      'try_builder': try_builder,
      'avg_count': avg_count,
  }


def get_written_output(mock_open):
  write_mock = mock_open.return_value.__enter__.return_value.write
  mock_call_args, _ = write_mock.call_args
  written_data = mock_call_args[0]
  return json.loads(written_data)


@unittest.skipIf(platform.system() == 'Windows',
                 'These tests are currently not supported on Windows.')
class FormatQueryResults(unittest.TestCase):
  def setUp(self):
    self._mock_check_call_patcher = mock.patch(
        'query_optimal_shard_counts.subprocess.check_call')
    self._mock_check_call = self._mock_check_call_patcher.start()
    self._mock_check_output_patcher = mock.patch(
        'query_optimal_shard_counts.subprocess.check_output')
    self._mock_check_output = self._mock_check_output_patcher.start()
    self.output_file_handle, self.output_file = tempfile.mkstemp()
    with open(self.output_file, 'w') as f:
      f.write(
          json.dumps({
              'chromium.builder_group': {
                  'builder_name': {
                      'fake_test_suite': {
                          'shards': 4
                      }
                  }
              },
          }))

  def tearDown(self):
    os.remove(self.output_file)
    os.close(self.output_file_handle)
    self._mock_check_call_patcher.stop()
    self._mock_check_output_patcher.stop()

  def _testBQNotInstalled(self):
    self._mock_check_call.side_effect = (subprocess.CalledProcessError(
        returncode=1, cmd="['which', 'bq']"))
    with self.assertRaises(RuntimeError) as context:
      query_optimal_shard_counts.main([])
    self.assertTrue(query_optimal_shard_counts._BQ_SETUP_INSTRUCTION in str(
        context.exception))

  def testBasic(self):
    expected_optimal_shard_count = 15
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='linux-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    query_optimal_shard_counts.main(['--output-file', self.output_file])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)
    self.assertEqual(
        script_result['chromium.linux']['Linux Tests'],
        {'browser_tests': {
            'shards': expected_optimal_shard_count
        }})

  def testMultipleBuildersInGroup(self):
    expected_optimal_shard_count_1 = 15
    expected_optimal_shard_count_2 = 23
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            shard_count=10,
            percentile_duration_minutes=20,
        ),
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux GPU Tests',
            try_builder='linux-rel',
            test_suite='gpu_tests',
            shard_count=10,
            percentile_duration_minutes=30,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux GPU Tests',
            try_builder='linux-rel',
            test_suite='gpu_tests',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='linux-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    query_optimal_shard_counts.main(['--output-file', self.output_file])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)
    self.assertEqual(
        len(script_result['chromium.linux']),
        2,
    )
    self.assertEqual(
        script_result['chromium.linux']['Linux Tests'],
        {'browser_tests': {
            'shards': expected_optimal_shard_count_1
        }})
    self.assertEqual(script_result['chromium.linux']['Linux GPU Tests'],
                     {'gpu_tests': {
                         'shards': expected_optimal_shard_count_2
                     }})

  def testVerbose(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='linux-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    query_optimal_shard_counts.main(
        ['--output-file', self.output_file, '--verbose'])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)
    expected_debug_dict_keys = [
        'prev_shard_count',
        'simulated_max_shard_duration',
        'prev_percentile_duration_minutes',
        'test_overhead_min',
    ]
    dict_result = script_result['chromium.linux']['Linux Tests'][
        'browser_tests']
    self.assertEqual(dict_result['shards'], 15)
    debug_keys = set(dict_result.keys())
    self.assertTrue(key in debug_keys for key in expected_debug_dict_keys)

  def testNoQueryResults(self):
    self._mock_check_output.return_value = json.dumps([])
    query_optimal_shard_counts.main(
        ['--output-file', self.output_file, '--overwrite-output-file'])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)
    self.assertEqual({}, script_result)

  def testLookbackDates(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='linux-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    query_optimal_shard_counts.main([
        '--lookback-start-date', '2023-01-01', '--lookback-end-date',
        '2023-02-01', '--output-file', self.output_file
    ])
    mock_call_args, _ = self._mock_check_output.call_args
    big_query_arg_list = mock_call_args[0]
    self.assertTrue(any('2023-01-01' in arg for arg in big_query_arg_list))
    self.assertTrue(any('2023-02-01' in arg for arg in big_query_arg_list))

  def testLookbackDays(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='linux-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    fake_now = datetime.datetime(2023, 1, 1)
    with mock.patch(
        'query_optimal_shard_counts.datetime.datetime') as mock_datetime:
      mock_datetime.now.return_value = fake_now
      query_optimal_shard_counts.main(
          ['--lookback-days', '3', '--output-file', self.output_file])
    mock_call_args, _ = self._mock_check_output.call_args
    big_query_arg_list = mock_call_args[0]
    self.assertTrue(any('2023-01-01' in arg for arg in big_query_arg_list))
    self.assertTrue(any('2022-12-29' in arg for arg in big_query_arg_list))

  def testMergeExistingOutputFile(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            percentile_duration_minutes=30,
            shard_count=10,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='linux-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    existing_output_file_data = json.dumps({
        'chromium.linux': {
            'Linux Tests': {
                'browser_tests': {
                    'shards': 10,
                },
                'interactive_ui_tests': {
                    'shards': 3,
                }
            },
        },
        'chromium.android': {
            'android-12-x64-rel': {
                'webview_instrumentation_test_apk': {
                    'shards': 11,
                }
            }
        }
    })
    with open(self.output_file, 'w') as f:
      f.write(existing_output_file_data)
    query_optimal_shard_counts.main(['--output-file', self.output_file])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)
    self.assertEqual(
        script_result['chromium.linux']['Linux Tests']['browser_tests'],
        {'shards': 23},
    )
    self.assertEqual(
        script_result['chromium.linux']['Linux Tests']['interactive_ui_tests'],
        {'shards': 3},
    )
    self.assertEqual(
        script_result['chromium.android']['android-12-x64-rel']
        ['webview_instrumentation_test_apk'],
        {'shards': 11},
    )

  def testOverwriteExistingOutputFile(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            percentile_duration_minutes=30,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='linux-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    existing_output_file_data = json.dumps({
        'chromium.linux': {
            'Linux Tests': {
                'browser_tests': {
                    'shards': 15,
                },
                'interactive_ui_tests': {
                    'shards': 3,
                }
            },
        },
        'chromium.android': {
            'android-12-x64-rel': {
                'webview_instrumentation_test_apk': {
                    'shards': 11,
                }
            }
        }
    })
    with open(self.output_file, 'w') as f:
      f.write(existing_output_file_data)
    query_optimal_shard_counts.main(
        ['--output-file', self.output_file, '--overwrite-output-file'])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)
    self.assertEqual(
        script_result['chromium.linux']['Linux Tests']['browser_tests'],
        {'shards': 23},
    )
    self.assertIsNone(script_result['chromium.linux']['Linux Tests'].get(
        'interactive_ui_tests'))
    self.assertIsNone(script_result.get('chromium.android'))

  def testReduceShardsAlreadyAutosharded(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            percentile_duration_minutes=10,
            shard_count=15,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='linux-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    existing_output_file_data = json.dumps({
        'chromium.linux': {
            'Linux Tests': {
                'browser_tests': {
                    'shards': 15,
                },
                'interactive_ui_tests': {
                    'shards': 3,
                }
            },
        },
        'chromium.android': {
            'android-12-x64-rel': {
                'webview_instrumentation_test_apk': {
                    'shards': 11,
                }
            }
        }
    })
    with open(self.output_file, 'w') as f:
      f.write(existing_output_file_data)

    query_optimal_shard_counts.main(['--output-file', self.output_file])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)
    self.assertLess(
        script_result['chromium.linux']['Linux Tests']['browser_tests']
        ['shards'],
        14,
    )
    self.assertEqual(
        script_result['chromium.linux']['Linux Tests']['interactive_ui_tests'],
        {'shards': 3},
    )
    self.assertIsNone(script_result['chromium.android']
                      ['android-12-x64-rel'].get('other_fake_test'))

  def testMultipleOverheadValues(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            shard_count=20,
            percentile_duration_minutes=20,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            normally_assigned_shard_count=19,
            experimental_shard_count=20,
            p50_task_setup_duration_sec=30,
            p50_test_harness_overhead_sec=60,
        ),
        query_test_overheads_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            normally_assigned_shard_count=20,
            experimental_shard_count=21,
            p50_task_setup_duration_sec=30,
            p50_test_harness_overhead_sec=120,
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='android-12-x64-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    query_optimal_shard_counts.main(
        ['--output-file', self.output_file, '--verbose'])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)

    self.assertEqual(
        script_result['chromium.android']['android-12-x64-rel']
        ['webview_instrumentation_test_apk']['debug']['test_overhead_min'],
        2.5,
    )

  def testRejectSuggestedDecreaseIfPercentileMinWasCloseToDesired(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            shard_count=10,
            percentile_duration_minutes=14.5,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='android-12-x64-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    existing_output_file_data = json.dumps({
        'chromium.android': {
            'android-12-x64-rel': {
                'webview_instrumentation_test_apk': {
                    'shards': 10,
                }
            }
        }
    })
    with open(self.output_file, 'w') as f:
      f.write(existing_output_file_data)

    query_optimal_shard_counts.main(['--output-file', self.output_file])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)

    self.assertEqual(
        script_result['chromium.android']['android-12-x64-rel']
        ['webview_instrumentation_test_apk'],
        {'shards': 10},
    )

  def testRejectSuggestedDecreaseIfSimulatedIsTooHigh(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            shard_count=10,
            percentile_duration_minutes=14,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='android-12-x64-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    existing_output_file_data = json.dumps({
        'chromium.android': {
            'android-12-x64-rel': {
                'webview_instrumentation_test_apk': {
                    'shards': 10,
                }
            }
        }
    })
    with open(self.output_file, 'w') as f:
      f.write(existing_output_file_data)

    query_optimal_shard_counts.main(['--output-file', self.output_file])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)

    self.assertEqual(
        script_result['chromium.android']['android-12-x64-rel']
        ['webview_instrumentation_test_apk'],
        {'shards': 10},
    )

  def testNotAlreadyAutoshardedDecrease(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            shard_count=10,
            percentile_duration_minutes=9,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='android-12-x64-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    existing_output_file_data = json.dumps({
        'chromium.linux': {
            'Linux Tests': {
                'browser_tests': {
                    'shards': 10,
                }
            }
        }
    })
    with open(self.output_file, 'w') as f:
      f.write(existing_output_file_data)
    query_optimal_shard_counts.main(['--output-file', self.output_file])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)

    self.assertIsNone(script_result.get('chromium.android'))

  def testNotAlreadyAutoshardedIncrease(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            shard_count=10,
            percentile_duration_minutes=20,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='android-12-x64-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    existing_output_file_data = json.dumps({
        'chromium.linux': {
            'Linux Tests': {
                'browser_tests': {
                    'shards': 10,
                }
            }
        }
    })
    with open(self.output_file, 'w') as f:
      f.write(existing_output_file_data)
    query_optimal_shard_counts.main(['--output-file', self.output_file])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)

    self.assertEqual(
        script_result['chromium.android']['android-12-x64-rel']
        ['webview_instrumentation_test_apk'],
        {'shards': 15},
    )

  @mock.patch('query_optimal_shard_counts.TEST_SUITE_EXCLUDE_SET',
              new={'browser_tests', 'ui_tests'})
  def testTestSuiteExcludeSet(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            shard_count=10,
            percentile_duration_minutes=20,
        ),
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='ui_tests',
            shard_count=10,
            percentile_duration_minutes=20,
        ),
        query_suite_durations_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            shard_count=10,
            percentile_duration_minutes=20,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='android-12-x64-rel'),
        query_average_number_builds_per_hour(try_builder='linux-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    existing_output_file_data = json.dumps({
        'chromium.linux': {
            'Linux Tests': {
                'browser_tests': {
                    'shards': 10,
                }
            }
        },
        'chromium.android': {
            'android-12-x64-rel': {
                'browser_tests': {
                    'shards': 20,
                }
            }
        },
    })
    with open(self.output_file, 'w') as f:
      f.write(existing_output_file_data)
    query_optimal_shard_counts.main(['--output-file', self.output_file])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)

    self.assertEqual(
        script_result['chromium.linux']['Linux Tests']['browser_tests']
        ['shards'],
        10,
    )
    self.assertEqual(
        script_result['chromium.android']['android-12-x64-rel']['browser_tests']
        ['shards'],
        20,
    )
    self.assertTrue(
        'ui_tests' not in script_result['chromium.linux']['Linux Tests'])

  @mock.patch('query_optimal_shard_counts.BUILDER_EXCLUDE_SET',
              new={'mac-rel', 'linux-rel'})
  def testBuilderExcludeSet(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            shard_count=10,
            percentile_duration_minutes=20,
        ),
        query_suite_durations_dict(
            waterfall_builder_group='chromium.mac',
            waterfall_builder_name='Mac Builder',
            try_builder='mac-rel',
            test_suite='browser_tests',
            shard_count=10,
            percentile_duration_minutes=20,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='mac-rel'),
        query_average_number_builds_per_hour(try_builder='linux-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    existing_output_file_data = json.dumps({
        'chromium.linux': {
            'Linux Tests': {
                'ui_tests': {
                    'shards': 10,
                }
            }
        },
        'chromium.mac': {
            'Mac Builder': {
                'browser_tests': {
                    'shards': 10,
                }
            }
        }
    })
    with open(self.output_file, 'w') as f:
      f.write(existing_output_file_data)
    query_optimal_shard_counts.main(['--output-file', self.output_file])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)

    self.assertEqual(
        script_result['chromium.linux']['Linux Tests']['ui_tests']['shards'],
        10,
    )
    self.assertEqual(
        script_result['chromium.mac']['Mac Builder']['browser_tests']['shards'],
        10,
    )
    self.assertTrue(
        'browser_tests' not in script_result['chromium.linux']['Linux Tests'])

  @mock.patch('query_optimal_shard_counts.BUILDER_TEST_SUITE_EXCLUDE_DICT',
              new={'linux-rel': {'browser_tests', 'ui_tests'}})
  def testBuilderTestSuiteExcludeDict(self):
    suite_durations = json.dumps([
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            shard_count=10,
            percentile_duration_minutes=20,
        ),
        query_suite_durations_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='ui_tests',
            shard_count=10,
            percentile_duration_minutes=20,
        ),
    ])
    overheads = json.dumps([
        query_test_overheads_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    avg_num_builds_per_hour = json.dumps([
        query_average_number_builds_per_hour(try_builder='linux-rel'),
    ])
    self._mock_check_output.side_effect = [
        suite_durations,
        overheads,
        avg_num_builds_per_hour,
    ]
    existing_output_file_data = json.dumps({
        'chromium.linux': {
            'Linux Tests': {
                'browser_tests': {
                    'shards': 10,
                }
            }
        },
        'chromium.android': {
            'android-12-x64-rel': {
                'browser_tests': {
                    'shards': 20,
                }
            }
        },
    })
    with open(self.output_file, 'w') as f:
      f.write(existing_output_file_data)
    query_optimal_shard_counts.main(['--output-file', self.output_file])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)

    self.assertEqual(
        script_result['chromium.linux']['Linux Tests']['browser_tests']
        ['shards'],
        10,
    )
    self.assertTrue(
        'ui_tests' not in script_result['chromium.linux']['Linux Tests'])


if __name__ == '__main__':
  unittest.main(verbosity=2)

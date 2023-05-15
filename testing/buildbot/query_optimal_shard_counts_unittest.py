#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for query_optimal_shard_counts.py."""

import datetime
import sys
import os
import json
import mock
import platform
import subprocess
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..'))

import query_optimal_shard_counts

DEFAULT_DICT = {
    'shard_count': 10,
    'optimal_shard_count': 20,
    'simulated_max_shard_duration': 11,
    'avg_num_builds_per_peak_hour': 80,
    'p50_pending_time_sec': 1,
    'p90_pending_time_sec': 231,
    'avg_pending_time_sec': 65,
    'avg_task_setup_overhead_sec': 27,
    'estimated_bot_hour_cost': 1.27,
    'percentile_duration_minutes': 20,
    'sample_size': 9508,
    'most_used_shard_count': 10,
}


def query_response_test_suite_dict(
    waterfall_builder_group,
    waterfall_builder_name,
    try_builder,
    test_suite,
    **kwargs,
):
  return_dict = DEFAULT_DICT.copy()
  return_dict.update({
      'test_suite': test_suite,
      'try_builder': try_builder,
      'waterfall_builder_group': waterfall_builder_group,
      'waterfall_builder_name': waterfall_builder_name,
      **kwargs,
  })
  return return_dict


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
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            optimal_shard_count=expected_optimal_shard_count,
        ),
    ])
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
    expected_optimal_shard_count_2 = 20
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            optimal_shard_count=expected_optimal_shard_count_1,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux GPU Tests',
            try_builder='linux-rel',
            test_suite='gpu_tests',
            optimal_shard_count=expected_optimal_shard_count_2,
        ),
    ])
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
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    query_optimal_shard_counts.main(
        ['--output-file', self.output_file, '--verbose'])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)
    expected_debug_dict = {
        'debug': {
            'prev_shard_count':
            DEFAULT_DICT['shard_count'],
            'simulated_max_shard_duration':
            DEFAULT_DICT['simulated_max_shard_duration'],
            'prev_percentile_duration_minutes':
            DEFAULT_DICT['percentile_duration_minutes'],
        },
    }
    dict_result = script_result['chromium.linux']['Linux Tests'][
        'browser_tests']
    self.assertTrue(
        dict_result['shards'] == DEFAULT_DICT['optimal_shard_count'])
    self.assertTrue(
        all(dict_result['debug'][key] == val
            for key, val in expected_debug_dict['debug'].items()))

  def testNoQueryResults(self):
    self._mock_check_output.return_value = json.dumps([])
    query_optimal_shard_counts.main(
        ['--output-file', self.output_file, '--overwrite-output-file'])
    with open(self.output_file, 'r') as f:
      script_result = json.load(f)
    self.assertEqual({}, script_result)

  def testLookbackDates(self):
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
    query_optimal_shard_counts.main([
        '--lookback-start-date', '2023-01-01', '--lookback-end-date',
        '2023-02-01', '--output-file', self.output_file
    ])
    mock_call_args, _ = self._mock_check_output.call_args
    big_query_arg_list = mock_call_args[0]
    self.assertTrue(any('2023-01-01' in arg for arg in big_query_arg_list))
    self.assertTrue(any('2023-02-01' in arg for arg in big_query_arg_list))

  def testLookbackDays(self):
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
        ),
    ])
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
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            optimal_shard_count=16,
            shard_count=15,
        ),
    ])
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
    self.assertEqual(
        script_result['chromium.linux']['Linux Tests']['browser_tests'],
        {'shards': 16},
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
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            optimal_shard_count=16,
        ),
    ])
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
        {'shards': 16},
    )
    self.assertIsNone(script_result['chromium.linux']['Linux Tests'].get(
        'interactive_ui_tests'))
    self.assertIsNone(script_result.get('chromium.android'))

  def testReduceShardsAlreadyAutosharded(self):
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(waterfall_builder_group='chromium.linux',
                                       waterfall_builder_name='Linux Tests',
                                       try_builder='linux-rel',
                                       test_suite='browser_tests',
                                       optimal_shard_count=14,
                                       shard_count=15),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='other_fake_test',
            optimal_shard_count=10,
            shard_count=20,
        ),
    ])
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
    self.assertEqual(
        script_result['chromium.linux']['Linux Tests']['browser_tests'],
        {'shards': 14},
    )
    self.assertEqual(
        script_result['chromium.linux']['Linux Tests']['interactive_ui_tests'],
        {'shards': 3},
    )
    self.assertIsNone(script_result['chromium.android']
                      ['android-12-x64-rel'].get('other_fake_test'))

  def testMultipleShardValuesFromQueryAlreadyAutoshardedNoChange(self):
    # Most used shard count is still the old 20 value, because it was recently
    # autosharded to 10
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=10,
            shard_count=20,
            most_used_shard_count=20,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=10,
            shard_count=10,
            most_used_shard_count=20,
        ),
    ])
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

  def testMultipleShardValuesFromQueryAlreadyAutoshardedDecrease(self):
    # Most used shard count is still the old 20 value, because it was recently
    # autosharded to 10
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=10,
            shard_count=20,
            most_used_shard_count=20,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=8,
            shard_count=10,
            most_used_shard_count=20,
        ),
    ])
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
        {'shards': 8},
    )

  def testMultipleShardValuesFromQueryAlreadyAutoshardedIncrease(self):
    # Most used shard count is still the old 20 value, because it was recently
    # autosharded to 10
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=10,
            shard_count=20,
            most_used_shard_count=20,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=12,
            shard_count=10,
            most_used_shard_count=20,
        ),
    ])
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
        {'shards': 12},
    )

  def testMultipleShardValuesFromQueryNotAutoshardedNoChange(self):
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=10,
            shard_count=20,
            most_used_shard_count=10,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=10,
            shard_count=10,
            most_used_shard_count=10,
        ),
    ])
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

  def testMultipleShardValuesFromQueryNotAutoshardedSuggestedDecrease(self):
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=10,
            shard_count=20,
            most_used_shard_count=10,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=9,
            shard_count=10,
            most_used_shard_count=10,
        ),
    ])
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

  def testMultipleShardValuesFromQueryNotAutoshardedIncrease(self):
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=10,
            shard_count=20,
            most_used_shard_count=10,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='webview_instrumentation_test_apk',
            optimal_shard_count=11,
            shard_count=10,
            most_used_shard_count=10,
        ),
    ])
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
        {'shards': 11},
    )

  @mock.patch('query_optimal_shard_counts.TEST_SUITE_EXCLUDE_SET',
              new={'browser_tests', 'ui_tests'})
  def testTestSuiteExcludeSet(self):
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            optimal_shard_count=15,
            shard_count=10,
            most_used_shard_count=10,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.android',
            waterfall_builder_name='android-12-x64-rel',
            try_builder='android-12-x64-rel',
            test_suite='browser_tests',
            optimal_shard_count=25,
            shard_count=20,
            most_used_shard_count=20,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='ui_tests',
            optimal_shard_count=15,
            shard_count=10,
            most_used_shard_count=10,
        ),
    ])
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
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.mac',
            waterfall_builder_name='Mac Builder',
            try_builder='mac-rel',
            test_suite='browser_tests',
            optimal_shard_count=15,
            shard_count=10,
            most_used_shard_count=10,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='ui_tests',
            optimal_shard_count=15,
            shard_count=10,
            most_used_shard_count=10,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            optimal_shard_count=15,
            shard_count=12,
            most_used_shard_count=12,
        ),
    ])
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
    self._mock_check_output.return_value = json.dumps([
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='browser_tests',
            optimal_shard_count=15,
            shard_count=10,
            most_used_shard_count=10,
        ),
        query_response_test_suite_dict(
            waterfall_builder_group='chromium.linux',
            waterfall_builder_name='Linux Tests',
            try_builder='linux-rel',
            test_suite='ui_tests',
            optimal_shard_count=15,
            shard_count=10,
            most_used_shard_count=10,
        ),
    ])
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

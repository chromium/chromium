# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Please see the comments section at the top of the `run_performance_tests.py`.

import json
import os
import pathlib
import shutil
import tempfile
import unittest
from unittest import mock

import run_performance_tests
from run_performance_tests import TelemetryCommandGenerator
from telemetry.internal.util import binary_manager

# Protected access is allowed for unittests.
# pylint: disable=protected-access

# The path where the output of a wpt run was written. This is the file that
# gets processed by BaseWptScriptAdapter.
OUTPUT_JSON_FILENAME = 'out.json'


class TelemetryCommandGeneratorTest(unittest.TestCase):

  def setUp(self):
    fake_args = ['./run_benchmark', '--isolated-script-test-output=output.json']
    self._fake_options = run_performance_tests.parse_arguments(fake_args)
    mock.patch.object(binary_manager, 'InitDependencyManager').start()

  def testStorySelectionBeginEnd(self):
    story_selection_config = json.loads(
        '{"begin": 11, "end": 21, "abridged": false}')
    generator = TelemetryCommandGenerator('benchmark_name', self._fake_options,
                                          story_selection_config)
    command = generator.generate('output_dir')
    self.assertIn('--story-shard-begin-index=11', command)
    self.assertIn('--story-shard-end-index=21', command)
    self.assertNotIn('--run-abridged-story-set', command)

  def testStorySelectionAbridgedDefault(self):
    story_selection_config = json.loads('{"begin": 11, "end": 21}')
    generator = TelemetryCommandGenerator('benchmark_name', self._fake_options,
                                          story_selection_config)
    command = generator.generate('output_dir')
    self.assertIn('--run-abridged-story-set', command)

  def testStorySelectionIndexSectionsSingleIndex(self):
    story_selection_config = json.loads(
        '{"sections": [{"begin": 11, "end": 21}, {"begin": 25, "end": 26}]}')
    generator = TelemetryCommandGenerator('benchmark_name', self._fake_options,
                                          story_selection_config)
    command = generator.generate('output_dir')
    self.assertIn('--story-shard-indexes=11-21,25', command)

  def testStorySelectionIndexSectionsOpenEnds(self):
    story_selection_config = json.loads(
        '{"sections": [{"end": 10}, {"begin": 15, "end": 16}, {"begin": 20}]}')
    generator = TelemetryCommandGenerator('benchmark_name', self._fake_options,
                                          story_selection_config)
    command = generator.generate('output_dir')
    self.assertIn('--story-shard-indexes=-10,15,20-', command)

  def testStorySelectionIndexSectionsIllegalRange(self):
    with self.assertRaises(ValueError):
      story_selection_config = json.loads(
          '{"sections": [{"begin": 15, "end": 16}, {"foo": "bar"}]}')
      generator = TelemetryCommandGenerator('benchmark_name',
                                            self._fake_options,
                                            story_selection_config)
      generator.generate('output_dir')

  def testStorySelectionIndexSectionsEmpty(self):
    story_selection_config = json.loads('{"sections": []}')
    generator = TelemetryCommandGenerator('benchmark_name', self._fake_options,
                                          story_selection_config)
    command = generator.generate('output_dir')
    self.assertNotIn('--story-shard-indexes=', command)

  @mock.patch.object(os.path, 'exists')
  @mock.patch.object(run_performance_tests, 'copy_map_file_to_out_dir')
  @mock.patch('builtins.open',
              new_callable=mock.mock_open,
              read_data='{"foo": 1}')
  def testLoadMapFileSuccess(self, mock_open, mock_copy_map_file_to_out_dir,
                             mock_exists):
    del mock_open, mock_exists
    content = run_performance_tests.load_map_file('file', 'dir')

    self.assertTrue(isinstance(content, dict))
    self.assertEqual(content['foo'], 1)
    mock_copy_map_file_to_out_dir.assert_called_with('file', 'dir')

  @mock.patch.object(os.path, 'exists')
  @mock.patch.object(pathlib.Path, 'exists')
  @mock.patch.object(run_performance_tests, 'copy_map_file_to_out_dir')
  @mock.patch('builtins.open',
              new_callable=mock.mock_open,
              read_data='{"foo": 1}')
  def testLoadMapFileShardMapDirectory(self, mock_open,
                                       mock_copy_map_file_to_out_dir,
                                       mock_pathlib_exists, mock_exists):
    del mock_open
    mock_exists.return_value = False
    mock_pathlib_exists.return_value = True
    expected_file = str(run_performance_tests.SHARD_MAPS_DIR / 'file')

    run_performance_tests.load_map_file('file', 'dir')

    mock_copy_map_file_to_out_dir.assert_called_with(expected_file, 'dir')

  @mock.patch.object(os.path, 'exists')
  @mock.patch.object(run_performance_tests, 'copy_map_file_to_out_dir')
  @mock.patch('builtins.open',
              new_callable=mock.mock_open,
              read_data='{"foo": 1}')
  def testLoadMapFileException(self, mock_open, mock_copy_map_file_to_out_dir,
                               mock_exists):
    del mock_open, mock_copy_map_file_to_out_dir
    mock_exists.side_effect = [False, False]

    with self.assertRaises(Exception):
      run_performance_tests.load_map_file('file', 'dir')

  @mock.patch.object(run_performance_tests, 'copy_map_file_to_out_dir')
  @mock.patch.object(tempfile, 'NamedTemporaryFile')
  def testLoadMapStringSuccess(self, mock_named_temporary_file,
                               mock_copy_map_file_to_out_dir):
    del mock_named_temporary_file
    content = run_performance_tests.load_map_string('{"foo": 1}', 'dir')

    self.assertTrue(isinstance(content, dict))
    self.assertEqual(content['foo'], 1)
    mock_copy_map_file_to_out_dir.assert_called_with(mock.ANY, 'dir')

  @mock.patch.object(os.path, 'exists')
  @mock.patch.object(shutil, 'copyfile')
  def testCopyMapFileToOutDirSuccess(self, mock_copyfile, mock_exists):
    del mock_exists
    run_performance_tests.copy_map_file_to_out_dir('file', 'dir')

    mock_copyfile.assert_called_with('file', 'dir/benchmarks_shard_map.json')

  @mock.patch.object(run_performance_tests.CrossbenchTest, 'execute_benchmark')
  def testCrossbenchTestBenchmarksArg(self, mock_execute_benchmark):
    fake_args = self._create_crossbench_args()
    options = run_performance_tests.parse_arguments(fake_args)

    run_performance_tests.CrossbenchTest(options, 'dir').execute()

    mock_execute_benchmark.assert_called_with('speedometer_3.0',
                                              'speedometer3.crossbench', [])

  def testCrossbenchTestBenchmarksException(self):
    fake_args = ['./cp.py', '--isolated-script-test-output=output']
    options = run_performance_tests.parse_arguments(fake_args)

    with self.assertRaises(Exception):
      run_performance_tests.CrossbenchTest(options, 'dir').execute()

  def testCrossbenchTestMultiBenchmarksException(self):
    fake_args = [
        './cp.py', '--isolated-script-test-output=output',
        '--benchmarks=speedometer_3.0,speedometer_2.0'
    ]
    options = run_performance_tests.parse_arguments(fake_args)

    with self.assertRaises(Exception):
      run_performance_tests.CrossbenchTest(options, 'dir').execute()

  @mock.patch.object(run_performance_tests, '_run_benchmarks_on_shardmap')
  @mock.patch.object(os.path, 'dirname')
  @mock.patch.object(run_performance_tests, 'load_map_file')
  def testCrossbenchTestShardMapFile(self, mock_load_map_file, mock_direname,
                                     mock_run_benchmarks_on_shardmap):
    mock_load_map_file.return_value = 'map_file'
    mock_direname.return_value = 'dir'
    fake_args = [
        'skip', 'run_benchmark', '--isolated-script-test-output=output',
        '--test-shard-map-filename=foo'
    ]
    expected_options = run_performance_tests.parse_arguments(fake_args[1:])

    run_performance_tests.main(fake_args)

    mock_load_map_file.assert_called_with('foo', 'dir')
    mock_run_benchmarks_on_shardmap.assert_called_with('map_file',
                                                       expected_options, 'dir',
                                                       [])

  @mock.patch.object(run_performance_tests, '_run_benchmarks_on_shardmap')
  @mock.patch.object(os.path, 'dirname')
  @mock.patch.object(run_performance_tests, 'load_map_string')
  def testCrossbenchTestShardMapString(self, mock_load_map_string,
                                       mock_direname,
                                       mock_run_benchmarks_on_shardmap):
    mock_load_map_string.return_value = 'map_string'
    mock_direname.return_value = 'dir'
    fake_args = [
        'skip', 'run_benchmark', '--isolated-script-test-output=output',
        '--use-dynamic-shards', '--dynamic-shardmap=json'
    ]
    expected_options = run_performance_tests.parse_arguments(fake_args[1:])

    run_performance_tests.main(fake_args)

    mock_load_map_string.assert_called_with('json', 'dir')
    mock_run_benchmarks_on_shardmap.assert_called_with('map_string',
                                                       expected_options, 'dir',
                                                       [])

  @mock.patch.object(run_performance_tests.CrossbenchTest, 'execute_benchmark')
  @mock.patch.dict(os.environ, {'GTEST_SHARD_INDEX': '0'})
  def testCrossbenchTestRunBenchmarkOnShardMap(self, mock_execute_benchmark):
    fake_args = [
        'run_benchmark',
        '--isolated-script-test-output=output',
        '--test-shard-map-filename=foo',
        '--browser=./chrome',
    ]
    options = run_performance_tests.parse_arguments(fake_args)
    shard_map = {
        '0': {
            'crossbench': {
                'my_benchmark': {
                    'display_name': 'my_display',
                    'arguments': []
                }
            }
        }
    }
    mock_execute_benchmark.return_value = 0

    return_code = run_performance_tests._run_benchmarks_on_shardmap(
        shard_map, options, 'dir', [])

    self.assertEqual(return_code, 0)
    mock_execute_benchmark.assert_called_with('my_benchmark', 'my_display', [])

  @mock.patch.object(run_performance_tests.CrossbenchTest, 'execute_benchmark')
  def testCrossbenchTestMissingShardIndex(self, mock_execute_benchmark):
    del mock_execute_benchmark
    fake_args = [
        'run_benchmark', '--isolated-script-test-output=output',
        '--test-shard-map-filename=foo'
    ]
    options = run_performance_tests.parse_arguments(fake_args)
    shard_map = {'0': {'crossbench': {'my_benchmark': []}}}

    with self.assertRaises(Exception):
      run_performance_tests._run_benchmarks_on_shardmap(shard_map, options,
                                                        'dir', [])

  @mock.patch.object(run_performance_tests.CrossbenchTest, 'execute_benchmark')
  @mock.patch.dict(os.environ, {'GTEST_SHARD_INDEX': '0'})
  def testCrossbenchTestMissingBenchmark(self, mock_execute_benchmark):
    fake_args = [
        'run_benchmark',
        '--isolated-script-test-output=output',
        '--test-shard-map-filename=foo',
        '--browser=./chrome',
    ]
    options = run_performance_tests.parse_arguments(fake_args)
    shard_map = {'0': {'crossbench': {}}}

    return_code = run_performance_tests._run_benchmarks_on_shardmap(
        shard_map, options, 'dir', [])
    self.assertEqual(return_code, 0)
    mock_execute_benchmark.assert_not_called()

  @mock.patch.object(run_performance_tests.CrossbenchTest, 'execute_benchmark')
  @mock.patch.dict(os.environ, {'GTEST_SHARD_INDEX': '0'})
  def testCrossbenchTestRunMultiBenchmarkOnShardMap(self,
                                                    mock_execute_benchmark):
    fake_args = [
        'run_benchmark',
        '--isolated-script-test-output=output',
        '--test-shard-map-filename=foo',
        '--browser=./chrome',
    ]
    options = run_performance_tests.parse_arguments(fake_args)
    shard_map = {
        '0': {
            'crossbench': {
                'b1': {
                    'display_name': 'display1',
                    'arguments': []
                },
                'b2': {
                    'display_name': 'display2',
                    'arguments': []
                }
            }
        }
    }
    mock_execute_benchmark.return_value = 1

    return_code = run_performance_tests._run_benchmarks_on_shardmap(
        shard_map, options, 'dir', [])

    self.assertEqual(return_code, 1)
    mock_execute_benchmark.assert_has_calls(
        [mock.call('b1', 'display1', []),
         mock.call('b2', 'display2', [])])

  def testCrossbenchGetNetworkArgWithNetwork(self):
    fake_args = self._create_crossbench_args() + ['--network=foo']
    options = run_performance_tests.parse_arguments(fake_args)
    expected_network = ['--network=foo']

    crosebench_test = run_performance_tests.CrossbenchTest(options, 'dir')

    self.assertEqual(crosebench_test.network, expected_network)

  def testCrossbenchGetDefaultFileServer(self):
    fake_args = self._create_crossbench_args() + ['--fileserver']
    options = run_performance_tests.parse_arguments(fake_args)
    src_dir = run_performance_tests.CHROMIUM_SRC_DIR
    local_fileserver = str(src_dir / 'third_party/speedometer/v3.0')
    expected_dict = {
        'type': 'local',
        'path': local_fileserver,
        'url': 'http://localhost:8000'
    }

    crosebench_test = run_performance_tests.CrossbenchTest(options, 'dir')

    network_dict = json.loads(crosebench_test.network[0].split('=', 1)[1])
    self.assertDictEqual(network_dict, expected_dict)

  def testCrossbenchGetTargetFileServer(self):
    fake_args = self._create_crossbench_args() + ['--fileserver=foo']
    options = run_performance_tests.parse_arguments(fake_args)
    src_dir = run_performance_tests.CHROMIUM_SRC_DIR
    local_fileserver = str(src_dir / 'foo')
    expected_dict = {
        'type': 'local',
        'path': local_fileserver,
        'url': 'http://localhost:8000'
    }

    crosebench_test = run_performance_tests.CrossbenchTest(options, 'dir')

    network_dict = json.loads(crosebench_test.network[0].split('=', 1)[1])
    self.assertDictEqual(network_dict, expected_dict)

  @mock.patch.object(binary_manager, 'FetchPath')
  def testCrossbenchGetDefaultWpr(self, mock_fetch_path):
    mock_fetch_path.return_value = 'wpr_go_path'
    fake_args = self._create_crossbench_args() + ['--wpr']
    options = run_performance_tests.parse_arguments(fake_args)
    data_dir = run_performance_tests.PAGE_SETS_DATA
    archive = str(data_dir / 'crossbench_android_speedometer_3.0_000.wprgo')
    expected_dict = {
        'type': 'wpr',
        'path': archive,
        'wpr_go_bin': 'wpr_go_path'
    }

    crosebench_test = run_performance_tests.CrossbenchTest(options, 'dir')

    network_dict = json.loads(crosebench_test.network[0].split('=', 1)[1])
    self.assertDictEqual(network_dict, expected_dict)

  @mock.patch.object(binary_manager, 'FetchPath')
  def testCrossbenchGetTargetWpr(self, mock_fetch_path):
    mock_fetch_path.return_value = 'wpr_go_path'
    fake_args = self._create_crossbench_args() + ['--wpr=foo']
    options = run_performance_tests.parse_arguments(fake_args)
    data_dir = run_performance_tests.PAGE_SETS_DATA
    archive = str(data_dir / 'foo')
    expected_dict = {
        'type': 'wpr',
        'path': archive,
        'wpr_go_bin': 'wpr_go_path'
    }

    crosebench_test = run_performance_tests.CrossbenchTest(options, 'dir')

    network_dict = json.loads(crosebench_test.network[0].split('=', 1)[1])
    self.assertDictEqual(network_dict, expected_dict)

  def _create_crossbench_args(self, browser='./chrome'):
    return [
        './cp.py',
        '--isolated-script-test-output=output',
        '--benchmarks=speedometer_3.0',
        '--benchmark-display-name=speedometer3.crossbench',
        f'--browser={browser}',
    ]

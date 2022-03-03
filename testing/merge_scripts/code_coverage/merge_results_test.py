#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json
import os
import subprocess
import sys
import unittest

import mock

import merge_results
import merge_steps
import merge_lib as merger


class MergeProfilesTest(unittest.TestCase):

  # pylint: disable=super-with-arguments
  def __init__(self, *args, **kwargs):
    super(MergeProfilesTest, self).__init__(*args, **kwargs)
    self.maxDiff = None
  # pylint: enable=super-with-arguments

  def test_merge_script_api_parameters(self):
    """Test the step-level merge front-end."""
    build_properties = json.dumps({
        'some': {
            'complicated': ['nested', {
                'json': None,
                'object': 'thing',
            }]
        }
    })
    task_output_dir = 'some/task/output/dir'
    profdata_dir = '/some/different/path/to/profdata/default.profdata'
    profdata_file = os.path.join(profdata_dir, 'base_unittests.profdata')
    args = [
        'script_name', '--output-json', 'output.json', '--build-properties',
        build_properties, '--summary-json', 'summary.json', '--task-output-dir',
        task_output_dir, '--profdata-dir', profdata_dir, '--llvm-profdata',
        'llvm-profdata', 'a.json', 'b.json', 'c.json', '--test-target-name',
        'base_unittests', '--sparse'
    ]
    with mock.patch.object(merger, 'merge_profiles') as mock_merge:
      mock_merge.return_value = None, None
      with mock.patch.object(sys, 'argv', args):
        merge_results.main()
        self.assertEqual(
            mock_merge.call_args,
            mock.call(task_output_dir, profdata_file, '.profraw',
                      'llvm-profdata', sparse=True,
                      skip_validation=False), None)

  def test_merge_steps_parameters(self):
    """Test the build-level merge front-end."""
    input_dir = 'some/task/output/dir'
    output_file = '/some/different/path/to/profdata/merged.profdata'
    args = [
        'script_name',
        '--input-dir',
        input_dir,
        '--output-file',
        output_file,
        '--llvm-profdata',
        'llvm-profdata',
        '--profdata-filename-pattern',
        '.*'
    ]
    with mock.patch.object(merger, 'merge_profiles') as mock_merge:
      mock_merge.return_value = None
      with mock.patch.object(sys, 'argv', args):
        merge_steps.main()
        self.assertEqual(
            mock_merge.call_args,
            mock.call(input_dir, output_file, '.profdata', 'llvm-profdata',
                '.*', sparse=False))

  @mock.patch.object(merger, '_validate_and_convert_profraws')
  def test_merge_profraw(self, mock_validate_and_convert_profraws):
    mock_input_dir_walk = [
        ('/b/some/path', ['0', '1', '2', '3'], ['summary.json']),
        ('/b/some/path/0', [],
         ['output.json', 'default-1.profraw', 'default-2.profraw']),
        ('/b/some/path/1', [],
         ['output.json', 'default-1.profraw', 'default-2.profraw']),
    ]

    mock_validate_and_convert_profraws.return_value = [
        '/b/some/path/0/default-1.profdata',
        '/b/some/path/1/default-2.profdata',
    ], [
        '/b/some/path/0/default-2.profraw',
        '/b/some/path/1/default-1.profraw',
    ], [
        '/b/some/path/1/default-1.profraw',
    ]

    with mock.patch.object(os, 'walk') as mock_walk:
      with mock.patch.object(os, 'remove'):
        mock_walk.return_value = mock_input_dir_walk
        with mock.patch.object(subprocess, 'check_call') as mock_exec_cmd:
          merger.merge_profiles('/b/some/path', 'output/dir/default.profdata',
                                '.profraw', 'llvm-profdata')
          self.assertEqual(
              mock.call(
                  [
                      'llvm-profdata',
                      'merge',
                      '-o',
                      'output/dir/default.profdata',
                      '/b/some/path/0/default-1.profdata',
                      '/b/some/path/1/default-2.profdata',
                  ],
                  stderr=-2,
              ), mock_exec_cmd.call_args)

    self.assertTrue(mock_validate_and_convert_profraws.called)

  @mock.patch.object(merger, '_validate_and_convert_profraws')
  def test_profraw_skip_validation(self, mock_validate_and_convert_profraws):
    mock_input_dir_walk = [
        ('/b/some/path', ['0', '1', '2', '3'], ['summary.json']),
        ('/b/some/path/0', [],
         ['output.json', 'default-1.profraw', 'default-2.profraw']),
        ('/b/some/path/1', [],
         ['output.json', 'default-1.profraw', 'default-2.profraw']),
    ]

    with mock.patch.object(os, 'walk') as mock_walk:
      with mock.patch.object(os, 'remove'):
        mock_walk.return_value = mock_input_dir_walk
        with mock.patch.object(subprocess, 'check_call') as mock_exec_cmd:
          merger.merge_profiles('/b/some/path',
                                'output/dir/default.profdata',
                                '.profraw',
                                'llvm-profdata',
                                skip_validation=True)
          self.assertEqual(
              mock.call(
                  [
                      'llvm-profdata',
                      'merge',
                      '-o',
                      'output/dir/default.profdata',
                      '/b/some/path/0/default-1.profraw',
                      '/b/some/path/0/default-2.profraw',
                      '/b/some/path/1/default-1.profraw',
                      '/b/some/path/1/default-2.profraw'
                  ],
                  stderr=-2,
              ), mock_exec_cmd.call_args)

    # Skip validation should've passed all profraw files directly, and
    # this validate call should not have been invoked.
    self.assertFalse(mock_validate_and_convert_profraws.called)


  def test_merge_profraw_skip_if_there_is_no_file(self):
    mock_input_dir_walk = [
        ('/b/some/path', ['0', '1', '2', '3'], ['summary.json']),
    ]

    with mock.patch.object(os, 'walk') as mock_walk:
      mock_walk.return_value = mock_input_dir_walk
      with mock.patch.object(subprocess, 'check_call') as mock_exec_cmd:
        merger.merge_profiles('/b/some/path', 'output/dir/default.profdata',
                              '.profraw', 'llvm-profdata')
        self.assertFalse(mock_exec_cmd.called)


  @mock.patch.object(merger, '_validate_and_convert_profraws')
  def test_merge_profdata(self, mock_validate_and_convert_profraws):
    mock_input_dir_walk = [
        ('/b/some/path', ['base_unittests', 'url_unittests'], ['summary.json']),
        ('/b/some/path/base_unittests', [], ['output.json',
                                             'default.profdata']),
        ('/b/some/path/url_unittests', [], ['output.json', 'default.profdata']),
    ]
    with mock.patch.object(os, 'walk') as mock_walk:
      with mock.patch.object(os, 'remove'):
        mock_walk.return_value = mock_input_dir_walk
        with mock.patch.object(subprocess, 'check_call') as mock_exec_cmd:
          merger.merge_profiles('/b/some/path', 'output/dir/default.profdata',
                                '.profdata', 'llvm-profdata')
          self.assertEqual(
              mock.call(
                  [
                      'llvm-profdata',
                      'merge',
                      '-o',
                      'output/dir/default.profdata',
                      '/b/some/path/base_unittests/default.profdata',
                      '/b/some/path/url_unittests/default.profdata',
                  ],
                  stderr=-2,
              ), mock_exec_cmd.call_args)

    # The mock method should only apply when merging .profraw files.
    self.assertFalse(mock_validate_and_convert_profraws.called)

  @mock.patch.object(merger, '_validate_and_convert_profraws')
  def test_merge_profdata_pattern(self, mock_validate_and_convert_profraws):
    mock_input_dir_walk = [
        ('/b/some/path', ['base_unittests', 'url_unittests'], ['summary.json']),
        ('/b/some/path/base_unittests', [], ['output.json',
                                             'base_unittests.profdata']),
        ('/b/some/path/url_unittests', [], ['output.json',
                                            'url_unittests.profdata'],),
        ('/b/some/path/ios_chrome_smoke_eg2tests',
            [], ['output.json','ios_chrome_smoke_eg2tests.profdata'],),
    ]
    with mock.patch.object(os, 'walk') as mock_walk:
      with mock.patch.object(os, 'remove'):
        mock_walk.return_value = mock_input_dir_walk
        with mock.patch.object(subprocess, 'check_call') as mock_exec_cmd:
          input_profdata_filename_pattern = '.+_unittests\.profdata'
          merger.merge_profiles('/b/some/path',
                                'output/dir/default.profdata',
                                '.profdata',
                                'llvm-profdata',
                                input_profdata_filename_pattern)
          self.assertEqual(
              mock.call(
                  [
                      'llvm-profdata',
                      'merge',
                      '-o',
                      'output/dir/default.profdata',
                      '/b/some/path/base_unittests/base_unittests.profdata',
                      '/b/some/path/url_unittests/url_unittests.profdata',
                  ],
                  stderr=-2,
              ), mock_exec_cmd.call_args)

    # The mock method should only apply when merging .profraw files.
    self.assertFalse(mock_validate_and_convert_profraws.called)

  @mock.patch('merge_lib._JAVA_PATH', 'java')
  def test_merge_java_exec_files(self):
    mock_input_dir_walk = [
        ('/b/some/path', ['0', '1', '2', '3'], ['summary.json']),
        ('/b/some/path/0', [],
         ['output.json', 'default-1.exec', 'default-2.exec']),
        ('/b/some/path/1', [],
         ['output.json', 'default-3.exec', 'default-4.exec']),
    ]

    with mock.patch.object(os, 'walk') as mock_walk:
      mock_walk.return_value = mock_input_dir_walk
      with mock.patch.object(subprocess, 'check_call') as mock_exec_cmd:
        merger.merge_java_exec_files(
            '/b/some/path', 'output/path', 'path/to/jacococli.jar')
        self.assertEqual(
            mock.call(
                [
                    'java',
                    '-jar',
                    'path/to/jacococli.jar',
                    'merge',
                    '/b/some/path/0/default-1.exec',
                    '/b/some/path/0/default-2.exec',
                    '/b/some/path/1/default-3.exec',
                    '/b/some/path/1/default-4.exec',
                    '--destfile',
                    'output/path',
                ],
                stderr=-2,
            ), mock_exec_cmd.call_args)

  def test_merge_java_exec_files_if_there_is_no_file(self):
    mock_input_dir_walk = [
        ('/b/some/path', ['0', '1', '2', '3'], ['summary.json']),
    ]

    with mock.patch.object(os, 'walk') as mock_walk:
      mock_walk.return_value = mock_input_dir_walk
      with mock.patch.object(subprocess, 'check_call') as mock_exec_cmd:
        merger.merge_java_exec_files(
            '/b/some/path', 'output/path', 'path/to/jacococli.jar')
        self.assertFalse(mock_exec_cmd.called)

  def test_calls_merge_js_results_script(self):
    task_output_dir = 'some/task/output/dir'
    profdata_dir = '/some/different/path/to/profdata/default.profdata'

    args = [
        'script_name', '--output-json', 'output.json', '--task-output-dir',
        task_output_dir, '--profdata-dir', profdata_dir, '--llvm-profdata',
        'llvm-profdata', 'a.json', 'b.json', 'c.json', '--test-target-name',
        'v8_unittests', '--sparse',
        '--javascript-coverage-dir', 'output/dir/devtools_code_coverage',
        '--merged-js-cov-filename', 'path/js/cov/filename'
    ]
    with mock.patch.object(merger, 'merge_profiles') as mock_merge:
      mock_merge.return_value = None, None
      with mock.patch.object(sys, 'argv', args):
        with mock.patch.object(subprocess, 'call') as mock_exec_cmd:
          with mock.patch.object(os.path, 'join') as mock_os_path_join:
            mock_merge_js_results_path = 'path/to/js/merge_js_results.py'
            mock_os_path_join.return_value = mock_merge_js_results_path
            python_exec = sys.executable
            merge_results.main()

            mock_exec_cmd.assert_called_with(
                [python_exec, mock_merge_js_results_path, '--task-output-dir',
                 task_output_dir, '--javascript-coverage-dir',
                 'output/dir/devtools_code_coverage',
                 '--merged-js-cov-filename', 'path/js/cov/filename'])

  def test_argparse_sparse(self):
    """Ensure that sparse flag defaults to true, and is set to correct value"""
    # Basic required args
    build_properties = json.dumps({
        'some': {
            'complicated': ['nested', {
                'json': None,
                'object': 'thing',
            }]
        }
    })
    task_output_dir = 'some/task/output/dir'
    profdata_dir = '/some/different/path/to/profdata/default.profdata'
    profdata_file = os.path.join(profdata_dir, 'base_unittests.profdata')
    args = [
        'script_name', '--output-json', 'output.json', '--build-properties',
        build_properties, '--summary-json', 'summary.json', '--task-output-dir',
        task_output_dir, '--profdata-dir', profdata_dir, '--llvm-profdata',
        'llvm-profdata', 'a.json', 'b.json', 'c.json', '--test-target-name',
        'base_unittests'
    ]

    test_scenarios = [
      {
        # Base set of args should set --sparse to false by default
        'args': None,
        'expected_outcome': False,
      },
      {
        # Sparse should parse True when only --sparse is specified
        'args': ['--sparse'],
        'expected_outcome': True,
      }
    ]

    for scenario in test_scenarios:
      args = copy.deepcopy(args)
      additional_args = scenario['args']
      if additional_args:
        args.extend(additional_args)
      expected_outcome = scenario['expected_outcome']

      with mock.patch.object(merger, 'merge_profiles') as mock_merge:
        mock_merge.return_value = None, None
        with mock.patch.object(sys, 'argv', args):
          merge_results.main()
          self.assertEqual(
              mock_merge.call_args,
              mock.call(task_output_dir, profdata_file, '.profraw',
                        'llvm-profdata', sparse=expected_outcome,
                        skip_validation=False), None)


if __name__ == '__main__':
  unittest.main()

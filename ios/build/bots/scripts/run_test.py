#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for run.py."""

import json
import re
import unittest

import run


class UnitTest(unittest.TestCase):

  def test_parse_args_ok(self):
    cmd = [
        '--app',
        './foo-Runner.app',
        '--host-app',
        './bar.app',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir'
    ]

    runner = run.Runner()
    runner.parse_args(cmd)
    self.assertTrue(runner.args.app == './foo-Runner.app')

  def test_parse_args_iossim_platform_version(self):
    """
    iossim, platforma and version should all be set.
    missing iossim
    """
    test_cases = [
        {
            'error':
                2,
            'cmd': [
                '--platform',
                'iPhone X',
                '--version',
                '13.2.2',

                # Required
                '--xcode-build-version',
                '123abc',
                '--out-dir',
                'some/dir'
            ],
        },
        {
            'error':
                2,
            'cmd': [
                '--iossim',
                'path/to/iossim',
                '--version',
                '13.2.2',

                # Required
                '--xcode-build-version',
                '123abc',
                '--out-dir',
                'some/dir'
            ],
        },
        {
            'error':
                2,
            'cmd': [
                '--iossim',
                'path/to/iossim',
                '--platform',
                'iPhone X',

                # Required
                '--xcode-build-version',
                '123abc',
                '--out-dir',
                'some/dir'
            ],
        },
    ]

    runner = run.Runner()
    for test_case in test_cases:
      with self.assertRaises(SystemExit) as ctx:
        runner.parse_args(test_case['cmd'])
        self.assertTrue(re.match('must specify all or none of *', ctx.message))
        self.assertEqual(ctx.exception.code, test_case['error'])

  def test_parse_args_xcode_parallelization_requirements(self):
    """
    xcode parallelization set requires both platform and version
    """
    test_cases = [
        {
            'error':
                2,
            'cmd': [
                '--xcode-parallelization',
                '--platform',
                'iPhone X',

                # Required
                '--xcode-build-version',
                '123abc',
                '--out-dir',
                'some/dir'
            ]
        },
        {
            'error':
                2,
            'cmd': [
                '--xcode-parallelization',
                '--version',
                '13.2.2',

                # Required
                '--xcode-build-version',
                '123abc',
                '--out-dir',
                'some/dir'
            ]
        }
    ]

    runner = run.Runner()
    for test_case in test_cases:
      with self.assertRaises(SystemExit) as ctx:
        runner.parse_args(test_case['cmd'])
        self.assertTrue(
            re.match('--xcode-parallelization also requires both *',
                     ctx.message))
        self.assertEqual(ctx.exception.code, test_case['error'])

  def test_parse_args_from_json(self):
    json_args = {
        'test_cases': ['test1'],
        'restart': 'true',
        'xcode_parallelization': True,
        'shards': 2
    }

    cmd = [
        '--shards',
        '1',
        '--platform',
        'iPhone X',
        '--version',
        '13.2.2',
        '--args-json',
        json.dumps(json_args),

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir'
    ]

    # shards should be 2, since json arg takes precedence over cmd line
    runner = run.Runner()
    runner.parse_args(cmd)
    # Empty array
    self.assertEquals(len(runner.args.env_var), 0)
    self.assertTrue(runner.args.xcode_parallelization)
    self.assertTrue(runner.args.restart)
    self.assertEquals(runner.args.shards, 2)


if __name__ == '__main__':
  unittest.main()

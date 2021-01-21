#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for base_device_trigger.py."""

import argparse
import json
import unittest

import mock

from pyfakefs import fake_filesystem_unittest

import base_test_triggerer


class UnitTest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()

  def test_convert_to_go_swarming_args(self):
    args = [
        '--swarming', 'x.apphost.com', '--dimension', 'pool', 'ci',
        '--dimension', 'os', 'linux', '--env', 'FOO', 'foo', '--hello',
        '--cipd-package', 'path:name:123', '--scalar', '42',
        '--optional-dimension', 'os', 'ubuntu', '60'
    ]
    go_args = base_test_triggerer._convert_to_go_swarming_args(args)
    expected = [
        '--server', 'x.apphost.com', '--dimension', 'pool=ci', '--dimension',
        'os=linux', '--env', 'FOO=foo', '--hello', '--cipd-package',
        'path:name=123', '--scalar', '42', '--optional-dimension',
        'os=ubuntu:60'
    ]
    self.assertEquals(go_args, expected)

  def test_convert_to_go_swarming_args_failed(self):
    invalid_args = [
        # expected format: --dimension key value
        ([
            '--dimension',
            'key',
        ], IndexError),
        # expected format: --env key value
        ([
            '--env',
            'key',
        ], IndexError),
        # expected format: --cipd-package path:name:version
        (['--cipd-package', 'path:name'], ValueError),
        # expected format: --optional-dimension key value expiry
        (['--optional-dimension', 'key', 'value'], ValueError),
        # can only specify one optional dimension
        ([
            '--optional-dimension',
            'k1',
            'v1',
            '123',
            '--optional-dimension',
            'k2',
            'v2',
            '456',
        ], AssertionError),
    ]
    for args, ex in invalid_args:
      self.assertRaises(ex, base_test_triggerer._convert_to_go_swarming_args,
                        args)

  def test_run_swarming_go(self):
    triggerer = base_test_triggerer.BaseTestTriggerer()

    dump_json = 'dump_json'
    with open(dump_json, 'w') as f:
      f.write(json.dumps({
        'tasks': [{
          'request': {
            'task_id': 'f0',
          },
        }]
      }))

    with mock.patch('subprocess.call', return_value=0) as mock_call:
      triggerer.run_swarming_go([], False, dump_json, 0, 1, {})
      mock_call.assert_called_once()

    with open(dump_json) as f:
      self.assertEqual(
        json.load(f),
        {
          u'tasks': {
            u'f0:0:1': {
              u'shard_index': 0,
              u'task_id': u'f0'
            }
          }
        })


  def test_arg_parser(self):
    # Added for https://crbug.com/1143224
    parser = argparse.ArgumentParser()
    base_test_triggerer.BaseTestTriggerer.add_use_swarming_go_arg(parser)
    swarming_args = ['--server', 'x.apphost.com', '--dimension', 'os', 'Linux']
    args, _ = parser.parse_known_args(swarming_args)
    self.assertFalse(args.use_swarming_go)

    args, _ = parser.parse_known_args(swarming_args + ['--use-swarming-go'])
    self.assertTrue(args.use_swarming_go)

if __name__ == '__main__':
    unittest.main()

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for base_device_trigger.py."""

import unittest

import base_test_triggerer


class UnitTest(unittest.TestCase):
  def test_convert_to_go_swarming_args(self):
    args = [
        '--dimension', 'pool', 'ci', '--dimension', 'os', 'linux', '--env',
        'FOO', 'foo', '--hello', '--cipd-package', 'path:name:123', '--scalar',
        '42', '--optional-dimension', 'os', 'ubuntu', '60'
    ]
    go_args = base_test_triggerer._convert_to_go_swarming_args(args)
    expected = [
        '--dimension', 'pool=ci', '--dimension', 'os=linux', '--env', 'FOO=foo',
        '--hello', '--cipd-package', 'path:name=123', '--scalar', '42',
        '--optional-dimension', 'os=ubuntu:60'
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

if __name__ == '__main__':
    unittest.main()

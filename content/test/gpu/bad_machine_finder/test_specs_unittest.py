# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from bad_machine_finder import test_specs


class GetMixinDimensionsUnittest(unittest.TestCase):

  def testBasic(self):
    """Tests behavior along the basic/happy path."""
    mixins = {
        'linux_amd': {
            'swarming': {
                'dimensions': {
                    'os': 'linux',
                    'gpu': '1002:1234',
                },
            },
        },
        'mac_amd': {
            'swarming': {
                'dimensions': {
                    'os': 'mac',
                    'gpu': '1002:2345',
                },
            },
        },
    }
    with mock.patch.object(test_specs, '_LoadPylFile', return_value=mixins):
      dimensions = test_specs.GetMixinDimensions('mac_amd')
    self.assertEqual(dimensions.AsDict(), {'os': ['mac'], 'gpu': ['1002:2345']})

  def testSwarmingOrOperator(self):
    """Tests behavior when dimensions contain |."""
    mixins = {
        'mac_amd': {
            'swarming': {
                'dimensions': {
                    'os': 'Mac-14.5|Mac-15.0',
                    'gpu': '1002:2345',
                },
            },
        },
    }
    with mock.patch.object(test_specs, '_LoadPylFile', return_value=mixins):
      dimensions = test_specs.GetMixinDimensions('mac_amd')
    self.assertEqual(dimensions.AsDict(), {
        'os': ['Mac-14.5', 'Mac-15.0'],
        'gpu': ['1002:2345']
    })

  def testNoDimensions(self):
    """Tests behavior when dimensions can not be found."""
    cases = [
        # No matching mixin.
        {},
        # No swarming key.
        {
            'mac_amd': {},
        },
        # No dimensions key.
        {
            'mac_amd': {
                'swarming': {},
            },
        },
        # Empty dimensions.
        {
            'mac_amd': {
                'swarming': {
                    'dimensions': {},
                },
            },
        },
    ]
    for c in cases:
      with mock.patch.object(test_specs, '_LoadPylFile', return_value=c):
        with self.assertRaisesRegex(
            RuntimeError,
            'Specified mixin mac_amd does not contain Swarming dimensions'):
          test_specs.GetMixinDimensions('mac_amd')

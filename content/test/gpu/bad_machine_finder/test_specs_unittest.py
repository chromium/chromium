# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from bad_machine_finder import test_specs


class GetBuildersWithMixinUnittest(unittest.TestCase):

  def testBasic(self):
    """Tests behavior along the basic/happy path."""
    waterfalls = [
        {
            'name': 'chromium.dawn',
            'machines': {
                'Dawn Mac AMD': {
                    'mixins': [
                        'mac_amd',
                    ],
                },
                'Dawn Builder': {},
            },
        },
        {
            'name': 'chromium.gpu.fyi',
            'machines': {
                'Mac AMD': {
                    'mixins': ['mac_amd'],
                },
                'Mac Intel': {
                    'mixins': [
                        'mac_intel',
                    ],
                },
            },
        },
    ]
    with mock.patch.object(test_specs, '_LoadPylFile', return_value=waterfalls):
      builders = test_specs.GetBuildersWithMixin('mac_amd')
    self.assertEqual(builders, ['Dawn Mac AMD', 'Mac AMD'])

#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.web_tests.stale_expectation_removal import builders


class BuilderRunsTestOfInterestUnittest(unittest.TestCase):
    def setUp(self):
        self.instance = builders.WebTestBuilders()

    def testMatch(self):
        """Tests that a match can be successfully found."""
        test_map = {
            'isolated_scripts': [
                {
                    'isolate_name': 'blink_web_tests',
                },
            ],
        }
        self.assertTrue(
            self.instance._BuilderRunsTestOfInterest(test_map, None))

        # Re-add once WebGPU tests are supported.
        # test_map = {
        #     'isolated_scripts': [
        #         {
        #             'isolate_name': 'webgpu_blink_web_tests',
        #         },
        #     ],
        # }
        # self.assertTrue(
        #     self.instance._BuilderRunsTestOfInterest(test_map, None))

    def testNoMatch(self):
        test_map = {
            'isolated_scripts': [
                {
                    'isolate_name': 'foo_web_tests',
                },
            ],
        }
        self.assertFalse(
            self.instance._BuilderRunsTestOfInterest(test_map, None))


if __name__ == '__main__':
    unittest.main(verbosity=2)

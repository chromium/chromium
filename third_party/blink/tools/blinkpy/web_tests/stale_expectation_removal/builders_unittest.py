#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.web_tests.stale_expectation_removal import builders
from unexpected_passes_common import constants
from unexpected_passes_common import data_types


class BuilderRunsTestOfInterestUnittest(unittest.TestCase):
    def setUp(self) -> None:
        self.instance = builders.WebTestBuilders(False)

    def testMatch(self) -> None:
        """Tests that a match can be successfully found."""
        test_map = {
            'isolated_scripts': [
                {
                    'test': 'blink_web_tests',
                },
            ],
        }
        self.assertTrue(self.instance._BuilderRunsTestOfInterest(test_map))

        # Re-add once WebGPU tests are supported.
        # test_map = {
        #     'isolated_scripts': [
        #         {
        #             'isolate_name': 'webgpu_blink_web_tests',
        #         },
        #     ],
        # }
        # self.assertTrue(
        #     self.instance._BuilderRunsTestOfInterest(test_map))

    def testNoMatch(self) -> None:
        test_map = {
            'isolated_scripts': [
                {
                    'test': 'foo_web_tests',
                },
            ],
        }
        self.assertFalse(self.instance._BuilderRunsTestOfInterest(test_map))


class GetFakeCiBuildersUnittest(unittest.TestCase):
    def testStringsConvertedToBuilderEntries(self) -> None:
        """Tests that the easier-to-read strings get converted to BuilderEntry."""
        instance = builders.WebTestBuilders(False)
        fake_builders = instance.GetFakeCiBuilders()
        ci_builder = data_types.BuilderEntry('linux-blink-rel-dummy',
                                             constants.BuilderTypes.CI, False)
        expected_try = set([
            data_types.BuilderEntry('linux-blink-rel',
                                    constants.BuilderTypes.TRY, False),
            data_types.BuilderEntry('v8_linux_blink_rel',
                                    constants.BuilderTypes.TRY, False)
        ])
        self.assertEqual(fake_builders[ci_builder], expected_try)


class GetNonChromiumBuildersUnittest(unittest.TestCase):
    def testStringsConvertedToBuilderEntries(self) -> None:
        """Tests that the easier-to-read strings get converted to BuilderEntry."""
        instance = builders.WebTestBuilders(False)
        builder = data_types.BuilderEntry('ToTMacOfficial',
                                          constants.BuilderTypes.CI, False)
        self.assertIn(builder, instance.GetNonChromiumBuilders())


if __name__ == '__main__':
    unittest.main(verbosity=2)

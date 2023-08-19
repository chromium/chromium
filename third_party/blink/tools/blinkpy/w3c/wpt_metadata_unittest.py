# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import textwrap
import unittest
from blinkpy.common import path_finder
from blinkpy.w3c import wpt_metadata

path_finder.bootstrap_wpt_imports()
from wptrunner import manifestexpected
from wptrunner.wptmanifest.backends import static


def _compile(contents: bytes, run_info=None) -> manifestexpected.TestNode:
    contents = textwrap.dedent(contents.decode()).encode()
    expectations = static.compile(io.BytesIO(contents), (run_info or {}),
                                  manifestexpected.data_cls_getter,
                                  test_path='/path/to/test.html')
    return expectations.get_test('test.html')


class WPTMetadataUnittest(unittest.TestCase):
    def test_fill_implied_expectations_testharness(self):
        test = _compile(b"""\
            [test.html]
              [subtest]
            """)
        wpt_metadata.fill_implied_expectations(test, {'extra-subtest'},
                                               'testharness')

        self.assertEqual(test.expected, 'OK')
        self.assertEqual(test.known_intermittent, [])
        self.assertEqual(set(test.subtests), {'subtest', 'extra-subtest'})
        self.assertEqual(test.get_subtest('subtest').expected, 'PASS')
        self.assertEqual(test.get_subtest('subtest').known_intermittent, [])
        self.assertEqual(test.get_subtest('extra-subtest').expected, 'PASS')
        self.assertEqual(
            test.get_subtest('extra-subtest').known_intermittent, [])

    def test_fill_implied_expectations_reftest(self):
        test = _compile(b"""\
            [test.html]
            """)
        wpt_metadata.fill_implied_expectations(test, test_type='reftest')
        self.assertEqual(test.expected, 'PASS')
        self.assertEqual(test.known_intermittent, [])

    def test_fill_implied_expectations_noop(self):
        test = _compile(b"""\
            [test.html]
              expected: [ERROR, OK]
              [subtest]
                expected: FAIL
            """)
        wpt_metadata.fill_implied_expectations(test)
        self.assertEqual(test.expected, 'ERROR')
        self.assertEqual(test.known_intermittent, ['OK'])
        self.assertEqual(test.get_subtest('subtest').expected, 'FAIL')
        self.assertEqual(test.get_subtest('subtest').known_intermittent, [])

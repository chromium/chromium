# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.web_tests.models import typ_types


class ExpectationUnittest(unittest.TestCase):

    def testSpaceEncoding(self):
        e = typ_types.Expectation(reason='crbug.com/1234',
                                  test='test.html?foo bar',
                                  tags=['win'],
                                  results={typ_types.ResultType.Failure})
        self.assertEqual(
            e.to_string(),
            'crbug.com/1234 [ Win ] test.html?foo%20bar [ Failure ]')

    def testPercentEncoding(self):
        e = typ_types.Expectation(reason='crbug.com/1234',
                                  test='test.html?foo%bar',
                                  tags=['win'],
                                  results={typ_types.ResultType.Failure})
        self.assertEqual(
            e.to_string(),
            'crbug.com/1234 [ Win ] test.html?foo%25bar [ Failure ]')


class TestExpectationsUnittest(unittest.TestCase):

    def testSpaceDecoding(self):
        content = """\
# tags: [ Mac ]
# results: [ Skip ]
crbug.com/123 [ Mac ] http://google.com/Foo%20Bar [ Skip ]
"""
        te = typ_types.TestExpectations()
        ret, _ = te.parse_tagged_list(content)
        self.assertEqual(ret, 0)
        self.assertEqual(len(te.individual_exps), 1)
        self.assertIn('http://google.com/Foo Bar', te.individual_exps)
        expected_expectation = typ_types.Expectation(
            reason='crbug.com/123',
            test='http://google.com/Foo Bar',
            tags=['mac'],
            results={typ_types.ResultType.Skip},
            lineno=3)
        self.assertEqual(te.individual_exps['http://google.com/Foo Bar'],
                         [expected_expectation])

    def testPercentDecoding(self):
        content = """\
# tags: [ Mac ]
# results: [ Skip ]
crbug.com/123 [ Mac ] http://google.com/Foo%2520Bar [ Skip ]
"""
        te = typ_types.TestExpectations()
        ret, _ = te.parse_tagged_list(content)
        self.assertEqual(ret, 0)
        self.assertEqual(len(te.individual_exps), 1)
        self.assertIn('http://google.com/Foo%20Bar', te.individual_exps)
        expected_expectation = typ_types.Expectation(
            reason='crbug.com/123',
            test='http://google.com/Foo%20Bar',
            tags=['mac'],
            results={typ_types.ResultType.Skip},
            lineno=3)
        self.assertEqual(te.individual_exps['http://google.com/Foo%20Bar'],
                         [expected_expectation])

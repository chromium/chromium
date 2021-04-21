#!/usr/bin/env vpython

# Copyright (C) 2021 Google Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import copy
import io
import unittest

from collections import namedtuple
from diff_wpt_results import (
    map_tests_to_results, WPTResultsDiffer, CSV_HEADING)

MockArgs = namedtuple('MockArgs', ['product_to_compare', 'baseline_product'])
TEST_PRODUCT = 'android_weblayer'
TEST_BASELINE_PRODUCT = 'chrome_android'


class MockWPTResultsDiffer(WPTResultsDiffer):

    def __init__(self, actual_results_map, baseline_results_map, csv_output):
        super(MockWPTResultsDiffer, self).__init__(
            MockArgs(product_to_compare=TEST_PRODUCT,
                     baseline_product=TEST_BASELINE_PRODUCT),
            actual_results_map, baseline_results_map, csv_output)

    def _get_bot_expectations(self, product):
        assert product in (TEST_PRODUCT, TEST_BASELINE_PRODUCT)

        class BotExpectations(object):
            def flakes_by_path(self, *args, **kwargs):

                class AlwaysGet(object):
                    def get(*_):
                        if product == TEST_PRODUCT:
                            return {'FAIL', 'TIMEOUT'}
                        else:
                            return {'CRASH', }

                return AlwaysGet()

        return BotExpectations()


class JsonResultsCompressTest(unittest.TestCase):
    def test_compress_json(self):
        output_mp = {}
        input_mp = {'dir1': {'dir2': {'actual': 'PASS'}}}
        map_tests_to_results(output_mp, input_mp)
        self.assertEquals(output_mp, {'dir1/dir2': {'actual': 'PASS'}})


class CreateCsvTest(unittest.TestCase):
    def test_name_with_comma_escaped_in_csv(self):
        actual_mp = {'test, name.html': {'actual': 'PASS'}}
        with io.BytesIO() as csv_out:
            MockWPTResultsDiffer(actual_mp, actual_mp, csv_out).create_csv()
            csv_out.seek(0)
            content = csv_out.read()
            self.assertEquals(content, CSV_HEADING +
                              '"test, name.html",PASS,PASS,SAME RESULTS,{},{},No\n')

    def test_create_csv_with_same_result(self):
        actual_mp = {'test.html': {'actual': 'PASS'}}
        with io.BytesIO() as csv_out:
            MockWPTResultsDiffer(actual_mp, actual_mp, csv_out).create_csv()
            csv_out.seek(0)
            content = csv_out.read()
            self.assertEquals(content, CSV_HEADING +
                              'test.html,PASS,PASS,SAME RESULTS,{},{},No\n')

    def test_create_csv_with_reliable_different_result(self):
        actual_mp = {'test.html': {'actual': 'PASS'}}
        baseline_mp = copy.deepcopy(actual_mp)
        baseline_mp['test.html']['actual'] = 'FAIL'
        with io.BytesIO() as csv_out:
            MockWPTResultsDiffer(actual_mp, baseline_mp, csv_out).create_csv()
            csv_out.seek(0)
            content = csv_out.read()
            self.assertEquals(content, CSV_HEADING +
                              ('test.html,PASS,FAIL,DIFFERENT RESULTS,'
                               '"{FAIL, TIMEOUT, PASS}","{FAIL, CRASH}",No\n'))

    def test_create_csv_with_unreliable_different_result(self):
        actual_mp = {'test.html': {'actual': 'CRASH'}}
        baseline_mp = copy.deepcopy(actual_mp)
        baseline_mp['test.html']['actual'] = 'FAIL'
        with io.BytesIO() as csv_out:
            MockWPTResultsDiffer(actual_mp, baseline_mp, csv_out).create_csv()
            csv_out.seek(0)
            content = csv_out.read()
            self.assertEquals(content, CSV_HEADING +
                              ('test.html,CRASH,FAIL,DIFFERENT RESULTS,'
                               '"{FAIL, CRASH, TIMEOUT}","{FAIL, CRASH}",Yes\n'))

    def test_create_csv_with_missing_result(self):
        actual_mp = {'test.html': {'actual': 'PASS'}}
        with io.BytesIO() as csv_out:
            MockWPTResultsDiffer(actual_mp, {}, csv_out).create_csv()
            csv_out.seek(0)
            content = csv_out.read()
            self.assertEquals(content, CSV_HEADING +
                              'test.html,PASS,MISSING,MISSING RESULTS,{},{},No\n')


if __name__ == '__main__':
    unittest.main()

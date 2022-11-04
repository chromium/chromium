#!/usr/bin/env vpython3

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
import logging
import json
import six
import unittest

if six.PY3:
    from io import StringIO as DataIO
else:
    from io import BytesIO as DataIO

from blinkpy.common.host import Host
from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.executive_mock import MockExecutive

from collections import namedtuple
from diff_wpt_results import (
    map_tests_to_results, WPTResultsDiffer, CSV_HEADING,
    _get_product_test_results)

MockArgs = namedtuple('MockArgs', ['product_to_compare', 'baseline_product'])
TEST_PRODUCT = 'android_webview'
TEST_BASELINE_PRODUCT = 'chrome_android'


class MockWPTResultsDiffer(WPTResultsDiffer):

    def __init__(self, actual_results_map, baseline_results_map, csv_output):
        super(MockWPTResultsDiffer, self).__init__(
            MockArgs(product_to_compare=TEST_PRODUCT,
                     baseline_product=TEST_BASELINE_PRODUCT),
            MockHost(),
            actual_results_map, baseline_results_map, csv_output)

    def _get_bot_expectations(self, product):
        assert product in (TEST_PRODUCT, TEST_BASELINE_PRODUCT)

        class BotExpectations(object):
            def flakes_by_path(self, *args, **kwargs):

                class AlwaysGet(object):
                    def get(self, *_):
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
        with DataIO() as csv_out:
            MockWPTResultsDiffer(actual_mp, actual_mp, csv_out).create_csv()
            csv_out.seek(0)
            content = csv_out.read()
            heading = CSV_HEADING % (TEST_PRODUCT, TEST_BASELINE_PRODUCT)
            self.assertEquals(content, heading +
                              ('"test, name.html",PASS,PASS,'
                               'SAME RESULTS,"{FAIL, PASS, TIMEOUT}",'
                               '"{CRASH, PASS}",Yes\n'))

    def test_create_csv_with_same_result(self):
        actual_mp = {'test.html': {'actual': 'PASS'}}
        with DataIO() as csv_out:
            MockWPTResultsDiffer(actual_mp, actual_mp, csv_out).create_csv()
            csv_out.seek(0)
            content = csv_out.read()
            heading = CSV_HEADING % (TEST_PRODUCT, TEST_BASELINE_PRODUCT)
            self.assertEquals(content, heading +
                              ('test.html,PASS,PASS,SAME RESULTS,'
                               '"{FAIL, PASS, TIMEOUT}","{CRASH, PASS}",Yes\n'))

    def test_create_csv_with_reliable_different_result(self):
        actual_mp = {'test.html': {'actual': 'PASS'}}
        baseline_mp = copy.deepcopy(actual_mp)
        baseline_mp['test.html']['actual'] = 'FAIL'
        with DataIO() as csv_out:
            MockWPTResultsDiffer(actual_mp, baseline_mp, csv_out).create_csv()
            csv_out.seek(0)
            content = csv_out.read()
            heading = CSV_HEADING % (TEST_PRODUCT, TEST_BASELINE_PRODUCT)
            self.assertEquals(content, heading +
                              ('test.html,PASS,FAIL,DIFFERENT RESULTS,'
                               '"{FAIL, PASS, TIMEOUT}","{CRASH, FAIL}",No\n'))

    def test_create_csv_with_unreliable_different_result(self):
        actual_mp = {'test.html': {'actual': 'CRASH'}}
        baseline_mp = copy.deepcopy(actual_mp)
        baseline_mp['test.html']['actual'] = 'FAIL'
        with DataIO() as csv_out:
            MockWPTResultsDiffer(actual_mp, baseline_mp, csv_out).create_csv()
            csv_out.seek(0)
            content = csv_out.read()
            heading = CSV_HEADING % (TEST_PRODUCT, TEST_BASELINE_PRODUCT)
            self.assertEquals(content, heading +
                              ('test.html,CRASH,FAIL,DIFFERENT RESULTS,'
                               '"{CRASH, FAIL, TIMEOUT}","{CRASH, FAIL}",Yes\n'))

    def test_create_csv_with_missing_result(self):
        actual_mp = {'test.html': {'actual': 'PASS'}}
        with DataIO() as csv_out:
            MockWPTResultsDiffer(actual_mp, {}, csv_out).create_csv()
            csv_out.seek(0)
            content = csv_out.read()
            heading = CSV_HEADING % (TEST_PRODUCT, TEST_BASELINE_PRODUCT)
            self.assertEquals(content, heading +
                              'test.html,PASS,MISSING,MISSING RESULTS,{},{},No\n')

    def test_use_bb_to_get_results(self):
        actual_mp = {'tests': {'test.html': {'actual': 'PASS'}}}
        baseline_mp = copy.deepcopy(actual_mp)
        baseline_mp['tests']['test.html']['actual'] = 'FAIL'
        host = Host()

        def process_cmds(cmd_args):
            if 'token' in cmd_args:
                return '00000'
            elif (('system_webview_wpt on '
                   'Ubuntu-16.04 or Ubuntu-18.04') in cmd_args):
                return json.dumps(actual_mp)
            elif (('chrome_public_wpt on '
                   'Ubuntu-16.04 or Ubuntu-18.04') in cmd_args):
                raise ScriptError('Test Error')
            elif 'chrome_public_wpt' in cmd_args:
                return json.dumps(baseline_mp)
            else:
                return '{"number": 400, "id":"abcd"}'

        host.executive = MockExecutive(run_command_fn=process_cmds)

        with DataIO() as csv_out,                                                 \
                _get_product_test_results(host, 'android_webview', 0) as test_results,   \
                _get_product_test_results(host, 'chrome_android', 0) as baseline_results:

            actual_results_json = json.loads(test_results.read())
            baseline_results_json = json.loads(baseline_results.read())

            tests_to_actual_results = {}
            tests_to_baseline_results = {}
            map_tests_to_results(tests_to_actual_results,
                                 actual_results_json['tests'])
            map_tests_to_results(tests_to_baseline_results,
                                 baseline_results_json['tests'])

            MockWPTResultsDiffer(tests_to_actual_results,
                                 tests_to_baseline_results,
                                 csv_out).create_csv()
            csv_out.seek(0)
            content = csv_out.read()
            heading = CSV_HEADING % (TEST_PRODUCT, TEST_BASELINE_PRODUCT)
            self.assertEquals(content, heading +
                              ('test.html,PASS,FAIL,DIFFERENT RESULTS,'
                               '"{FAIL, PASS, TIMEOUT}","{CRASH, FAIL}",No\n'))

if __name__ == '__main__':
    logging.basicConfig()
    unittest.main()

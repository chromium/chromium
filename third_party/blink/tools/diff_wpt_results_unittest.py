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

from diff_wpt_results import map_tests_to_results, create_csv

CSV_HEADING = 'Test name, Test Result, Baseline Result, Result Comparison'


class JsonResultsCompressTest(unittest.TestCase):
    def test_compress_json(self):
        output_mp = {}
        input_mp = {'dir1': {'dir2': {'actual': 'PASS'}}}
        map_tests_to_results(output_mp, input_mp)
        self.assertEquals(output_mp, {'dir1/dir2': {'actual': 'PASS'}})


class CreateCsvTest(unittest.TestCase):
    def test_create_csv_with_same_result(self):
        actual_mp = {'test.html': {'actual': 'PASS'}}
        with io.BytesIO() as csv_out:
            create_csv(actual_mp, actual_mp, csv_out)
            csv_out.seek(0)
            content = csv_out.read()
            self.assertEquals(content, CSV_HEADING + '\n' +
                              'test.html,PASS,PASS,SAME RESULTS\n')

    def test_create_csv_with_different_result(self):
        actual_mp = {'test.html': {'actual': 'PASS'}}
        baseline_mp = copy.deepcopy(actual_mp)
        baseline_mp['test.html']['actual'] = 'FAIL'
        with io.BytesIO() as csv_out:
            create_csv(actual_mp, baseline_mp, csv_out)
            csv_out.seek(0)
            content = csv_out.read()
            self.assertEquals(content, CSV_HEADING + '\n' +
                              'test.html,PASS,FAIL,DIFFERENT RESULTS\n')

    def test_create_csv_with_missing_result(self):
        actual_mp = {'test.html': {'actual': 'PASS'}}
        with io.BytesIO() as csv_out:
            create_csv(actual_mp, {}, csv_out)
            csv_out.seek(0)
            content = csv_out.read()
            self.assertEquals(content, CSV_HEADING + '\n' +
                              'test.html,PASS,MISSING,MISSING RESULTS\n')


if __name__ == '__main__':
    unittest.main()

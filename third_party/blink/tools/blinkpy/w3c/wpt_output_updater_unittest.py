# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import OrderedDict
import json
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.port.factory_mock import MockPortFactory
from blinkpy.w3c.wpt_output_updater import WPTOutputUpdater

_EXPECTATIONS_TEST_LIST = [
    "some/test.html",
    "external/wpt/expected_crash.html",
    "external/wpt/flake.html",
    "external/wpt/subdir/unexpected_failure.html",
    "passed/test.html"
]

_EXPECTATIONS_FILE_STRING = """
Bug(test) some/test.html [ Failure ]
Bug(test) external/wpt/expected_crash.html [ Crash ]
Bug(test) external/wpt/flake.html [ Timeout Failure ]
Bug(test) external/wpt/subdir/unexpected_failure.html [ Timeout ]
"""


class WPTOutputUpdaterTest(unittest.TestCase):

    def setUp(self):
        self.host = MockHost()
        self.host.port_factory = MockPortFactory(self.host)
        self.port = self.host.port_factory.get()
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = _EXPECTATIONS_FILE_STRING
        self.exp = TestExpectations(self.port, tests=_EXPECTATIONS_TEST_LIST, expectations_dict=expectations_dict)

    def test_update_output_json(self):
        """Tests that output JSON is properly updated with expectations."""
        # Create a WPTOutputUpdater and reset it to use our test expectations.
        output_updater = WPTOutputUpdater(self.exp)

        # Note: this is the WPT output which omits the "external/wpt" root that
        # is present in the expectations file. Also, the expected status is
        # always PASS by default.
        output_string = """
        {
            "path_delimiter": "/",
            "tests": {
                "some": {
                    "test.html": {
                        "expected": "PASS",
                        "actual": "PASS"
                    }
                },
                "expected_crash.html": {
                    "expected": "PASS",
                    "actual": "CRASH"
                },
                "flake.html": {
                    "expected": "PASS",
                    "actual": "TIMEOUT"
                },
                "subdir": {
                    "unexpected_failure.html": {
                        "expected": "PASS",
                        "actual": "FAIL"
                    }
                }
            }
        }
        """
        output_json = json.loads(output_string)

        # A few simple assertions that the original JSON is formatter properly.
        self.assertEqual("PASS", output_json["tests"]["expected_crash.html"]["expected"])
        self.assertEqual("FAIL", output_json["tests"]["subdir"]["unexpected_failure.html"]["actual"])

        # Run the output updater, and confirm the expected statuses are updated.
        new_output_json = output_updater.update_output_json(output_json)

        # some/test.html should not be updated since the expectation is not for
        # external/wpt
        cur_test = new_output_json["tests"]["some"]["test.html"]
        self.assertEqual("PASS", cur_test["expected"])

        # The expected_crash.html test crashed as expected. It's expected status
        # should be updated but is_regression and is_unexpected are both False
        # since this test ran as we expected.
        cur_test = new_output_json["tests"]["expected_crash.html"]
        self.assertEqual("CRASH", cur_test["expected"])
        self.assertFalse(cur_test["is_regression"])
        self.assertFalse(cur_test["is_unexpected"])

        # The flake.html test ran as expected because its status was one of the
        # ones from the expectation file.
        cur_test = new_output_json["tests"]["flake.html"]
        self.assertEqual("TIMEOUT FAIL", cur_test["expected"])
        self.assertFalse(cur_test["is_regression"])
        self.assertFalse(cur_test["is_unexpected"])

        # The unexpected_failure.html test had a different status than expected,
        # so is_unexpected is true. Since the actual status wasn't a Pass, it's
        # also a regression.
        cur_test = new_output_json["tests"]["subdir"]["unexpected_failure.html"]
        self.assertEqual("TIMEOUT", cur_test["expected"])
        self.assertTrue(cur_test["is_regression"])
        self.assertTrue(cur_test["is_unexpected"])

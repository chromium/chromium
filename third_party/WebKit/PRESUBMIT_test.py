#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Note: running this test requires installing the package python-mock.
# pylint: disable=C0103
# pylint: disable=F0401
import PRESUBMIT

import os.path
import subprocess
import sys
import unittest

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'pymock'))
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))

import mock
from PRESUBMIT_test_mocks import MockInputApi
from PRESUBMIT_test_mocks import MockOutputApi
from PRESUBMIT_test_mocks import MockAffectedFile


class Capture(object):
    """Class to capture a call argument that can be tested later on."""

    def __init__(self):
        self.value = None

    def __eq__(self, other):
        self.value = other
        return True


class PresubmitTest(unittest.TestCase):

    @mock.patch('subprocess.Popen')
    def testCheckChangeOnUploadWithWebKitAndChromiumFiles(self, _):
        """This verifies that CheckChangeOnUpload will only call check_blink_style.py
        on WebKit files.
        """
        diff_file_webkit_h = ['some diff']
        diff_file_chromium_h = ['another diff']
        diff_file_test_expectations = ['more diff']
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('FileWebkit.h', diff_file_webkit_h),
            MockAffectedFile('file_chromium.h', diff_file_chromium_h),
            MockAffectedFile('LayoutTests/TestExpectations', diff_file_test_expectations)
        ]
        # Access to a protected member _CheckStyle
        # pylint: disable=W0212
        PRESUBMIT._CheckStyle(mock_input_api, MockOutputApi())
        capture = Capture()
        # pylint: disable=E1101
        subprocess.Popen.assert_called_with(capture, stderr=-1)
        self.assertEqual(6, len(capture.value))
        self.assertEqual('../../FileWebkit.h', capture.value[3])
        self.assertEqual('../../LayoutTests/TestExpectations', capture.value[5])

    @mock.patch('subprocess.Popen')
    def testCheckChangeOnUploadWithEmptyAffectedFileList(self, _):
        """This verifies that CheckChangeOnUpload will skip calling
        check_blink_style.py if the affected file list is empty.
        """
        diff_file_layout_test_html = ['more diff']
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('LayoutTests/some_tests.html', diff_file_layout_test_html)
        ]
        # Access to a protected member _CheckStyle
        # pylint: disable=W0212
        PRESUBMIT._CheckStyle(mock_input_api, MockOutputApi())
        # pylint: disable=E1101
        subprocess.Popen.assert_not_called()

if __name__ == '__main__':
    unittest.main()

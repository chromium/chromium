#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
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
    @mock.patch('PRESUBMIT_test_mocks.MockInputApi.RunTests', create=True)
    @mock.patch('PRESUBMIT_test_mocks.MockCannedChecks.GetPylint', create=True)
    def testCheckChangeOnUploadWithBlinkAndChromiumFiles(
            self, _, _run_tests, _get_pylint):
        """This verifies that CheckChangeOnUpload will only call
        check_blink_style.py on non-test files.
        """
        diff_file_blink_h = ['some diff']
        diff_file_chromium_h = ['another diff']
        diff_file_test_expectations = ['more diff']
        mock_input_api = MockInputApi()
        mock_python_file = MockAffectedFile('file_blink.py', ['lint me'])
        mock_input_api.files = [
            MockAffectedFile('file_blink.h', diff_file_blink_h),
            MockAffectedFile('file_chromium.h', diff_file_chromium_h),
            MockAffectedFile(
                mock_input_api.os_path.join('web_tests', 'TestExpectations'),
                diff_file_test_expectations),
            mock_python_file,
        ]
        mock_input_api.presubmit_local_path = mock_input_api.os_path.join(
            'path', 'to', 'third_party', 'blink')
        with mock.patch.object(mock_python_file,
                               'AbsoluteLocalPath',
                               return_value=mock_input_api.os_path.join(
                                   'path', 'to', 'third_party', 'blink',
                                   'file_blink.py')):
            # Access to a protected member _CheckStyle
            # pylint: disable=W0212
            PRESUBMIT._CheckStyle(mock_input_api, MockOutputApi())
            mock_input_api.canned_checks.GetPylint.assert_called_once_with(
                mock.ANY,
                mock.ANY,
                files_to_check=[r'file_blink\.py'],
                pylintrc=mock_input_api.os_path.join('tools', 'blinkpy',
                                                     'pylintrc'))

        capture = Capture()
        # pylint: disable=E1101
        subprocess.Popen.assert_called_with(capture, stderr=-1)
        self.assertEqual(6, len(capture.value))
        self.assertEqual('file_blink.h', capture.value[3])

    @mock.patch('subprocess.Popen')
    def testCheckChangeOnUploadWithEmptyAffectedFileList(self, _):
        """This verifies that CheckChangeOnUpload will skip calling
        check_blink_style.py if the affected file list is empty.
        """
        diff_file_chromium1_h = ['some diff']
        diff_file_chromium2_h = ['another diff']
        diff_file_layout_test_html = ['more diff']
        mock_input_api = MockInputApi()
        mock_input_api.files = []
        # Access to a protected member _CheckStyle
        # pylint: disable=W0212
        PRESUBMIT._CheckStyle(mock_input_api, MockOutputApi())
        self.assertEqual(0, subprocess.Popen.call_count)

    def test_FilterPaths(self):
        """This verifies that _FilterPaths removes expected paths."""
        diff_file_chromium1_h = ['some diff']
        diff_web_tests_html = ['more diff']
        diff_presubmit = ['morer diff']
        diff_test_expectations = ['morest diff']
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('file_chromium1.h', diff_file_chromium1_h),
            MockAffectedFile(
                mock_input_api.os_path.join('web_tests', 'some_tests.html'),
                diff_web_tests_html),
            MockAffectedFile(
                mock_input_api.os_path.join('web_tests', 'TestExpectations'),
                diff_test_expectations),
            # Note that this path must have a slash, whereas most other paths
            # must have os-standard path separators.
            MockAffectedFile('blink/PRESUBMIT', diff_presubmit),
        ]
        # Access to a protected member _FilterPaths
        # pylint: disable=W0212
        filtered = PRESUBMIT._FilterPaths(mock_input_api)
        self.assertEqual(['file_chromium1.h'], filtered)

    def testCheckPublicHeaderWithBlinkMojo(self):
        """This verifies that _CheckForWrongMojomIncludes detects -blink mojo
        headers in public files.
        """

        mock_input_api = MockInputApi()
        potentially_bad_content = \
            '#include "public/platform/modules/cache_storage.mojom-blink.h"'
        mock_input_api.files = [
            MockAffectedFile(
                mock_input_api.os_path.join('third_party', 'blink', 'public',
                                            'a_header.h'),
                [potentially_bad_content], None)
        ]
        # Access to a protected member _CheckForWrongMojomIncludes
        # pylint: disable=W0212
        errors = PRESUBMIT._CheckForWrongMojomIncludes(mock_input_api,
                                                       MockOutputApi())
        self.assertEqual(
            'Public blink headers using Blink variant mojoms found. ' +
            'You must include .mojom-forward.h or .mojom-shared.h instead:',
            errors[0].message)

    def testCheckInternalHeaderWithBlinkMojo(self):
        """This verifies that _CheckForWrongMojomIncludes accepts -blink mojo
        headers in blink internal files.
        """

        mock_input_api = MockInputApi()
        potentially_bad_content = """
        #include "public/platform/modules/cache_storage.mojom-blink.h"
        #include "public/platform/modules/cache_storage.mojom-blink-forward.h"
        #include "public/platform/modules/cache_storage.mojom-blink-test-utils.h"
        """
        mock_input_api.files = [
            MockAffectedFile(
                mock_input_api.os_path.join('third_party', 'blink', 'renderer',
                                            'core', 'a_header.h'),
                [potentially_bad_content], None)
        ]
        # Access to a protected member _CheckForWrongMojomIncludes
        # pylint: disable=W0212
        errors = PRESUBMIT._CheckForWrongMojomIncludes(mock_input_api,
                                                       MockOutputApi())
        self.assertEqual([], errors)


class CxxDependencyTest(unittest.TestCase):
    allow_list = [
        'base::OnceCallback<void()>',
        'base::RepeatingCallback<void()>',
        'gfx::ColorSpace',
        'gfx::CubicBezier',
        'gfx::ICCProfile',
        'gfx::Point',
        'gfx::Rect',
        'scoped_refptr<base::SingleThreadTaskRunner>',
    ]
    disallow_list = [
        'content::RenderFrame',
        'gfx::Canvas',
        'net::IPEndPoint',
        'ui::Clipboard',
    ]
    disallow_message = []

    def runCheck(self, filename, file_contents):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(filename, file_contents),
        ]
        # Access to a protected member
        # pylint: disable=W0212
        return PRESUBMIT._CheckForForbiddenChromiumCode(
            mock_input_api, MockOutputApi())

    # References in comments should never be checked.
    def testCheckCommentsIgnored(self):
        filename = 'third_party/blink/renderer/core/frame/frame.cc'
        for item in self.allow_list:
            errors = self.runCheck(filename, ['// %s' % item])
            self.assertEqual([], errors)

        for item in self.disallow_list:
            errors = self.runCheck(filename, ['// %s' % item])
            self.assertEqual([], errors)

    # References in Test files should never be checked.
    def testCheckTestsIgnored(self):
        filename = 'third_party/blink/rendere/core/frame/frame_test.cc'
        for item in self.allow_list:
            errors = self.runCheck(filename, ['// %s' % item])
            self.assertEqual([], errors)

        for item in self.disallow_list:
            errors = self.runCheck(filename, ['// %s' % item])
            self.assertEqual([], errors)

    # core, modules, public, et cetera should all have dependency enforcement.
    def testCheckCoreEnforcement(self):
        filename = 'third_party/blink/renderer/core/frame/frame.cc'
        for item in self.allow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEqual([], errors)

        for item in self.disallow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEqual(1, len(errors))
            self.assertRegex(errors[0].message,
                             r'^[^:]+:\d+ uses disallowed identifier .+$')

    def testCheckModulesEnforcement(self):
        filename = 'third_party/blink/renderer/modules/modules_initializer.cc'
        for item in self.allow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEqual([], errors)

        for item in self.disallow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEqual(1, len(errors))
            self.assertRegex(errors[0].message,
                             r'^[^:]+:\d+ uses disallowed identifier .+$')

    def testCheckPublicEnforcement(self):
        filename = 'third_party/blink/renderer/public/platform/web_thread.h'
        for item in self.allow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEqual([], errors)

        for item in self.disallow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEqual(1, len(errors))
            self.assertRegex(errors[0].message,
                             r'^[^:]+:\d+ uses disallowed identifier .+$')

    # platform and controller should be opted out of enforcement, but aren't
    # currently checked because the PRESUBMIT test mocks are missing too
    # much functionality...

    # External module checks should not affect CSS files.
    def testCheckCSSIgnored(self):
        filename = 'third_party/blink/renderer/someFile.css'
        errors = self.runCheck(filename,
                               ['.toolbar::after { color: pink; }\n'])
        self.assertEqual([], errors)


if __name__ == '__main__':
    unittest.main()

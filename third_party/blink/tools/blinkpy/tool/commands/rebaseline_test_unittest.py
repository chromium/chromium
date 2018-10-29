# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse

from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.output_capture import OutputCapture
from blinkpy.tool.commands.rebaseline_test import RebaselineTest
from blinkpy.tool.commands.rebaseline_unittest import BaseTestCase


class TestRebaselineTest(BaseTestCase):
    command_constructor = RebaselineTest

    @staticmethod
    def options(**kwargs):
        return optparse.Values(dict({
            'builder': 'MOCK Mac10.11',
            'port_name': None,
            'test': 'userscripts/another-test.html',
            'suffixes': 'txt',
            'results_directory': None,
            'build_number': None,
            'step_name': None,
        }, **kwargs))

    def test_rebaseline_test_internal_with_port_that_lacks_buildbot(self):
        self.tool.executive = MockExecutive()

        port = self.tool.port_factory.get('test-win-win7')
        self._write(
            port.host.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-win-win10/failures/expected/image-expected.txt'),
            'original win10 result')

        oc = OutputCapture()
        try:
            options = optparse.Values({
                'optimize': True,
                'builder': 'MOCK Win10',
                'port_name': None,
                'suffixes': 'txt',
                'verbose': True,
                'test': 'failures/expected/image.html',
                'results_directory': None,
                'build_number': None,
                'step_name': None,
            })
            oc.capture_output()
            self.command.execute(options, [], self.tool)
        finally:
            out, _, _ = oc.restore_output()

        self.assertMultiLineEqual(
            self._read(self.tool.filesystem.join(
                port.layout_tests_dir(),
                'platform/test-win-win10/failures/expected/image-expected.txt')),
            'MOCK Web result, convert 404 to None=True')
        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            port.layout_tests_dir(), 'platform/test-win-win7/failures/expected/image-expected.txt')))
        self.assertMultiLineEqual(
            out, '{"remove-lines": [{"test": "failures/expected/image.html", "port_name": "test-win-win10"}]}\n')

    def test_baseline_directory(self):
        self.assertMultiLineEqual(
            self.command.baseline_directory('MOCK Mac10.11'),
            '/test.checkout/wtests/platform/test-mac-mac10.11')
        self.assertMultiLineEqual(
            self.command.baseline_directory('MOCK Mac10.10'),
            '/test.checkout/wtests/platform/test-mac-mac10.10')
        self.assertMultiLineEqual(
            self.command.baseline_directory('MOCK Trusty'),
            '/test.checkout/wtests/platform/test-linux-trusty')
        self.assertMultiLineEqual(
            self.command.baseline_directory('MOCK Precise'),
            '/test.checkout/wtests/platform/test-linux-precise')

    def test_rebaseline_updates_expectations_file_noop(self):
        # pylint: disable=protected-access
        self._zero_out_test_expectations()
        self._write(
            self.test_expectations_path,
            ('Bug(B) [ Mac Linux Win7 Debug ] fast/dom/Window/window-postmessage-clone-really-deep-array.html [ Pass ]\n'
             'Bug(A) [ Debug ] : fast/css/large-list-of-rules-crash.html [ Failure ]\n'))
        self._write('fast/dom/Window/window-postmessage-clone-really-deep-array.html', 'Dummy test contents')
        self._write('fast/css/large-list-of-rules-crash.html', 'Dummy test contents')
        self._write('userscripts/another-test.html', 'Dummy test contents')

        self.command._rebaseline_test_and_update_expectations(self.options(suffixes='png,wav,txt'))

        self.assertItemsEqual(self.tool.web.urls_fetched,
                              [self.WEB_PREFIX + '/userscripts/another-test-actual.png',
                               self.WEB_PREFIX + '/userscripts/another-test-actual.wav',
                               self.WEB_PREFIX + '/userscripts/another-test-actual.txt'])
        new_expectations = self._read(self.test_expectations_path)
        self.assertMultiLineEqual(
            new_expectations,
            ('Bug(B) [ Mac Linux Win7 Debug ] fast/dom/Window/window-postmessage-clone-really-deep-array.html [ Pass ]\n'
             'Bug(A) [ Debug ] : fast/css/large-list-of-rules-crash.html [ Failure ]\n'))

    def test_rebaseline_test(self):
        # pylint: disable=protected-access
        self.command._rebaseline_test('test-linux-trusty', 'userscripts/another-test.html', 'txt', self.WEB_PREFIX)
        self.assertItemsEqual(
            self.tool.web.urls_fetched, [self.WEB_PREFIX + '/userscripts/another-test-actual.txt'])

    def test_rebaseline_test_with_results_directory(self):
        # pylint: disable=protected-access
        self._write('userscripts/another-test.html', 'test data')
        self._write(
            self.test_expectations_path,
            ('Bug(x) [ Mac ] userscripts/another-test.html [ Failure ]\n'
             'bug(z) [ Linux ] userscripts/another-test.html [ Failure ]\n'))
        self.command._rebaseline_test_and_update_expectations(self.options(results_directory='/tmp'))
        self.assertItemsEqual(self.tool.web.urls_fetched, ['file:///tmp/userscripts/another-test-actual.txt'])

    def test_rebaseline_reftest(self):
        # pylint: disable=protected-access
        self._write('userscripts/another-test.html', 'test data')
        self._write('userscripts/another-test-expected.html', 'generic result')
        OutputCapture().assert_outputs(
            self, self.command._rebaseline_test_and_update_expectations, args=[self.options(suffixes='png')],
            expected_logs='Cannot rebaseline image result for reftest: userscripts/another-test.html\n')
        self.assertDictEqual(self.command.expectation_line_changes.to_dict(), {'remove-lines': []})

# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2012 Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import optparse
import unittest

from blinkpy.common.path_finder import WEB_TESTS_LAST_COMPONENT
from blinkpy.common.system.output_capture import OutputCapture
from blinkpy.tool.commands.queries import PrintBaselines, PrintExpectations
from blinkpy.tool.mock_tool import MockBlinkTool


class PrintExpectationsTest(unittest.TestCase):

    def run_test(self, tests, expected_stdout, platform='test-win-win7', **kwargs):
        options_defaults = {
            'all': False, 'csv': False, 'full': False, 'platform': platform,
            'include_keyword': [], 'exclude_keyword': [], 'paths': False,
        }
        options_defaults.update(kwargs)
        options = optparse.Values(dict(**options_defaults))
        tool = MockBlinkTool()
        tool.port_factory.all_port_names = lambda: [
            'test-linux-trusty', 'test-linux-precise',
            'test-mac-mac10.11', 'test-mac-mac10.10',
            'test-win-win10', 'test-win-win7'
        ]
        command = PrintExpectations()

        oc = OutputCapture()
        try:
            oc.capture_output()
            command.execute(options, tests, tool)
        finally:
            stdout, _, _ = oc.restore_output()
        self.assertMultiLineEqual(stdout, expected_stdout)

    def test_basic(self):
        self.run_test(['failures/expected/text.html', 'failures/expected/timeout.html'],
                      ('// For test-win-win7\n'
                       'failures/expected/text.html [ Failure ]\n'
                       'failures/expected/timeout.html [ Timeout ]\n'))

    def test_multiple(self):
        self.run_test(['failures/expected/text.html', 'failures/expected/timeout.html'],
                      ('// For test-win-win10\n'
                       'failures/expected/text.html [ Failure ]\n'
                       'failures/expected/timeout.html [ Timeout ]\n'
                       '\n'
                       '// For test-win-win7\n'
                       'failures/expected/text.html [ Failure ]\n'
                       'failures/expected/timeout.html [ Timeout ]\n'),
                      platform='test-win-*')

    def test_full(self):
        self.run_test(['failures/expected/text.html', 'failures/expected/timeout.html'],
                      ('// For test-win-win7\n'
                       'Bug(test) failures/expected/text.html [ Failure ]\n'
                       'Bug(test) failures/expected/timeout.html [ Timeout ]\n'),
                      full=True)

    def test_exclude(self):
        self.run_test(['failures/expected/text.html', 'failures/expected/crash.html'],
                      ('// For test-win-win7\n'
                       'failures/expected/text.html [ Failure ]\n'),
                      exclude_keyword=['crash'])

    def test_include(self):
        self.run_test(['failures/expected/text.html', 'failures/expected/crash.html'],
                      ('// For test-win-win7\n'
                       'failures/expected/crash.html\n'),
                      include_keyword=['crash'])

    def test_csv(self):
        self.run_test(['failures/expected/text.html', 'failures/expected/image.html'],
                      ('test-win-win7,failures/expected/image.html,Bug(test),,FAIL\n'
                       'test-win-win7,failures/expected/text.html,Bug(test),,FAIL\n'),
                      csv=True)

    def test_paths(self):
        self.run_test([],
                      (WEB_TESTS_LAST_COMPONENT + '/TestExpectations\n' +
                       WEB_TESTS_LAST_COMPONENT + '/WebDriverExpectations\n' +
                       WEB_TESTS_LAST_COMPONENT + '/NeverFixTests\n' +
                       WEB_TESTS_LAST_COMPONENT + '/StaleTestExpectations\n' +
                       WEB_TESTS_LAST_COMPONENT + '/SlowTests\n'),
                      paths=True)


class PrintBaselinesTest(unittest.TestCase):

    def setUp(self):
        self.oc = None
        self.tool = MockBlinkTool()
        self.test_port = self.tool.port_factory.get('test-win-win7')
        self.tool.port_factory.get = lambda port_name=None: self.test_port
        self.tool.port_factory.all_port_names = lambda: [
            'test-linux-trusty', 'test-linux-precise',
            'test-mac-mac10.11', 'test-mac-mac10.10',
            'test-win-win10', 'test-win-win7'
        ]

    def tearDown(self):
        if self.oc:
            self.restore_output()

    def capture_output(self):
        self.oc = OutputCapture()
        self.oc.capture_output()

    def restore_output(self):
        stdout, stderr, logs = self.oc.restore_output()
        self.oc = None
        return (stdout, stderr, logs)

    def test_basic(self):
        command = PrintBaselines()
        self.capture_output()
        options = optparse.Values({'all': False, 'include_virtual_tests': False, 'csv': False, 'platform': None})
        command.execute(options, ['passes/text.html'], self.tool)
        stdout, _, _ = self.restore_output()
        self.assertMultiLineEqual(stdout,
                                  ('// For test-win-win7\n'
                                   'passes/text-expected.png\n'
                                   'passes/text-expected.txt\n'))

    def test_multiple(self):
        command = PrintBaselines()
        self.capture_output()
        options = optparse.Values({'all': False, 'include_virtual_tests': False, 'csv': False, 'platform': 'test-win-*'})
        command.execute(options, ['passes/text.html'], self.tool)
        stdout, _, _ = self.restore_output()
        self.assertMultiLineEqual(stdout,
                                  ('// For test-win-win10\n'
                                   'passes/text-expected.png\n'
                                   'passes/text-expected.txt\n'
                                   '\n'
                                   '// For test-win-win7\n'
                                   'passes/text-expected.png\n'
                                   'passes/text-expected.txt\n'))

    def test_csv(self):
        command = PrintBaselines()
        self.capture_output()
        options = optparse.Values({'all': False, 'platform': '*win7', 'csv': True, 'include_virtual_tests': False})
        command.execute(options, ['passes/text.html'], self.tool)
        stdout, _, _ = self.restore_output()
        self.assertMultiLineEqual(stdout,
                                  ('test-win-win7,passes/text.html,None,png,passes/text-expected.png,None\n'
                                   'test-win-win7,passes/text.html,None,txt,passes/text-expected.txt,None\n'))

# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

import StringIO
import optparse
import unittest

from blinkpy.common import exit_codes
from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.web_tests import lint_test_expectations
from blinkpy.web_tests.port.base import VirtualTestSuite
from blinkpy.web_tests.port.test import WEB_TEST_DIR


class FakePort(object):

    def __init__(self, host, name, path):
        self.host = host
        self.name = name
        self.path = path

    def test_configuration(self):
        return None

    def expectations_dict(self):
        self.host.ports_parsed.append(self.name)
        return {self.path: ''}

    def all_expectations_dict(self):
        return self.expectations_dict()

    def bot_expectations(self):
        return {}

    def all_test_configurations(self):
        return []

    def configuration_specifier_macros(self):
        return []

    def get_option(self, _, val):
        return val

    def path_to_generic_test_expectations_file(self):
        return ''

    def extra_expectations_files(self):
        return ['/fake-port-base-directory/web_tests/ExtraExpectations']

    def web_tests_dir(self):
        return '/fake-port-base-directory/web_tests'


class FakeFactory(object):

    def __init__(self, host, ports):
        self.host = host
        self.ports = {}
        for port in ports:
            self.ports[port.name] = port

    def get(self, port_name='a', *args, **kwargs):  # pylint: disable=unused-argument,method-hidden
        return self.ports[port_name]

    def all_port_names(self, platform=None):  # pylint: disable=unused-argument,method-hidden
        return sorted(self.ports.keys())


class LintTest(LoggingTestCase):

    def test_all_configurations(self):
        host = MockHost()
        host.ports_parsed = []
        host.port_factory = FakeFactory(host, (FakePort(host, 'a', 'path-to-a'),
                                               FakePort(host, 'b', 'path-to-b'),
                                               FakePort(host, 'b-win', 'path-to-b')))

        options = optparse.Values({'platform': None})
        res = lint_test_expectations.lint(host, options)
        self.assertEqual(res, [])
        self.assertEqual(host.ports_parsed, ['a', 'b', 'b-win'])

    def test_lint_test_files(self):
        options = optparse.Values({'platform': 'test-mac-mac10.10'})
        host = MockHost()

        host.port_factory.all_port_names = lambda platform=None: [platform]

        res = lint_test_expectations.lint(host, options)
        self.assertEqual(res, [])

    def test_lint_test_files_errors(self):
        options = optparse.Values({'platform': 'test', 'debug_rwt_logging': False})
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        port.expectations_dict = lambda: {'foo': '-- syntax error1', 'bar': '-- syntax error2'}

        host.port_factory.get = lambda platform, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]

        res = lint_test_expectations.lint(host, options)

        self.assertTrue(res)
        all_logs = ''.join(self.logMessages())
        self.assertIn('foo:1', all_logs)
        self.assertIn('bar:1', all_logs)

    def test_extra_files_errors(self):
        options = optparse.Values({'platform': 'test', 'debug_rwt_logging': False})
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        port.expectations_dict = lambda: {}

        host.port_factory.get = lambda platform, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]
        host.filesystem.write_text_file(WEB_TEST_DIR + '/LeakExpectations', '-- syntax error')

        res = lint_test_expectations.lint(host, options)

        self.assertTrue(res)
        all_logs = ''.join(self.logMessages())
        self.assertIn('LeakExpectations:1', all_logs)

    def test_lint_flag_specific_expectation_errors(self):
        options = optparse.Values({'platform': 'test', 'debug_rwt_logging': False})
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        port.expectations_dict = lambda: {'flag-specific': 'does/not/exist', 'noproblem': ''}

        host.port_factory.get = lambda platform, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]

        res = lint_test_expectations.lint(host, options)

        self.assertTrue(res)
        all_logs = ''.join(self.logMessages())
        self.assertIn('flag-specific:1 Path does not exist. does/not/exist', all_logs)
        self.assertNotIn('noproblem', all_logs)


class CheckVirtualSuiteTest(unittest.TestCase):

    def setUp(self):
        self.host = MockHost()
        self.options = optparse.Values({'platform': 'test', 'debug_rwt_logging': False})
        self.port = self.host.port_factory.get('test', options=self.options)
        self.host.port_factory.get = lambda options=None: self.port

    def test_check_virtual_test_suites_readme(self):
        self.port.virtual_test_suites = lambda: [
            VirtualTestSuite(prefix='foo', bases=['test'], args=['--foo']),
            VirtualTestSuite(prefix='bar', bases=['test'], args=['--bar']),
        ]
        self.host.filesystem.maybe_make_directory(WEB_TEST_DIR + '/test')

        res = lint_test_expectations.check_virtual_test_suites(self.host, self.options)
        self.assertEqual(len(res), 2)

        self.host.filesystem.files[WEB_TEST_DIR + '/virtual/foo/README.md'] = ''
        self.host.filesystem.files[WEB_TEST_DIR + '/virtual/bar/test/README.txt'] = ''
        res = lint_test_expectations.check_virtual_test_suites(self.host, self.options)
        self.assertFalse(res)

    def test_check_virtual_test_suites_redundant(self):
        self.port.virtual_test_suites = lambda: [
            VirtualTestSuite(prefix='foo', bases=['test/sub', 'test'], args=['--foo']),
        ]

        self.host.filesystem.exists = lambda _: True
        self.host.filesystem.isdir = lambda _: True
        res = lint_test_expectations.check_virtual_test_suites(self.host, self.options)
        self.assertEqual(len(res), 1)

    def test_check_virtual_test_suites_non_redundant(self):
        self.port.virtual_test_suites = lambda: [
            VirtualTestSuite(prefix='foo', bases=['test_a', 'test'], args=['--foo']),
        ]

        self.host.filesystem.exists = lambda _: True
        self.host.filesystem.isdir = lambda _: True
        res = lint_test_expectations.check_virtual_test_suites(self.host, self.options)
        self.assertEqual(len(res), 0)

    def test_check_virtual_test_suites_non_existent_base(self):
        self.port.virtual_test_suites = lambda: [
            VirtualTestSuite(prefix='foo', bases=['base1', 'base2', 'base3.html'], args=['-foo']),
        ]

        self.host.filesystem.maybe_make_directory(WEB_TEST_DIR + '/base1')
        self.host.filesystem.files[WEB_TEST_DIR + '/base3.html'] = ''
        self.host.filesystem.files[WEB_TEST_DIR + '/virtual/foo/README.md'] = ''
        res = lint_test_expectations.check_virtual_test_suites(self.host, self.options)
        self.assertEqual(len(res), 1)


class MainTest(unittest.TestCase):

    def setUp(self):
        self.orig_lint_fn = lint_test_expectations.lint
        self.orig_check_fn = lint_test_expectations.check_virtual_test_suites
        lint_test_expectations.check_virtual_test_suites = lambda host, options: []
        self.stderr = StringIO.StringIO()

    def tearDown(self):
        lint_test_expectations.lint = self.orig_lint_fn
        lint_test_expectations.check_virtual_test_suites = self.orig_check_fn

    def test_success(self):
        lint_test_expectations.lint = lambda host, options: []
        res = lint_test_expectations.main(['--platform', 'test'], self.stderr)
        self.assertEqual('Lint succeeded.', self.stderr.getvalue().strip())
        self.assertEqual(res, 0)

    def test_failure(self):
        lint_test_expectations.lint = lambda host, options: ['test failure']
        res = lint_test_expectations.main(['--platform', 'test'], self.stderr)
        self.assertEqual('Lint failed.', self.stderr.getvalue().strip())
        self.assertEqual(res, 1)

    def test_interrupt(self):
        def interrupting_lint(host, options):  # pylint: disable=unused-argument
            raise KeyboardInterrupt

        lint_test_expectations.lint = interrupting_lint
        res = lint_test_expectations.main([], self.stderr, host=MockHost())
        self.assertEqual(res, exit_codes.INTERRUPTED_EXIT_STATUS)

    def test_exception(self):
        def exception_raising_lint(host, options):  # pylint: disable=unused-argument
            assert False

        lint_test_expectations.lint = exception_raising_lint
        res = lint_test_expectations.main([], self.stderr, host=MockHost())
        self.assertEqual(res, exit_codes.EXCEPTIONAL_EXIT_STATUS)

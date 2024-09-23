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

import json
import optparse
import textwrap
import unittest
from unittest.mock import patch

from blinkpy.common import exit_codes
from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.web_tests import lint_test_expectations
from blinkpy.web_tests.port.base import VirtualTestSuite
from blinkpy.web_tests.port.test import MOCK_WEB_TESTS

from six import StringIO


class FakePort(object):
    def __init__(self, host, name, path):
        self.host = host
        self.name = name
        self.path = path

    ALL_BUILD_TYPES = ('debug', 'release')
    FLAG_EXPECTATIONS_PREFIX = 'FlagExpectations'

    def test_configuration(self):
        return None

    def get_platform_tags(self):
        return frozenset(['linux'])

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
        return {}

    def get_option(self, _, val):
        return val

    def path_to_generic_test_expectations_file(self):
        return ''

    def extra_expectations_files(self):
        return ['/fake-port-base-directory/web_tests/ExtraExpectations']

    def web_tests_dir(self):
        return '/fake-port-base-directory/web_tests'

    def tests(self,_):
        return set()


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
    def test_lint_test_files(self):
        options = optparse.Values({
            'additional_expectations': [],
            'platform': 'test-mac-mac10.10'
        })
        host = MockHost()
        path_finder = PathFinder(host.filesystem)
        host.filesystem.write_text_file(
            path_finder.path_from_web_tests('TestExpectations'),
            textwrap.dedent("""\
                            # results: [ Pass Failure ]
                            passes/text.html [ Pass ]
                            failures/flaky/text.html [ Failure Pass ]
                            """))
        host.filesystem.write_text_file(
            path_finder.path_from_web_tests('NeverFixTests'), '')
        host.filesystem.write_text_file(
            path_finder.path_from_web_tests('VirtualTestSuites'), '[]')

        host.port_factory.all_port_names = lambda platform=None: [platform]

        failures, warnings = lint_test_expectations.lint(host, options)
        self.assertEqual(failures, [])
        self.assertEqual(warnings, [])

    def test_lint_test_files_errors(self):
        options = optparse.Values({
            'additional_expectations': [],
            'platform': 'test',
            'debug_rwt_logging': False
        })
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        port.expectations_dict = lambda: {'foo': '-- syntax error1', 'bar': '-- syntax error2'}

        host.port_factory.get = lambda platform=None, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]

        failures, warnings = lint_test_expectations.lint(host, options)
        self.assertTrue(failures)
        self.assertEqual(warnings, [])

        all_logs = ''.join(failures)
        self.assertIn('foo', all_logs)
        self.assertIn('bar', all_logs)

    def test_extra_files_errors(self):
        options = optparse.Values({
            'additional_expectations': [],
            'platform': 'test',
            'debug_rwt_logging': False
        })
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        port.expectations_dict = lambda: {}

        host.port_factory.get = lambda platform=None, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]
        host.filesystem.write_text_file(
            host.filesystem.join(MOCK_WEB_TESTS, 'LeakExpectations'),
            '-- syntax error')

        failures, warnings = lint_test_expectations.lint(host, options)
        self.assertTrue(failures)
        self.assertEqual(warnings, [])

        all_logs = ''.join(failures)
        self.assertIn('LeakExpectations', all_logs)

    def test_lint_flag_specific_expectation_errors(self):
        options = optparse.Values({
            'platform': 'test',
            'debug_rwt_logging': False,
            'additional_expectations': []
        })
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        port.expectations_dict = lambda: {'flag-specific': 'does/not/exist', 'noproblem': ''}

        host.port_factory.get = lambda platform=None, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]

        failures, warnings = lint_test_expectations.lint(host, options)
        self.assertTrue(failures)
        self.assertEqual(warnings, [])

        all_logs = ''.join(failures)
        self.assertIn('flag-specific', all_logs)
        self.assertIn('does/not/exist', all_logs)
        self.assertNotIn('noproblem', all_logs)

    def test_lint_conflicts_in_test_expectations_between_os_and_os_version(
            self):
        options = optparse.Values({
            'additional_expectations': [],
            'platform': 'test',
            'debug_rwt_logging': False
        })
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        test_expectations = ('# tags: [ mac mac10.10 ]\n'
                             '# results: [ Failure Pass ]\n'
                             '[ mac ] test1 [ Failure ]\n'
                             '[ mac10.10 ] test1 [ Pass ]\n')
        port.expectations_dict = lambda: {
            'testexpectations': test_expectations}

        host.port_factory.get = lambda platform=None, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]

        failures, warnings = lint_test_expectations.lint(host, options)
        self.assertTrue(failures)
        self.assertEqual(warnings, [])

        all_logs = ''.join(failures)
        self.assertIn('conflict', all_logs)

    def test_lint_existence(self):
        options = optparse.Values({
            'additional_expectations': [],
            'platform': 'test',
            'debug_rwt_logging': False
        })
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        test_expectations = ('# results: [ Pass Failure ]\n'
                             'test1/* [ Failure ]\n'
                             'test2/* [ Failure ]\n'
                             'test2/foo.html [ Failure ]\n'
                             'test2/bar.html [ Failure ]\n'
                             'test3/foo.html [ Failure ]\n'
                             'virtual/foo/* [ Failure ]\n'
                             'virtual/foo/test2/* [ Pass ]\n'
                             'virtual/foo/test2/foo.html [ Pass ]\n'
                             'virtual/foo/test2/bar.html [ Pass ]\n'
                             'virtual/foo/test3/foo.html [ Pass ]\n'
                             'virtual/bar/* [ Pass ]\n'
                             'virtual/bar/test2/foo.html [ Pass ]\n'
                             'external/wpt/abc/def [ Failure ]\n')
        port.expectations_dict = lambda: {
            'testexpectations': test_expectations
        }
        port.virtual_test_suites = lambda: [
            VirtualTestSuite(prefix='foo', platforms=['Linux', 'Mac', 'Win'], bases=['test2'], args=['--foo'])
        ]
        host.filesystem.write_text_file(
            host.filesystem.join(port.web_tests_dir(), 'test2', 'foo.html'),
            'foo')
        host.filesystem.write_text_file(
            host.filesystem.join(port.web_tests_dir(), 'test3', 'foo.html'),
            'foo')
        host.filesystem.write_text_file(
            host.filesystem.join(port.web_tests_dir(), 'virtual', 'foo',
                                 'README.md'), 'foo')

        host.port_factory.get = lambda platform=None, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]

        failures, warnings = lint_test_expectations.lint(host, options)
        self.assertTrue(failures)
        self.assertEqual(warnings, [])

        self.assertEquals(len(failures), 6)
        expected_non_existence = [
            'test1/*',
            'test2/bar.html',
            'virtual/foo/test2/bar.html',
            'virtual/foo/test3/foo.html',
            'virtual/bar/*',
            'virtual/bar/test2/foo.html',
        ]
        for pattern, failure in zip(expected_non_existence, failures):
            self.assertIn('Test does not exist: %s' % pattern, failure)

    def test_lint_globs(self):
        options = optparse.Values({
            'additional_expectations': [],
            'platform': 'test',
            'debug_rwt_logging': False
        })
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        test_expectations = ('# tags: [ mac mac10.10 ]\n'
                             '# results: [ Failure Pass ]\n'
                             '[ mac ] test1 [ Failure ]\n'
                             '[ mac10.10 ] test2 [ Pass ]\n')
        port.expectations_dict = lambda: {
            'testexpectations': test_expectations}
        host.filesystem.maybe_make_directory(
            host.filesystem.join(port.web_tests_dir(), 'test2'))

        host.port_factory.get = lambda platform=None, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]

        failures, warnings = lint_test_expectations.lint(host, options)
        self.assertTrue(failures)
        self.assertEqual(warnings, [])

        all_logs = ''.join(failures)
        self.assertIn('directory', all_logs)

    def test_virtual_test_redundant_expectation(self):
        options = optparse.Values({
            'additional_expectations': [],
            'platform': 'test',
            'debug_rwt_logging': False
        })
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        port.virtual_test_suites = lambda: [
            VirtualTestSuite(
                prefix='foo',
                platforms=['Linux', 'Mac', 'Win'],
                bases=['test', 'external/wpt'],
                args=['--foo'])
        ]
        test_expectations = (
            '# tags: [ mac win ]\n'
            '# tags: [ debug release ]\n'
            '# results: [ Failure Pass ]\n'
            '[ mac ] test/test1.html [ Failure ]\n'
            '[ mac debug ] virtual/foo/test/test1.html [ Failure ]\n'
            '[ win ] virtual/foo/test/test1.html [ Failure ]\n'
            '[ mac release ] virtual/foo/test/test1.html [ Pass ]\n'
            'test/test2.html [ Failure ]\n'
            'crbug.com/1234 virtual/foo/test/test2.html [ Failure ]\n'
            'test/subtest/test2.html [ Failure ]\n'
            'virtual/foo/test/subtest/* [ Pass ]\n'
            'virtual/foo/test/subtest/test2.html [ Failure ]\n'
            'external/wpt/wpt.html [ Failure ]\n'
            # TODO(crbug.com/1080691): This is redundant with the above one, but
            # for now we intentially ignore it.
            'virtual/foo/external/wpt/wpt.html [ Failure ]\n')
        port.expectations_dict = lambda: {
            'testexpectations': test_expectations
        }
        port.test_exists = lambda test: True
        host.port_factory.get = lambda platform=None, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]

        failures, warnings = lint_test_expectations.lint(host, options)
        self.assertEqual(failures, [])

        self.assertEquals(len(warnings), 1)
        self.assertRegexpMatches(warnings[0], ':5 .*redundant with.* line 4$')

    def test_never_fix_tests(self):
        options = optparse.Values({
            'additional_expectations': [],
            'platform': 'test',
            'debug_rwt_logging': False
        })
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        port.virtual_test_suites = lambda: [
            VirtualTestSuite(
                prefix='foo',
                platforms=['Linux', 'Mac', 'Win'],
                bases=['test', 'test1'],
                args=['--foo'])
        ]
        test_expectations = ('# tags: [ mac win ]\n'
                             '# results: [ Skip Pass ]\n'
                             'test/* [ Skip ]\n'
                             '[ mac ] test1/* [ Skip ]\n'
                             'test/sub/* [ Pass ]\n'
                             'test/test1.html [ Pass ]\n'
                             'test1/foo/* [ Pass ]\n'
                             'test2/* [ Pass ]\n'
                             'test2.html [ Skip Pass ]\n'
                             'virtual/foo/test/* [ Pass ]\n'
                             'virtual/foo/test1/* [ Pass ]\n')
        port.expectations_dict = lambda: {'NeverFixTests': test_expectations}
        port.test_exists = lambda test: True
        host.port_factory.get = lambda platform=None, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]

        failures, warnings = lint_test_expectations.lint(host, options)
        self.assertEqual(warnings, [])

        self.assertEquals(len(failures), 5)
        self.assertRegexpMatches(failures[0], ':7 .*must override')
        self.assertRegexpMatches(failures[1], ':8 .*must override')
        self.assertRegexpMatches(failures[2], ':9 Only one of')
        self.assertRegexpMatches(failures[3], ':10 .*exclusive_test')
        self.assertRegexpMatches(failures[4], ':11 .*exclusive_test')

    def test_lint_stable_webexposed_disabled(self):
        options = optparse.Values({
            'additional_expectations': [],
            'platform': 'test',
            'debug_rwt_logging': False
        })
        host = MockHost()

        port = host.port_factory.get(options.platform, options=options)
        port.virtual_test_suites = lambda: [
            VirtualTestSuite(prefix='stable',
                             platforms=['Linux', 'Mac', 'Win'],
                             bases=['test', 'webexposed'],
                             args=['--foo'])
        ]
        test_expectations = (
            '# tags: [ mac win ]\n'
            '# results: [ Skip Pass Failure ]\n'
            'test/* [ Skip Failure ]\n'
            '[ mac ] webexposed/* [ Skip Failure ]\n'
            'test/sub/* [ Pass ]\n'
            'test/test1.html [ Pass ]\n'
            'webexposed/foo/* [ Pass ]\n'
            'webexposed/test2.html [ Failure ]\n'
            'virtual/test/test/* [ Failure ]\n'
            'virtual/test/foo.html [ Pass ]\n'
            'virtual/stable/webexposed/test1/* [ Pass ]\n'
            'virtual/stable/webexposed/test2/* [ Skip Failure ]\n'
            'virtual/stable/webexposed/api.html [ Pass Failure ]\n')
        port.expectations_dict = lambda: {
            'TestExpectations': test_expectations
        }
        port.test_exists = lambda test: True
        host.port_factory.get = lambda platform=None, options=None: port
        host.port_factory.all_port_names = lambda platform=None: [port.name()]

        (fail1, fail2), warnings = lint_test_expectations.lint(host, options)
        self.assertRegexpMatches(fail1, '.*virtual/stable/webexposed/test2/.*')
        self.assertRegexpMatches(fail2,
                                 r'.*virtual/stable/webexposed/api\.html.*')

    def test_lint_skip_in_test_expectations(self):
        options = optparse.Values({
            'additional_expectations': [],
            'platform': 'test'
        })
        host = MockHost()
        port = host.port_factory.get(options.platform, options=options)
        test_expectations = ('# results: [ Skip Timeout Failure ]\n'
                             'test1.html [ Skip ]\n'
                             'test2.html [ Skip Timeout ]\n'
                             'test3.html [ Skip Failure ]\n')
        port.expectations_dict = lambda: {
            'TestExpectations': test_expectations
        }
        port.test_exists = lambda test: True
        host.port_factory.get = lambda platform=None, options=None: port

        failures, warnings = lint_test_expectations.lint(host, options)
        self.assertEqual(warnings, [])
        self.assertEquals(len(failures), 1)
        self.assertRegexpMatches(failures[0], ':2 .*Skip')


class CheckVirtualSuiteTest(unittest.TestCase):
    def setUp(self):
        self.host = MockHost()
        self.options = optparse.Values({
            'platform': 'test',
            'debug_rwt_logging': False,
            # Assume the manifest is already up-to-date.
            'manifest_update': False,
        })
        self.port = self.host.port_factory.get('test', options=self.options)
        self.host.port_factory.get = lambda options=None: self.port

        fs = self.host.filesystem
        manifest_path = fs.join(self.port.web_tests_dir(), 'external', 'wpt',
                                'MANIFEST.json')
        fs.write_text_file(manifest_path, json.dumps({}))
        manifest_path = fs.join(self.port.web_tests_dir(), 'wpt_internal',
                                'MANIFEST.json')
        fs.write_text_file(manifest_path, json.dumps({}))

    def test_check_virtual_test_suites_readme(self):
        self.port.virtual_test_suites = lambda: [
            VirtualTestSuite(prefix='foo',
                             platforms=['Linux', 'Mac', 'Win'],
                             owners=['testowner@chromium.org'],
                             bases=['test'],
                             args=['--foo']),
            VirtualTestSuite(prefix='bar',
                             platforms=['Linux', 'Mac', 'Win'],
                             owners=['testowner@chromium.org'],
                             bases=['test'],
                             args=['--bar']),
        ]
        fs = self.host.filesystem
        fs.maybe_make_directory(fs.join(MOCK_WEB_TESTS, 'test'))

        res = lint_test_expectations.check_virtual_test_suites(
            self.host, self.options)
        self.assertEqual(len(res), 2)

        fs.write_text_file(
            fs.join(MOCK_WEB_TESTS, 'virtual', 'foo', 'README.md'), '')
        fs.write_text_file(
            fs.join(MOCK_WEB_TESTS, 'virtual', 'bar', 'test', 'README.txt'),
            '')
        res = lint_test_expectations.check_virtual_test_suites(
            self.host, self.options)
        self.assertFalse(res)

    def test_check_virtual_test_suites_generated(self):
        fs = self.host.filesystem
        # Satisfy the README check, which is out of scope for this test.
        fs.write_text_file(
            fs.join(self.port.web_tests_dir(), 'virtual', 'wpt-generated',
                    'README.md'), '')
        manifest = {
            'items': {
                'testharness': {
                    'test.any.js': [
                        'df2f8b048c370d3ab009946d73d7de6f8a412471',
                        ['test.any.html?a', {}],
                        ['test.any.worker.html?a', {}],
                        ['test.any.html?b', {}],
                        ['test.any.worker.html?b', {}],
                    ],
                },
            },
        }
        manifest_path = fs.join(self.port.web_tests_dir(), 'external', 'wpt',
                                'MANIFEST.json')
        fs.write_text_file(manifest_path, json.dumps(manifest))

        suites = [
            VirtualTestSuite(prefix='wpt-generated',
                             platforms=['Linux', 'Mac', 'Win'],
                             bases=[
                                 'external/wpt/test.any.html?a',
                                 'external/wpt/test.any.worker.html?b'
                             ],
                             exclusive_tests='ALL',
                             owners=['testowner@chromium.org'],
                             args=['--arg']),
        ]
        with patch.object(self.port,
                          'virtual_test_suites',
                          return_value=suites):
            self.assertEqual(
                lint_test_expectations.check_virtual_test_suites(
                    self.host, self.options), [])

    def test_check_virtual_test_suites_redundant(self):
        self.port.virtual_test_suites = lambda: [
            VirtualTestSuite(prefix='foo',
                             platforms=['Linux', 'Mac', 'Win'],
                             owners=['testowner@chromium.org'],
                             bases=['test/sub', 'test'],
                             args=['--foo']),
        ]

        self.host.filesystem.exists = lambda _: True
        self.host.filesystem.isdir = lambda _: True
        res = lint_test_expectations.check_virtual_test_suites(
            self.host, self.options)
        self.assertEqual(len(res), 1)

    def test_check_virtual_test_suites_non_redundant(self):
        self.port.virtual_test_suites = lambda: [
            VirtualTestSuite(prefix='foo',
                             platforms=['Linux', 'Mac', 'Win'],
                             owners=['testowner@chromium.org'],
                             bases=['test_a', 'test'],
                             args=['--foo']),
        ]

        self.host.filesystem.exists = lambda _: True
        self.host.filesystem.isdir = lambda _: True
        res = lint_test_expectations.check_virtual_test_suites(
            self.host, self.options)
        self.assertEqual(len(res), 0)

    def test_check_virtual_test_suites_bases_and_exclusive_tests(self):
        self.port.virtual_test_suites = lambda: [
            VirtualTestSuite(
                prefix='foo',
                platforms=['Linux', 'Mac', 'Win'],
                bases=['base1', 'base2', 'base3.html'],
                exclusive_tests=
                ['base1/exist.html', 'base1/missing.html', 'base4'],
                owners=['testowner@chromium.org'],
                args=['-foo']),
        ]

        fs = self.host.filesystem
        fs.maybe_make_directory(fs.join(MOCK_WEB_TESTS, 'base1'))
        fs.write_text_file(fs.join(MOCK_WEB_TESTS, 'base1', 'exist.html'), '')
        fs.write_text_file(fs.join(MOCK_WEB_TESTS, 'base3.html'), '')
        fs.write_text_file(
            fs.join(MOCK_WEB_TESTS, 'virtual', 'foo', 'README.md'), '')
        fs.maybe_make_directory(fs.join(MOCK_WEB_TESTS, 'base4'))
        res = lint_test_expectations.check_virtual_test_suites(
            self.host, self.options)
        self.assertEqual(len(res), 3)
        self.assertRegexpMatches(res[0], 'base2.* or directory')
        self.assertRegexpMatches(res[1], 'base1/missing.html.* or directory')
        self.assertRegexpMatches(res[2], 'base4.*subset of bases')

    def test_check_virtual_test_suites_name_length_tests(self):
        self.port.virtual_test_suites = lambda: [
            VirtualTestSuite(
                prefix=
                'testing_prefix_with_larger_character_count_then_the_allowed_amount_of_48',
                platforms=['Linux', 'Mac', 'Win'],
                owners=['testowner@chromium.org'],
                bases=['test'],
                args=['--arg']),
        ]

        self.host.filesystem.exists = lambda _: True
        self.host.filesystem.isdir = lambda _: True
        res = lint_test_expectations.check_virtual_test_suites(
            self.host, self.options)
        self.assertRegexpMatches(
            res,
            'Virtual suite name "testing_prefix_with_larger_character_count_then_the_allowed_amount_of_48" is over the "48" filename length limit'
        )

    def test_check_virtual_test_suites_with_no_owner(self):
        self.port.virtual_test_suites = lambda: [
            VirtualTestSuite(prefix='test',
                             platforms=['Linux', 'Mac', 'Win'],
                             bases=['test'],
                             args=['--arg']),
        ]
        self.host.filesystem.exists = lambda _: True
        self.host.filesystem.isdir = lambda _: True
        res = lint_test_expectations.check_virtual_test_suites(
            self.host, self.options)
        self.assertRegexpMatches(res[0],
                                 'Virtual suite name "test" has no owner.')
        self.assertEqual(len(res), 1)
        self.port.virtual_test_suites = lambda: [
            VirtualTestSuite(prefix='test',
                             platforms=['Linux', 'Mac', 'Win'],
                             owners=[],
                             bases=['test'],
                             args=['--arg']),
        ]
        res2 = lint_test_expectations.check_virtual_test_suites(
            self.host, self.options)
        self.assertRegexpMatches(res2[0],
                                 'Virtual suite name "test" has no owner.')
        self.assertEqual(len(res2), 1)

class MainTest(unittest.TestCase):
    def setUp(self):
        self.orig_lint_fn = lint_test_expectations.lint
        self.orig_check_fn = lint_test_expectations.check_virtual_test_suites
        lint_test_expectations.check_virtual_test_suites = lambda host, options: []
        self.orig_check_test_lists = lint_test_expectations.check_test_lists
        lint_test_expectations.check_test_lists = lambda host, options: []
        self.stderr = StringIO()

    def tearDown(self):
        lint_test_expectations.lint = self.orig_lint_fn
        lint_test_expectations.check_virtual_test_suites = self.orig_check_fn
        lint_test_expectations.check_test_lists = self.orig_check_test_lists

    def test_success(self):
        lint_test_expectations.lint = lambda host, options: ([], [])
        res = lint_test_expectations.main(['--platform', 'test'], self.stderr)
        self.assertEqual('', self.stderr.getvalue().strip())
        self.assertEqual(res, 0)

    def test_success_with_warning(self):
        lint_test_expectations.lint = lambda host, options: ([],
                                                             ['test warning'])
        res = lint_test_expectations.main(['--platform', 'test'], self.stderr)
        self.assertEqual(
            textwrap.dedent("""\
                test warning

                Lint succeeded with warnings.
                """), self.stderr.getvalue())
        self.assertEqual(res, 2)

    def test_failure(self):
        lint_test_expectations.lint = lambda host, options: (['test failure'],
                                                             [])
        res = lint_test_expectations.main(['--platform', 'test'], self.stderr)
        self.assertEqual(
            textwrap.dedent("""\
                test failure

                Lint failed.
                """), self.stderr.getvalue())
        self.assertEqual(res, 1)

    def test_failures_with_warnings(self):
        lint_test_expectations.lint = lambda host, options: (
            ['test failure', 'test failure'], ['test warning', 'test warning'])
        res = lint_test_expectations.main(['--platform', 'test'], self.stderr)
        self.assertEqual(
            textwrap.dedent("""\
                test failure

                test warning

                Lint failed.
                """), self.stderr.getvalue())
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

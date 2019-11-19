# Copyright (C) 2010 Google Inc. All rights reserved.
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

from collections import OrderedDict
import optparse
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.output_capture import OutputCapture
from blinkpy.web_tests.models.test_configuration import TestConfiguration, TestConfigurationConverter
from blinkpy.web_tests.models.test_expectations import (
    TestExpectationLine, TestExpectations, ParseError, TestExpectationParser,
    PASS, FAIL, TIMEOUT, CRASH, SKIP)


class Base(unittest.TestCase):
    # Note that all of these tests are written assuming the configuration
    # being tested is Windows 7, Release build.

    def __init__(self, testFunc):
        host = MockHost()
        self._port = host.port_factory.get('test-win-win7', None)
        self._exp = None
        unittest.TestCase.__init__(self, testFunc)

    def get_basic_tests(self):
        return ['failures/expected/text.html',
                'failures/expected/image_checksum.html',
                'failures/expected/crash.html',
                'failures/expected/image.html',
                'failures/expected/timeout.html',
                'passes/text.html',
                'reftests/failures/expected/has_unused_expectation.html']

    def get_basic_expectations(self):
        return """
Bug(test) failures/expected/text.html [ Failure ]
Bug(test) failures/expected/crash.html [ Crash ]
Bug(test) failures/expected/image_checksum.html [ Crash ]
Bug(test) failures/expected/image.html [ Crash Mac ]
"""

    def parse_exp(self, expectations, overrides=None, is_lint_mode=False):
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = expectations
        if overrides:
            expectations_dict['overrides'] = overrides
        self._port.expectations_dict = lambda: expectations_dict
        expectations_to_lint = expectations_dict if is_lint_mode else None
        self._exp = TestExpectations(self._port, self.get_basic_tests(
        ), expectations_dict=expectations_to_lint, is_lint_mode=is_lint_mode)

    def assert_exp_list(self, test, results):
        self.assertEqual(self._exp.get_expectations(test), set(results))

    def assert_exp(self, test, result):
        self.assert_exp_list(test, [result])

    def assert_bad_expectations(self, expectations, overrides=None):
        with self.assertRaises(ParseError):
            self.parse_exp(expectations, is_lint_mode=True, overrides=overrides)


class BasicTests(Base):

    def test_basic(self):
        self.parse_exp(self.get_basic_expectations())
        self.assert_exp('failures/expected/text.html', FAIL)
        self.assert_exp_list('failures/expected/image_checksum.html', [CRASH])
        self.assert_exp('passes/text.html', PASS)
        self.assert_exp('failures/expected/image.html', PASS)


class MiscTests(Base):

    def test_parse_mac_legacy_names(self):
        host = MockHost()
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = '\nBug(x) [ Mac10.10 ] failures/expected/text.html [ Failure ]\n'

        port = host.port_factory.get('test-mac-mac10.10', None)
        port.expectations_dict = lambda: expectations_dict
        expectations = TestExpectations(port, self.get_basic_tests())
        self.assertEqual(expectations.get_expectations('failures/expected/text.html'), set([FAIL]))

        port = host.port_factory.get('test-win-win7', None)
        port.expectations_dict = lambda: expectations_dict
        expectations = TestExpectations(port, self.get_basic_tests())
        self.assertEqual(expectations.get_expectations('failures/expected/text.html'), set([PASS]))

    def test_multiple_results(self):
        self.parse_exp('Bug(x) failures/expected/text.html [ Crash Failure ]')
        self.assertEqual(self._exp.get_expectations('failures/expected/text.html'), set([FAIL, CRASH]))

    def test_result_was_expected(self):
        # test basics
        self.assertEqual(TestExpectations.result_was_expected(PASS, set([PASS])), True)
        self.assertEqual(TestExpectations.result_was_expected(FAIL, set([PASS])), False)

        # test handling of SKIPped tests and results
        self.assertEqual(TestExpectations.result_was_expected(SKIP, set([CRASH])), False)
        self.assertEqual(TestExpectations.result_was_expected(SKIP, set([FAIL])), False)

        # test handling of MISSING results
        self.assertEqual(TestExpectations.result_was_expected(FAIL, set([PASS])), False)

    def test_category_expectations(self):
        # This test checks unknown tests are not present in the
        # expectations and that known test part of a test category is
        # present in the expectations.
        exp_str = 'Bug(x) failures/expected [ CRASH ]'
        self.parse_exp(exp_str)
        test_name = 'failures/expected/unknown-test.html'
        unknown_test = test_name
        with self.assertRaises(KeyError):
            self._exp.get_expectations(unknown_test)
        self.assert_exp_list('failures/expected/crash.html', [PASS])

    def test_get_expectations_string(self):
        self.parse_exp(self.get_basic_expectations())
        self.assertEqual(self._exp.get_expectations_string('failures/expected/text.html'), 'FAIL')

    def test_expectation_to_string(self):
        # Normal cases are handled by other tests.
        self.parse_exp(self.get_basic_expectations())
        with self.assertRaises(ValueError):
            self._exp.expectation_to_string(-1)

    def test_get_test_set(self):
        # Handle some corner cases for this routine not covered by other tests.
        self.parse_exp(self.get_basic_expectations())
        test_set = self._exp.get_test_set(CRASH)
        self.assertEqual(test_set, set(['failures/expected/crash.html', 'failures/expected/image_checksum.html']))

    def test_parse_warning(self):
        try:
            filesystem = self._port.host.filesystem
            filesystem.write_text_file(filesystem.join(self._port.web_tests_dir(), 'disabled-test.html-disabled'), 'content')
            filesystem.write_text_file(filesystem.join(self._port.web_tests_dir(), 'test-to-rebaseline.html'), 'content')
            self.parse_exp('Bug(user) [ FOO ] failures/expected/text.html [ Failure ]\n'
                           'Bug(user) non-existent-test.html [ Failure ]\n'
                           'Bug(user) disabled-test.html-disabled [ Failure ]\n',
                           is_lint_mode=True)
            self.assertFalse(True, "ParseError wasn't raised")
        except ParseError as error:
            warnings = ('expectations:1 Unrecognized specifier "FOO" failures/expected/text.html\n'
                        'expectations:2 Path does not exist. non-existent-test.html')
            self.assertEqual(str(error), warnings)

    def test_parse_warnings_are_logged_if_not_in_lint_mode(self):
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.parse_exp('-- this should be a syntax error', is_lint_mode=False)
        finally:
            _, _, logs = oc.restore_output()
            self.assertNotEquals(logs, '')

    def test_error_on_different_platform(self):
        # parse_exp uses a Windows port. Assert errors on Mac show up in lint mode.
        with self.assertRaises(ParseError):
            self.parse_exp(
                'Bug(test) [ Mac ] failures/expected/text.html [ Failure ]\n'
                'Bug(test) [ Mac ] failures/expected/text.html [ Failure ]',
                is_lint_mode=True)

    def test_error_on_different_build_type(self):
        # parse_exp uses a Release port. Assert errors on DEBUG show up in lint mode.
        with self.assertRaises(ParseError):
            self.parse_exp(
                'Bug(test) [ Debug ] failures/expected/text.html [ Failure ]\n'
                'Bug(test) [ Debug ] failures/expected/text.html [ Failure ]',
                is_lint_mode=True)

    def test_overrides(self):
        self.parse_exp('Bug(exp) failures/expected/text.html [ Failure ]',
                       'Bug(override) failures/expected/text.html [ Timeout ]')
        self.assert_exp_list('failures/expected/text.html', [FAIL, TIMEOUT])

    def test_overrides__directory(self):
        self.parse_exp('Bug(exp) failures/expected/text.html [ Failure ]',
                       'Bug(override) failures/expected [ Crash ]')
        self.assert_exp_list('failures/expected/text.html', [FAIL, CRASH])
        self.assert_exp_list('failures/expected/image.html', [CRASH])

    def test_overrides__duplicate(self):
        self.assert_bad_expectations('Bug(exp) failures/expected/text.html [ Failure ]',
                                     'Bug(override) failures/expected/text.html [ Timeout ]\n'
                                     'Bug(override) failures/expected/text.html [ Crash ]\n')

    def test_more_specific_override_resets_skip(self):
        self.parse_exp('Bug(x) failures/expected [ Skip ]\n'
                       'Bug(x) failures/expected/text.html [ Failure ]\n')
        self.assert_exp('failures/expected/text.html', FAIL)
        self.assertNotIn(
            self._port.host.filesystem.join(
                self._port.web_tests_dir(),
                'failures/expected/text.html'),
            self._exp.get_tests_with_result_type(SKIP))

    def test_bot_test_expectations(self):
        """Test that expectations are merged rather than overridden when using flaky option 'unexpected'."""
        test_name1 = 'failures/expected/text.html'
        test_name2 = 'passes/text.html'

        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = 'Bug(x) %s [ Failure ]\nBug(x) %s [ Crash ]\n' % (test_name1, test_name2)
        self._port.expectations_dict = lambda: expectations_dict

        expectations = TestExpectations(self._port, self.get_basic_tests())
        self.assertEqual(expectations.get_expectations(test_name1), set([FAIL]))
        self.assertEqual(expectations.get_expectations(test_name2), set([CRASH]))

        def bot_expectations():
            return {test_name1: ['PASS', 'TIMEOUT'], test_name2: ['CRASH']}
        self._port.bot_expectations = bot_expectations
        self._port._options.ignore_flaky_tests = 'unexpected'

        expectations = TestExpectations(self._port, self.get_basic_tests())
        self.assertEqual(expectations.get_expectations(test_name1), set([PASS, FAIL, TIMEOUT]))
        self.assertEqual(expectations.get_expectations(test_name2), set([CRASH]))

    def test_shorten_filename(self):
        expectations = TestExpectations(self._port, self.get_basic_tests())
        self.assertEquals(expectations._shorten_filename('/out-of-checkout/TestExpectations'),
                          '/out-of-checkout/TestExpectations')
        self.assertEquals(expectations._shorten_filename('/mock-checkout/third_party/blink/web_tests/TestExpectations'),
                          'third_party/blink/web_tests/TestExpectations')

class SkippedTests(Base):

    def check(self, expectations, overrides, ignore_tests, lint=False, expected_results=None):
        expected_results = expected_results or [SKIP, FAIL]
        port = MockHost().port_factory.get(
            'test-win-win7',
            options=optparse.Values({'ignore_tests': ignore_tests}))
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(
                port.web_tests_dir(), 'failures/expected/text.html'),
            'foo')
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = expectations
        if overrides:
            expectations_dict['overrides'] = overrides
        port.expectations_dict = lambda: expectations_dict
        expectations_to_lint = expectations_dict if lint else None
        exp = TestExpectations(port, ['failures/expected/text.html'], expectations_dict=expectations_to_lint, is_lint_mode=lint)
        self.assertEqual(exp.get_expectations('failures/expected/text.html'), set(expected_results))

    def test_duplicate_skipped_test_fails_lint(self):
        with self.assertRaises(ParseError):
            self.check(
                expectations='Bug(x) failures/expected/text.html [ Failure ]\n',
                overrides=None, ignore_tests=['failures/expected/text.html'], lint=True)

    def test_skipped_file_overrides_expectations(self):
        self.check(expectations='Bug(x) failures/expected/text.html [ Failure ]\n', overrides=None,
                   ignore_tests=['failures/expected/text.html'])

    def test_skipped_dir_overrides_expectations(self):
        self.check(expectations='Bug(x) failures/expected/text.html [ Failure ]\n', overrides=None,
                   ignore_tests=['failures/expected'])

    def test_skipped_file_overrides_overrides(self):
        self.check(expectations='', overrides='Bug(x) failures/expected/text.html [ Failure ]\n',
                   ignore_tests=['failures/expected/text.html'])

    def test_skipped_dir_overrides_overrides(self):
        self.check(expectations='', overrides='Bug(x) failures/expected/text.html [ Failure ]\n',
                   ignore_tests=['failures/expected'])

    def test_skipped_entry_dont_exist(self):
        port = MockHost().port_factory.get(
            'test-win-win7',
            options=optparse.Values({'ignore_tests': ['foo/bar/baz.html']}))
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = ''
        port.expectations_dict = lambda: expectations_dict
        capture = OutputCapture()
        capture.capture_output()
        TestExpectations(port)
        _, _, logs = capture.restore_output()
        self.assertEqual('The following test foo/bar/baz.html from the Skipped list doesn\'t exist\n', logs)

    def test_expectations_string(self):
        self.parse_exp(self.get_basic_expectations())
        notrun = 'failures/expected/text.html'
        self._exp.add_extra_skipped_tests([notrun])
        self.assertEqual('NOTRUN', self._exp.get_expectations_string(notrun))


class ExpectationSyntaxTests(Base):

    def test_unrecognized_expectation(self):
        self.assert_bad_expectations('Bug(test) failures/expected/text.html [ Unknown ]')

    def test_macro(self):
        exp_str = 'Bug(test) [ Win ] failures/expected/text.html [ Failure ]'
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', FAIL)

    def assert_tokenize_exp(self, line, bugs=None, specifiers=None, expectations=None, warnings=None,
                            comment=None, name='foo.html', filename='TestExpectations'):
        bugs = bugs or []
        specifiers = specifiers or []
        expectations = expectations or []
        warnings = warnings or []
        line_number = '1'
        host = MockHost()
        expectation_line = TestExpectationLine.tokenize_line(
            filename, line, line_number, host.port_factory.get('test-win-win7', None))
        self.assertEqual(expectation_line.warnings, warnings)
        self.assertEqual(expectation_line.name, name)
        self.assertEqual(expectation_line.filename, filename)
        self.assertEqual(expectation_line.line_numbers, line_number)
        if not warnings:
            self.assertEqual(expectation_line.specifiers, specifiers)
            self.assertEqual(expectation_line.expectations, expectations)

    def test_comments(self):
        self.assert_tokenize_exp('# comment', name=None, comment='# comment')
        self.assert_tokenize_exp('foo.html [ Pass ] # comment', comment='# comment', expectations=['PASS'], specifiers=[])

    def test_config_specifiers(self):
        self.assert_tokenize_exp('[ Mac ] foo.html [ Failure ] ', specifiers=['MAC'], expectations=['FAIL'])

    def test_unknown_config(self):
        self.assert_tokenize_exp('[ Foo ] foo.html [ Pass ]', specifiers=['Foo'], expectations=['PASS'],
                                 warnings=['Unrecognized specifier "Foo"'])

    def test_unknown_expectation(self):
        self.assert_tokenize_exp('foo.html [ Audio ]', warnings=['Unrecognized expectation "Audio"'])

    def test_skip(self):
        self.assert_tokenize_exp('foo.html [ Skip ]', specifiers=[], expectations=['SKIP'])

    def test_slow(self):
        self.assert_tokenize_exp('foo.html [ Slow ]', specifiers=[], expectations=['SLOW'],
                                 warnings=['SLOW tests should only be added to SlowTests and not to TestExpectations.'])
        self.assert_tokenize_exp('foo.html [ Slow ]', specifiers=[], expectations=['SLOW'], filename='SlowTests')
        self.assert_tokenize_exp('foo.html [ Timeout Slow ]', specifiers=[], expectations=['SKIP', 'TIMEOUT'], warnings=[
                                 'Only SLOW expectations are allowed in SlowTests'], filename='SlowTests')
        self.assert_tokenize_exp('external/wpt/foo.html [ Slow ]', name='external/wpt/foo.html', specifiers=[], expectations=['SLOW'], warnings=[
                                 'WPT should not be added to SlowTests; they should be marked as slow inside the test (see https://web-platform-tests.org/writing-tests/testharness-api.html#harness-timeout)'], filename='SlowTests')

    def test_wontfix(self):
        self.assert_tokenize_exp(
            'foo.html [ Skip Failure ]', specifiers=[], expectations=['SKIP'], warnings=[
                'A test marked SKIP must not have other expectations.'])
        self.assert_tokenize_exp(
            'foo.html [ Skip Failure ]', specifiers=[], expectations=['SKIP'], warnings=[
                'A test marked SKIP must not have other expectations.',
                'Only SKIP expectations are allowed in NeverFixTests.'], filename='NeverFixTests')
        self.assert_tokenize_exp(
            'foo.html [ Skip Timeout ]', specifiers=[], expectations=['Skip', 'TIMEOUT'], warnings=[
                'A test marked SKIP must not have other expectations.',
                'Only SKIP expectations are allowed in NeverFixTests.'], filename='NeverFixTests')

    def test_blank_line(self):
        self.assert_tokenize_exp('', name=None)

    def test_warnings(self):
        self.assert_tokenize_exp('[ Mac ]', warnings=['Did not find a test name.', 'Missing expectations.'], name=None)
        self.assert_tokenize_exp('[ [', warnings=['unexpected "["', 'Missing expectations.'], name=None)
        self.assert_tokenize_exp('crbug.com/12345 ]', warnings=['unexpected "]"', 'Missing expectations.'], name=None)

        self.assert_tokenize_exp('foo.html crbug.com/12345 ]',
                                 warnings=['"crbug.com/12345" is not at the start of the line.', 'Missing expectations.'])
        self.assert_tokenize_exp('foo.html', warnings=['Missing expectations.'])


class SemanticTests(Base):

    def test_bug_format(self):
        with self.assertRaises(ParseError):
            self.parse_exp('BUG1234 failures/expected/text.html [ Failure ]', is_lint_mode=True)

    def test_bad_bugid(self):
        try:
            self.parse_exp('crbug/1234 failures/expected/text.html [ Failure ]', is_lint_mode=True)
            self.fail('should have raised an error about a bad bug identifier')
        except ParseError as exp:
            self.assertEqual(len(exp.warnings), 3)

    def test_exclusive_specifiers_error_in_lint_mode(self):
        with self.assertRaises(ParseError):
            self.parse_exp('BUG1234 [ Mac Win ] failures/expected/text.html [ Failure ]',
                           is_lint_mode=True)

        with self.assertRaises(ParseError):
            self.parse_exp('BUG1234 [ Mac Debug Release ] failures/expected/text.html [ Failure ]',
                           is_lint_mode=True)

    def test_missing_bugid(self):
        self.parse_exp('failures/expected/text.html [ Failure ]', is_lint_mode=False)
        self.assertFalse(self._exp.has_warnings())

        try:
            self.parse_exp('failures/expected/text.html [ Failure ]', is_lint_mode=True)
        except ParseError as exp:
            self.assertEqual(exp.warnings, ['expectations:1 Test lacks BUG specifier. failures/expected/text.html'])

    def test_skip_and_wontfix(self):
        # Skip is not allowed to have other expectations as well, because those
        # expectations won't be exercised and may become stale .
        self.parse_exp('failures/expected/text.html [ Failure Skip ]')
        self.assertTrue(self._exp.has_warnings())

        self.parse_exp('failures/expected/text.html [ Crash WontFix ]')
        self.assertTrue(self._exp.has_warnings())

        self.parse_exp('failures/expected/text.html [ Pass WontFix ]')
        self.assertTrue(self._exp.has_warnings())

    def test_rebaseline(self):
        # Can't lint a file w/ 'REBASELINE' in it.
        with self.assertRaises(ParseError):
            self.parse_exp(
                'Bug(test) failures/expected/text.html [ Failure Rebaseline ]',
                is_lint_mode=True)

    def test_duplicates(self):
        self.assertRaises(ParseError, self.parse_exp, """
Bug(exp) failures/expected/text.html [ Failure ]
Bug(exp) failures/expected/text.html [ Timeout ]""", is_lint_mode=True)
        with self.assertRaises(ParseError):
            self.parse_exp(
                self.get_basic_expectations(), overrides="""
Bug(override) failures/expected/text.html [ Failure ]
Bug(override) failures/expected/text.html [ Timeout ]""", is_lint_mode=True)

    def test_duplicate_with_line_before_preceding_line(self):
        self.assert_bad_expectations("""Bug(exp) [ Debug ] failures/expected/text.html [ Failure ]
Bug(exp) [ Release ] failures/expected/text.html [ Failure ]
Bug(exp) [ Debug ] failures/expected/text.html [ Failure ]
""")

    def test_missing_file(self):
        self.parse_exp('Bug(test) missing_file.html [ Failure ]')
        self.assertTrue(self._exp.has_warnings(), 1)


class PrecedenceTests(Base):

    def test_file_over_directory(self):
        # This tests handling precedence of specific lines over directories
        # and tests expectations covering entire directories.
        exp_str = """
Bug(x) failures/expected/text.html [ Failure ]
Bug(y) failures/expected [ Crash ]
"""
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', FAIL)
        self.assert_exp_list('failures/expected/crash.html', [CRASH])

        exp_str = """
Bug(x) failures/expected [ Crash ]
Bug(y) failures/expected/text.html [ Failure ]
"""
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', FAIL)
        self.assert_exp_list('failures/expected/crash.html', [CRASH])

    def test_ambiguous(self):
        self.assert_bad_expectations('Bug(test) [ Release ] passes/text.html [ Pass ]\n'
                                     'Bug(test) [ Win ] passes/text.html [ Failure ]\n')

    def test_more_specifiers(self):
        self.assert_bad_expectations('Bug(test) [ Release ] passes/text.html [ Pass ]\n'
                                     'Bug(test) [ Win Release ] passes/text.html [ Failure ]\n')

    def test_order_in_file(self):
        self.assert_bad_expectations('Bug(test) [ Win Release ] : passes/text.html [ Failure ]\n'
                                     'Bug(test) [ Release ] : passes/text.html [ Pass ]\n')

    def test_macro_overrides(self):
        self.assert_bad_expectations('Bug(test) [ Win ] passes/text.html [ Pass ]\n'
                                     'Bug(test) [ Win7 ] passes/text.html [ Failure ]\n')


class RemoveConfigurationsTest(Base):

    def test_remove(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {
            'expectations': """Bug(x) [ Linux Win Release ] failures/expected/foo.html [ Failure ]
Bug(y) [ Win Mac Debug ] failures/expected/foo.html [ Crash ]
"""}
        expectations = TestExpectations(test_port, self.get_basic_tests())

        actual_expectations = expectations.remove_configurations([('failures/expected/foo.html', test_config)])

        self.assertEqual("""Bug(x) [ Linux Win10 Release ] failures/expected/foo.html [ Failure ]
Bug(y) [ Win Mac Debug ] failures/expected/foo.html [ Crash ]
""", actual_expectations)

    def test_remove_multiple_configurations(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {'expectations': """Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]
Bug(x) [ Win Release ] failures/expected/foo.html [ Failure ]
"""}
        expectations = TestExpectations(test_port)

        actual_expectations = expectations.remove_configurations([
            ('failures/expected/foo.html', test_config),
            ('failures/expected/foo.html', host.port_factory.get('test-win-win10', None).test_configuration()),
        ])

        self.assertEqual("""Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]
""", actual_expectations)

    def test_remove_line_with_comments(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {'expectations': """Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]

 # This comment line should get stripped. As should the preceding line.
Bug(x) [ Win Release ] failures/expected/foo.html [ Failure ]
"""}
        expectations = TestExpectations(test_port)

        actual_expectations = expectations.remove_configurations([('failures/expected/foo.html', test_config)])
        actual_expectations = expectations.remove_configurations(
            [('failures/expected/foo.html', host.port_factory.get('test-win-win10', None).test_configuration())])

        self.assertEqual("""Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]
""", actual_expectations)

    def test_remove_line_with_comments_at_start(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {'expectations': """
 # This comment line should get stripped. As should the preceding line.
Bug(x) [ Win Release ] failures/expected/foo.html [ Failure ]

Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]
"""}
        expectations = TestExpectations(test_port)

        actual_expectations = expectations.remove_configurations([('failures/expected/foo.html', test_config)])
        actual_expectations = expectations.remove_configurations(
            [('failures/expected/foo.html', host.port_factory.get('test-win-win10', None).test_configuration())])

        self.assertEqual("""
Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]
""", actual_expectations)

    def test_remove_line_with_comments_at_end_with_no_trailing_newline(self):
        # TODO(wkorman): Simplify the redundant initialization in every test case.
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {'expectations': """Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]

 # This comment line should get stripped. As should the preceding line.
Bug(x) [ Win Release ] failures/expected/foo.html [ Failure ]"""}
        expectations = TestExpectations(test_port)

        actual_expectations = expectations.remove_configurations([('failures/expected/foo.html', test_config)])
        actual_expectations = expectations.remove_configurations(
            [('failures/expected/foo.html', host.port_factory.get('test-win-win10', None).test_configuration())])

        self.assertEqual("""Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]""", actual_expectations)

    def test_remove_line_leaves_comments_for_next_line(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {'expectations': """
 # This comment line should not get stripped.
Bug(x) [ Win Release ] failures/expected/foo.html [ Failure ]
Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]
"""}
        expectations = TestExpectations(test_port)

        actual_expectations = expectations.remove_configurations([('failures/expected/foo.html', test_config)])
        actual_expectations = expectations.remove_configurations(
            [('failures/expected/foo.html', host.port_factory.get('test-win-win10', None).test_configuration())])

        self.assertEqual("""
 # This comment line should not get stripped.
Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]
""", actual_expectations)

    def test_remove_line_no_whitespace_lines(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {'expectations': """
 # This comment line should get stripped.
Bug(x) [ Win Release ] failures/expected/foo.html [ Failure ]
 # This comment line should not get stripped.
Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]
"""}
        expectations = TestExpectations(test_port)

        actual_expectations = expectations.remove_configurations([('failures/expected/foo.html', test_config)])
        actual_expectations = expectations.remove_configurations(
            [('failures/expected/foo.html', host.port_factory.get('test-win-win10', None).test_configuration())])

        self.assertEqual(""" # This comment line should not get stripped.
Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]
""", actual_expectations)

    def test_remove_line_keeping_comments_before_whitespace_lines(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {'expectations': """
 # This comment line should not get stripped.

 # This comment line should get stripped.
Bug(x) [ Win Release ] failures/expected/foo.html [ Failure ]
"""}
        expectations = TestExpectations(test_port)

        actual_expectations = expectations.remove_configurations([('failures/expected/foo.html', test_config)])
        actual_expectations = expectations.remove_configurations(
            [('failures/expected/foo.html', host.port_factory.get('test-win-win10', None).test_configuration())])

        self.assertEqual("""
 # This comment line should not get stripped.
""", actual_expectations)

    def test_remove_first_line(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {'expectations': """Bug(x) [ Win Release ] failures/expected/foo.html [ Failure ]
 # This comment line should not get stripped.
Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]
"""}
        expectations = TestExpectations(test_port)

        actual_expectations = expectations.remove_configurations([('failures/expected/foo.html', test_config)])
        actual_expectations = expectations.remove_configurations(
            [('failures/expected/foo.html', host.port_factory.get('test-win-win10', None).test_configuration())])

        self.assertEqual(""" # This comment line should not get stripped.
Bug(y) [ Win Debug ] failures/expected/foo.html [ Crash ]
""", actual_expectations)

    def test_remove_flaky_line(self):
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        test_port.test_exists = lambda test: True
        test_port.test_isfile = lambda test: True

        test_config = test_port.test_configuration()
        test_port.expectations_dict = lambda: {'expectations': """Bug(x) [ Win ] failures/expected/foo.html [ Failure Timeout ]
Bug(y) [ Mac ] failures/expected/foo.html [ Crash ]
"""}
        expectations = TestExpectations(test_port)

        actual_expectations = expectations.remove_configurations([('failures/expected/foo.html', test_config)])
        actual_expectations = expectations.remove_configurations(
            [('failures/expected/foo.html', host.port_factory.get('test-win-win10', None).test_configuration())])

        self.assertEqual("""Bug(x) [ Win Debug ] failures/expected/foo.html [ Failure Timeout ]
Bug(y) [ Mac ] failures/expected/foo.html [ Crash ]
""", actual_expectations)


class TestExpectationsParserTests(unittest.TestCase):

    def __init__(self, testFunc):
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        self._converter = TestConfigurationConverter(test_port.all_test_configurations(),
                                                     test_port.configuration_specifier_macros())
        unittest.TestCase.__init__(self, testFunc)
        self._parser = TestExpectationParser(host.port_factory.get('test-win-win7', None), [], is_lint_mode=False)

    def test_expectation_line_for_test(self):
        # This is kind of a silly test, but it at least ensures that we don't throw an error.
        test_name = 'foo/test.html'
        expectations = set(['PASS', 'FAIL'])

        expectation_line = TestExpectationLine()
        expectation_line.original_string = test_name
        expectation_line.name = test_name
        expectation_line.filename = '<Bot TestExpectations>'
        expectation_line.line_numbers = '0'
        expectation_line.expectations = expectations
        self._parser._parse_line(expectation_line)

        self.assertEqual(self._parser.expectation_line_for_test(test_name, expectations), expectation_line)


class TestExpectationSerializationTests(unittest.TestCase):

    def __init__(self, testFunc):
        host = MockHost()
        test_port = host.port_factory.get('test-win-win7', None)
        self._converter = TestConfigurationConverter(test_port.all_test_configurations(),
                                                     test_port.configuration_specifier_macros())
        unittest.TestCase.__init__(self, testFunc)

    def _tokenize(self, line):
        host = MockHost()
        return TestExpectationLine.tokenize_line('path', line, 0, host.port_factory.get('test-win-win7', None))

    def assert_round_trip(self, in_string, expected_string=None):
        expectation = self._tokenize(in_string)
        if expected_string is None:
            expected_string = in_string
        self.assertEqual(expected_string, expectation.to_string(self._converter))

    def assert_list_round_trip(self, in_string, expected_string=None):
        host = MockHost()
        parser = TestExpectationParser(host.port_factory.get('test-win-win7', None), [], is_lint_mode=False)
        expectations = parser.parse('path', in_string)
        if expected_string is None:
            expected_string = in_string
        self.assertEqual(expected_string, TestExpectations.list_to_string(expectations, self._converter))

    def test_unparsed_to_string(self):
        expectation = TestExpectationLine()

        self.assertEqual(expectation.to_string(self._converter), '')
        expectation.comment = ' Qux.'
        self.assertEqual(expectation.to_string(self._converter), '# Qux.')
        expectation.name = 'bar'
        self.assertEqual(expectation.to_string(self._converter), 'bar # Qux.')
        expectation.specifiers = ['foo']
        # FIXME: case should be preserved here but we can't until we drop the old syntax.
        self.assertEqual(expectation.to_string(self._converter), '[ FOO ] bar # Qux.')
        expectation.expectations = ['bAz']
        self.assertEqual(expectation.to_string(self._converter), '[ FOO ] bar [ BAZ ] # Qux.')
        expectation.expectations = ['bAz1', 'baZ2']
        self.assertEqual(expectation.to_string(self._converter), '[ FOO ] bar [ BAZ1 BAZ2 ] # Qux.')
        expectation.specifiers = ['foo1', 'foO2']
        self.assertEqual(expectation.to_string(self._converter), '[ FOO1 FOO2 ] bar [ BAZ1 BAZ2 ] # Qux.')
        expectation.warnings.append('Oh the horror.')
        self.assertEqual(expectation.to_string(self._converter), '')
        expectation.original_string = 'Yes it is!'
        self.assertEqual(expectation.to_string(self._converter), 'Yes it is!')

    def test_unparsed_list_to_string(self):
        expectation = TestExpectationLine()
        expectation.comment = 'Qux.'
        expectation.name = 'bar'
        expectation.specifiers = ['foo']
        expectation.expectations = ['bAz1', 'baZ2']
        # FIXME: case should be preserved here but we can't until we drop the old syntax.
        self.assertEqual(TestExpectations.list_to_string([expectation]), '[ FOO ] bar [ BAZ1 BAZ2 ] #Qux.')

    def test_parsed_to_string(self):
        expectation_line = TestExpectationLine()
        expectation_line.bugs = ['Bug(x)']
        expectation_line.name = 'test/name/for/realz.html'
        expectation_line.parsed_expectations = set([FAIL])
        self.assertIsNone(expectation_line.to_string(self._converter))
        expectation_line.matching_configurations = set([TestConfiguration('win7', 'x86', 'release')])
        self.assertEqual(expectation_line.to_string(self._converter),
                         'Bug(x) [ Win7 Release ] test/name/for/realz.html [ Failure ]')
        expectation_line.matching_configurations = set(
            [TestConfiguration('win7', 'x86', 'release'), TestConfiguration('win7', 'x86', 'debug')])
        self.assertEqual(expectation_line.to_string(self._converter), 'Bug(x) [ Win7 ] test/name/for/realz.html [ Failure ]')

    def test_parsed_to_string_mac_legacy_names(self):
        expectation_line = TestExpectationLine()
        expectation_line.bugs = ['Bug(x)']
        expectation_line.name = 'test/name/for/realz.html'
        expectation_line.parsed_expectations = set([FAIL])
        self.assertIsNone(expectation_line.to_string(self._converter))
        expectation_line.matching_configurations = set([TestConfiguration('mac10.10', 'x86', 'release')])
        self.assertEqual(expectation_line.to_string(self._converter),
                         'Bug(x) [ Mac10.10 Release ] test/name/for/realz.html [ Failure ]')

    def test_serialize_parsed_expectations(self):
        # Testing protected method - pylint: disable=protected-access
        expectation_line = TestExpectationLine()
        expectation_line.parsed_expectations = set([])
        parsed_expectation_to_string = dict([[parsed_expectation, expectation_string]
                                             for expectation_string, parsed_expectation in TestExpectations.EXPECTATIONS.items()])
        self.assertEqual(expectation_line._serialize_parsed_expectations(parsed_expectation_to_string), '')
        expectation_line.parsed_expectations = set([FAIL])
        self.assertEqual(expectation_line._serialize_parsed_expectations(parsed_expectation_to_string), 'fail')
        expectation_line.parsed_expectations = set([PASS, FAIL])
        self.assertEqual(expectation_line._serialize_parsed_expectations(parsed_expectation_to_string), 'pass fail')
        expectation_line.parsed_expectations = set([FAIL, PASS])
        self.assertEqual(expectation_line._serialize_parsed_expectations(parsed_expectation_to_string), 'pass fail')

    def test_serialize_parsed_specifier_string(self):
        # Testing protected method - pylint: disable=protected-access
        expectation_line = TestExpectationLine()
        expectation_line.bugs = ['garden-o-matic']
        expectation_line.parsed_specifiers = ['the', 'for']
        self.assertEqual(expectation_line._serialize_parsed_specifiers(self._converter, []), 'for the')
        self.assertEqual(expectation_line._serialize_parsed_specifiers(self._converter, ['win']), 'for the win')
        expectation_line.bugs = []
        expectation_line.parsed_specifiers = []
        self.assertEqual(expectation_line._serialize_parsed_specifiers(self._converter, []), '')
        self.assertEqual(expectation_line._serialize_parsed_specifiers(self._converter, ['win']), 'win')

    def test_format_line(self):
        # Testing protected method - pylint: disable=protected-access
        self.assertEqual(
            TestExpectationLine._format_line(
                [], ['MODIFIERS'], 'name', ['EXPECTATIONS'], 'comment'),
            '[ MODIFIERS ] name [ EXPECTATIONS ] #comment')
        self.assertEqual(
            TestExpectationLine._format_line([], ['MODIFIERS'], 'name', ['EXPECTATIONS'], None),
            '[ MODIFIERS ] name [ EXPECTATIONS ]')
        self.assertEqual(
            TestExpectationLine._format_line([], [], 'foo/test.html', ['Skip'], None),
            'foo/test.html [ Skip ]')

    def test_string_roundtrip(self):
        self.assert_round_trip('')
        self.assert_round_trip('[')
        self.assert_round_trip('FOO [')
        self.assert_round_trip('FOO ] bar')
        self.assert_round_trip('  FOO [')
        self.assert_round_trip('    [ FOO ] ')
        self.assert_round_trip('[ FOO ] bar [ BAZ ]')
        self.assert_round_trip('[ FOO ] bar [ BAZ ] # Qux.')
        self.assert_round_trip('[ FOO ] bar [ BAZ ] # Qux.')
        self.assert_round_trip('[ FOO ] bar [ BAZ ] # Qux.     ')
        self.assert_round_trip('[ FOO ] bar [ BAZ ] #        Qux.     ')
        self.assert_round_trip('[ FOO ] ] ] bar BAZ')
        self.assert_round_trip('[ FOO ] ] ] bar [ BAZ ]')
        self.assert_round_trip('FOO ] ] bar ==== BAZ')
        self.assert_round_trip('=')
        self.assert_round_trip('#')
        self.assert_round_trip('# ')
        self.assert_round_trip('# Foo')
        self.assert_round_trip('# Foo')
        self.assert_round_trip('# Foo :')
        self.assert_round_trip('# Foo : =')

    def test_list_roundtrip(self):
        self.assert_list_round_trip('')
        self.assert_list_round_trip('\n')
        self.assert_list_round_trip('\n\n')
        self.assert_list_round_trip('bar')
        self.assert_list_round_trip('bar\n# Qux.')
        self.assert_list_round_trip('bar\n# Qux.\n')

    def test_reconstitute_only_these(self):
        lines = []
        reconstitute_only_these = []

        def add_line(matching_configurations, reconstitute):
            expectation_line = TestExpectationLine()
            expectation_line.original_string = 'Nay'
            expectation_line.bugs = ['Bug(x)']
            expectation_line.name = 'Yay'
            expectation_line.parsed_expectations = set([FAIL])
            expectation_line.matching_configurations = matching_configurations
            lines.append(expectation_line)
            if reconstitute:
                reconstitute_only_these.append(expectation_line)

        add_line(set([TestConfiguration('win7', 'x86', 'release')]), True)
        add_line(set([TestConfiguration('win7', 'x86', 'release'), TestConfiguration('win7', 'x86', 'debug')]), False)
        serialized = TestExpectations.list_to_string(lines, self._converter)
        self.assertEqual(serialized, 'Bug(x) [ Win7 Release ] Yay [ Failure ]\nBug(x) [ Win7 ] Yay [ Failure ]')
        serialized = TestExpectations.list_to_string(lines, self._converter, reconstitute_only_these=reconstitute_only_these)
        self.assertEqual(serialized, 'Bug(x) [ Win7 Release ] Yay [ Failure ]\nNay')

    def disabled_test_string_whitespace_stripping(self):
        # FIXME: Re-enable this test once we rework the code to no longer support the old syntax.
        self.assert_round_trip('\n', '')
        self.assert_round_trip('  [ FOO ] bar [ BAZ ]', '[ FOO ] bar [ BAZ ]')
        self.assert_round_trip('[ FOO ]    bar [ BAZ ]', '[ FOO ] bar [ BAZ ]')
        self.assert_round_trip('[ FOO ] bar [ BAZ ]       # Qux.', '[ FOO ] bar [ BAZ ] # Qux.')
        self.assert_round_trip('[ FOO ] bar [        BAZ ]  # Qux.', '[ FOO ] bar [ BAZ ] # Qux.')
        self.assert_round_trip('[ FOO ]       bar [    BAZ ]  # Qux.', '[ FOO ] bar [ BAZ ] # Qux.')
        self.assert_round_trip('[ FOO ]       bar     [    BAZ ]  # Qux.', '[ FOO ] bar [ BAZ ] # Qux.')

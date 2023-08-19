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
import textwrap
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.web_tests.models.test_expectations import (
    TestExpectations, SystemConfigurationEditor, ParseError)
from blinkpy.web_tests.models.typ_types import ResultType, Expectation
from six.moves import range
from functools import reduce


class Base(unittest.TestCase):
    # Note that all of these tests are written assuming the configuration
    # being tested is Windows 7, Release build.

    def __init__(self, testFunc):
        host = MockHost()
        self._port = host.port_factory.get('test-win-win7', None)
        self._exp = None
        unittest.TestCase.__init__(self, testFunc)

    def get_basic_expectations(self):
        return """
# results: [ Failure Crash ]
failures/expected/text.html [ Failure ]
failures/expected/crash.html [ Crash ] # foo and bar
failures/expected/image_checksum.html [ Crash ]
failures/expected/image.html [ Crash ]
"""

    def parse_exp(self, expectations, overrides=None, is_lint_mode=False):
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = expectations
        if overrides:
            expectations_dict['overrides'] = overrides
        self._port.expectations_dict = lambda: expectations_dict
        expectations_to_lint = expectations_dict if is_lint_mode else None
        self._exp = TestExpectations(
            self._port, expectations_dict=expectations_to_lint)

    def assert_exp_list(self, test, results):
        exp = self._exp.get_expectations(test)
        self.assertEqual(exp.test, test)
        self.assertEqual(exp.results, set(results))

    def assert_exp(self, test, result):
        self.assert_exp_list(test, [result])

    def assert_is_slow(self, test, is_slow):
        self.assertEqual(
            self._exp.get_expectations(test).is_slow_test, is_slow)

    def assert_bad_expectations(self, expectations, overrides=None):
        with self.assertRaises(ParseError):
            self.parse_exp(
                expectations, is_lint_mode=True, overrides=overrides)

    def assert_trailing_comments(self, test, comments):
        self.assertEqual(
            self._exp.get_expectations(test).trailing_comments, comments)

    def assert_basic(self):
        self.assert_exp('failures/expected/text.html', ResultType.Failure)
        self.assert_exp_list('failures/expected/image_checksum.html',
                             [ResultType.Crash])
        self.assert_exp('passes/text.html', ResultType.Pass)
        self.assert_exp('failures/expected/image.html', ResultType.Crash)
        self.assert_trailing_comments('failures/expected/crash.html',
                                      ' # foo and bar\n')


class BasicTests(Base):
    def test_basic(self):
        self.parse_exp(self.get_basic_expectations())
        self.assert_basic()


class VirtualExpectationsTest(Base):
    def test_virtual_expectations(self):
        # See test.TestPort.virtual_test_suite() for the mapping of the virtual
        # test suites to bases.
        self.parse_exp(
            '# results: [ Pass Slow Skip ]\n' + self.get_basic_expectations() +
            'passes/text.html [ Slow ]\n'
            'passes/image.html [ Skip ]\n'
            'virtual/virtual_passes/passes/text.html [ Failure ]\n'
            'virtual/virtual_failures/failure/expected/crash.html [ Pass ]')
        self.assert_basic()
        # Overrides.
        self.assert_exp('virtual/virtual_passes/passes/text.html',
                        ResultType.Failure)
        self.assert_is_slow('virtual/virtual_passes/passes/text.html', True)
        self.assert_exp('virtual/virtual_failures/failure/expected/text.html',
                        ResultType.Pass)
        self.assert_is_slow(
            'virtual/virtual_failures/failure/expected/text.html', False)
        # Fallbacks.
        self.assert_exp(
            'virtual/virtual_failures/failures/expected/crash.html',
            ResultType.Crash)
        self.assert_exp('virtual/virtual_passes/passes/image.html',
                        ResultType.Skip)
        # Non existence virtual suite doesn't fallback.
        self.assert_exp('virtual/xyz/failures/expected/crash.html',
                        ResultType.Pass)


class FlagExpectationsTests(Base):
    def setup_using_raw_expectations(self,
                                     base_exps='',
                                     flag_exps='',
                                     flag_name=''):
        self._general_exp_filename = 'TestExpectations'
        expectations_dict = {self._general_exp_filename: base_exps}

        # set up flag specific expectations
        if flag_name:
            self._flag_exp_filename = self._port.host.filesystem.join(
                'FlagExpectations', flag_name)
            expectations_dict[self._flag_exp_filename] = flag_exps

        self._test_expectations = TestExpectations(self._port,
                                                   expectations_dict)

    def assert_base_and_flag_exp(self, test, base_exp, flag_exp):
        self.assertEqual(
            self._test_expectations.get_base_expectations(test).results,
            set([base_exp]))
        actual_flag_exp = self._test_expectations.get_flag_expectations(test)
        if flag_exp is None:
            self.assertIsNone(actual_flag_exp)
        else:
            self.assertEqual(actual_flag_exp.results, set([flag_exp]))

    def assert_exp(self, test, result):
        self.assert_exp_list(test, [result])

    def test_add_flag_test_expectations(self):
        raw_flag_exps = """
        # tags: [ Win ]
        # results: [ Failure ]
        [ Win ] failures/expected/text.html [ Failure ]
        """
        self.setup_using_raw_expectations(
            flag_exps=raw_flag_exps, flag_name='composite-after-paint')
        flag_exp = self._test_expectations.get_flag_expectations(
            'failures/expected/text.html')
        self.assertEqual(flag_exp.results, set([ResultType.Failure]))
        self.assertEqual(self._test_expectations.flag_name,
                         '/composite-after-paint')

    def test_override_and_fallback(self):
        raw_base_exps = """
        # tags: [ Win ]
        # results: [ Skip Slow Failure Crash Pass ]
        [ Win ] failures/expected/text.html [ Slow ]
        failures/expected/image.html [ Skip ]
        failures/expected/reftest.html [ Slow Failure ]
        failures/expected/crash.html [ Crash ]
        """
        raw_flag_exps = """
        # tags: [ Win ]
        # results: [ Failure Pass Slow ]
        [ Win ] failures/expected/text.html [ Failure ]
        failures/expected/image.html [ Pass ]
        failures/expected/reftest.html [ Pass ]
        """
        self.setup_using_raw_expectations(base_exps=raw_base_exps,
                                          flag_exps=raw_flag_exps,
                                          flag_name='composite-after-paint')
        self.assertEqual(self._test_expectations.flag_name,
                         '/composite-after-paint')

        # Default pass without any explicit expectations.
        exp = self._test_expectations.get_expectations('passes/text.html')
        self.assertEqual(exp.results, set([ResultType.Pass]))
        self.assertTrue(exp.is_default_pass)
        self.assertFalse(exp.is_slow_test)
        self.assert_base_and_flag_exp('passes/text.html', ResultType.Pass,
                                      None)

        # The test has a flag-specific expectation.
        exp = self._test_expectations.get_expectations(
            'failures/expected/text.html')
        self.assertEqual(exp.results, set([ResultType.Failure]))
        self.assertFalse(exp.is_default_pass)
        self.assertTrue(exp.is_slow_test)
        self.assert_base_and_flag_exp('failures/expected/text.html',
                                      ResultType.Pass, ResultType.Failure)

        # The flag-specific expectation overrides the base expectation.
        exp = self._test_expectations.get_expectations(
            'failures/expected/image.html')
        self.assertEqual(exp.results, set([ResultType.Pass]))
        self.assertFalse(exp.is_default_pass)
        self.assertFalse(exp.is_slow_test)
        self.assert_base_and_flag_exp('failures/expected/image.html',
                                      ResultType.Skip, ResultType.Pass)

        # The flag-specific expectation overrides the base expectation, but
        # inherits [ Slow ] of the base expectation.
        exp = self._test_expectations.get_expectations(
            'failures/expected/reftest.html')
        self.assertEqual(exp.results, set([ResultType.Pass]))
        self.assertFalse(exp.is_default_pass)
        self.assertTrue(exp.is_slow_test)
        self.assert_base_and_flag_exp('failures/expected/reftest.html',
                                      ResultType.Failure, ResultType.Pass)

        # No flag-specific expectation. Fallback to the base expectation.
        exp = self._test_expectations.get_expectations(
            'failures/expected/crash.html')
        self.assertEqual(exp.results, set([ResultType.Crash]))
        self.assertFalse(exp.is_default_pass)
        self.assertFalse(exp.is_slow_test)
        self.assert_base_and_flag_exp('failures/expected/crash.html',
                                      ResultType.Crash, None)

    def test_override_and_fallback_virtual_test(self):
        raw_base_exps = """
        # tags: [ Win ]
        # results: [ Skip Slow Failure Crash Pass ]
        [ Win ] failures/expected/text.html [ Slow ]
        failures/expected/image.html [ Skip ]
        failures/expected/reftest.html [ Failure ]
        failures/expected/crash.html [ Crash ]
        virtual/virtual_failures/failures/expected/crash.html [ Pass ]
        """
        raw_flag_exps = """
        # tags: [ Win ]
        # results: [ Failure Pass Timeout Slow ]
        [ Win ] failures/expected/text.html [ Failure ]
        failures/expected/image.html [ Pass ]
        failures/expected/reftest.html [ Slow ]
        failures/expected/crash.html [ Timeout ]
        virtual/virtual_failures/failures/expected/image.html [ Failure ]
        """
        self.setup_using_raw_expectations(base_exps=raw_base_exps,
                                          flag_exps=raw_flag_exps,
                                          flag_name='composite-after-paint')
        self.assertEqual(self._test_expectations.flag_name,
                         '/composite-after-paint')

        # Default pass of virtual test without any explicit expectations for
        # either the virtual test or the base test.
        exp = self._test_expectations.get_expectations(
            'virtual/virtual_passes/passes/image.html')
        self.assertEqual(exp.results, set([ResultType.Pass]))
        self.assertTrue(exp.is_default_pass)
        self.assertFalse(exp.is_slow_test)
        self.assert_base_and_flag_exp(
            'virtual/virtual_passes/passes/image.html', ResultType.Pass, None)

        # No virtual test expectation. The flag-specific expectation of the
        # base test override the base expectation of the base test, but [ Slow ]
        # is inherited.
        exp = self._test_expectations.get_expectations(
            'virtual/virtual_failures/failures/expected/text.html')
        self.assertEqual(exp.results, set([ResultType.Failure]))
        self.assertFalse(exp.is_default_pass)
        self.assertTrue(exp.is_slow_test)
        self.assert_base_and_flag_exp(
            'virtual/virtual_failures/failures/expected/text.html',
            ResultType.Pass, ResultType.Failure)

        # The flag-specific virtual test expectation wins.
        exp = self._test_expectations.get_expectations(
            'virtual/virtual_failures/failures/expected/image.html')
        self.assertEqual(exp.results, set([ResultType.Failure]))
        self.assertFalse(exp.is_default_pass)
        self.assertFalse(exp.is_slow_test)
        self.assert_base_and_flag_exp(
            'virtual/virtual_failures/failures/expected/image.html',
            ResultType.Skip, ResultType.Failure)

        # No virtual test expectations. [ Slow ] in the flag-specific
        # expectation of the base test and [ Failure ] in the base expectation
        # of the base test merged.
        exp = self._test_expectations.get_expectations(
            'virtual/virtual_failures/failures/expected/reftest.html')
        self.assertEqual(exp.results, set([ResultType.Failure]))
        self.assertFalse(exp.is_default_pass)
        self.assertTrue(exp.is_slow_test)
        self.assert_base_and_flag_exp(
            'virtual/virtual_failures/failures/expected/reftest.html',
            ResultType.Failure, None)

        # No virtual test flag-specific expectation. The virtual test
        # expectation in the base expectation file wins.
        exp = self._test_expectations.get_expectations(
            'virtual/virtual_failures/failures/expected/crash.html')
        self.assertEqual(exp.results, set([ResultType.Pass]))
        self.assertFalse(exp.is_default_pass)
        self.assertFalse(exp.is_slow_test)
        self.assert_base_and_flag_exp(
            'virtual/virtual_failures/failures/expected/crash.html',
            ResultType.Pass, ResultType.Timeout)


class SystemConfigurationEditorTests(Base):
    def __init__(self, testFunc):
        super(SystemConfigurationEditorTests, self).__init__(testFunc)
        self._port.configuration_specifier_macros_dict = {
            'mac': ['mac10.10', 'mac10.11', 'mac10.12', 'mac10.13'],
            'win': ['win7', 'win10'],
            'linux': ['precise', 'trusty']
        }
        self.maxDiff = None

    def set_up_using_raw_expectations(self, content):
        self._general_exp_filename = self._port.host.filesystem.join(
            self._port.web_tests_dir(), 'TestExpectations')
        self._port.host.filesystem.write_text_file(self._general_exp_filename,
                                                   content)
        expectations_dict = {self._general_exp_filename: content}
        test_expectations = TestExpectations(self._port, expectations_dict)
        self._system_config_remover = SystemConfigurationEditor(
            test_expectations)

    def test_update_versions_with_autotriage(self):
        raw_expectations = textwrap.dedent("""\
            # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]
            # results: [ Failure Crash ]
            # Below Expectation should be split
            crbug.com/123 [ Mac ] failures/expected/text.html?\* [ Failure ]
            """)
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.update_versions(
            'failures/expected/text.html?*', {'Mac10.11'}, {ResultType.Crash})
        self.assertEqual(len(change.lines_removed), 1)
        self.assertEqual(len(change.lines_added), 3)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]
                # results: [ Failure Crash ]
                # Below Expectation should be split
                crbug.com/123 [ Mac10.10 ] failures/expected/text.html?\* [ Failure ]
                crbug.com/123 [ Mac10.11 ] failures/expected/text.html?\* [ Crash ]
                crbug.com/123 [ Mac10.12 ] failures/expected/text.html?\* [ Failure ]
                """))

    def test_update_versions_marker(self):
        raw_expectations = textwrap.dedent("""\
            # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]
            # results: [ Failure Crash ]

            # === wpt-importer ===
            [ Mac10.12 ] failures/expected/image.html [ Failure ]

            # Should not change:
            [ Mac ] failures/expected/text.html?\* [ Failure ]
            """)
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.update_versions(
            'failures/expected/image.html', {'Mac10.11'}, {ResultType.Crash},
            marker='=== wpt-importer ===')
        self.assertEqual(len(change.lines_removed), 0)
        self.assertEqual(len(change.lines_added), 1)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]
                # results: [ Failure Crash ]

                # === wpt-importer ===
                [ Mac10.11 ] failures/expected/image.html [ Crash ]
                [ Mac10.12 ] failures/expected/image.html [ Failure ]

                # Should not change:
                [ Mac ] failures/expected/text.html?\* [ Failure ]
                """))

    def test_update_versions_marker_not_found(self):
        raw_expectations = textwrap.dedent("""\
            # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]
            # tags: [ Debug Release ]
            # results: [ Failure Crash ]
            [ Mac Debug ] failures/expected/text.html?\* [ Failure ]
            """)
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.update_versions(
            'failures/expected/text.html?*', {'Mac10.11'},
            {ResultType.Failure},
            marker='create-me')
        self.assertEqual(len(change.lines_removed), 1)
        self.assertEqual(len(change.lines_added), 3)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]
                # tags: [ Debug Release ]
                # results: [ Failure Crash ]
                [ Debug Mac10.10 ] failures/expected/text.html?\* [ Failure ]
                [ Debug Mac10.12 ] failures/expected/text.html?\* [ Failure ]

                # create-me
                [ Mac10.11 ] failures/expected/text.html?\* [ Failure ]
                """))

    def test_update_versions_end_of_file(self):
        raw_expectations = textwrap.dedent("""\
            # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]
            # results: [ Failure Crash ]
            # Below Expectation should be split
            crbug.com/123 [ Mac ] failures/expected/text.html?\* [ Failure ]  # comment

            """)
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.update_versions(
            'failures/expected/text.html?*', {'Mac10.11'},
            {ResultType.Failure, ResultType.Crash},
            autotriage=False)
        self.assertEqual(len(change.lines_removed), 1)
        self.assertEqual(len(change.lines_added), 3)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]
                # results: [ Failure Crash ]
                # Below Expectation should be split
                crbug.com/123 [ Mac10.10 ] failures/expected/text.html?\* [ Failure ]  # comment
                crbug.com/123 [ Mac10.12 ] failures/expected/text.html?\* [ Failure ]  # comment

                [ Mac10.11 ] failures/expected/text.html?\* [ Crash Failure ]
                """))

    def test_update_then_merge_without_net_change(self):
        raw_expectations = textwrap.dedent("""\
            # tags: [ Win7 Win10 Win ]
            # results: [ Failure Crash ]
            crbug.com/123 [ Win ] failures/expected/text.html?\* [ Failure ]
            """)
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.update_versions(
            'failures/expected/text.html?*', {'Win7'}, {ResultType.Failure})
        change += self._system_config_remover.merge_versions(
            'failures/expected/text.html?*')
        self.assertEqual(len(change.lines_removed), 0)
        self.assertEqual(len(change.lines_added), 0)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(updated_exps, raw_expectations)

    def test_merge_versions_os(self):
        raw_expectations = textwrap.dedent("""\
            # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac10.13 Mac Win7 Win10 Win ]
            # results: [ Failure Crash ]
            # Below Expectation should be merged
            crbug.com/123 [ Win7 ] failures/expected/text.html?\* [ Failure ]  # comment
            crbug.com/123 [ Mac10.10 ] failures/expected/text.html?\* [ Failure ]  # comment
            crbug.com/123 [ Mac10.11 ] failures/expected/text.html?\* [ Failure ]  # comment
            crbug.com/456 [ Mac10.12 ] failures/expected/text.html?\* [ Failure ]  # comment 2
            crbug.com/456 [ Mac10.13 ] failures/expected/text.html?\* [ Failure ]  # comment 2
            """)
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.merge_versions(
            'failures/expected/text.html?*')
        self.assertEqual(len(change.lines_removed), 4)
        self.assertEqual(len(change.lines_added), 1)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac10.13 Mac Win7 Win10 Win ]
                # results: [ Failure Crash ]
                # Below Expectation should be merged
                crbug.com/123 [ Win7 ] failures/expected/text.html?\* [ Failure ]  # comment
                crbug.com/123 crbug.com/456 [ Mac ] failures/expected/text.html?\* [ Failure ]  # comment, comment 2
                """))

    def test_merge_versions_generic(self):
        raw_expectations = textwrap.dedent("""\
            # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac10.13 Mac Win7 Win10 Win Precise Trusty Linux ]
            # results: [ Failure Crash ]
            # Below Expectation should be merged
            crbug.com/123 [ Win7 ] failures/expected/text.html?\* [ Failure ]  # comment
            crbug.com/123 [ Win10 ] failures/expected/text.html?\* [ Failure ]  # comment
            crbug.com/123 [ Mac ] failures/expected/text.html?\* [ Failure ]  # comment
            crbug.com/123 [ Precise ] failures/expected/text.html?\* [ Failure ]  # comment
            crbug.com/123 [ Trusty ] failures/expected/text.html?\* [ Failure ]  # comment
            """)
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.merge_versions(
            'failures/expected/text.html?*')
        self.assertEqual(len(change.lines_removed), 5)
        self.assertEqual(len(change.lines_added), 1)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac10.13 Mac Win7 Win10 Win Precise Trusty Linux ]
                # results: [ Failure Crash ]
                # Below Expectation should be merged
                crbug.com/123 failures/expected/text.html?\* [ Failure ]  # comment
                """))

    def test_merge_versions_with_other_specifiers(self):
        raw_expectations = textwrap.dedent("""\
            # tags: [ Win7 Win10 Win ]
            # tags: [ Debug Release ]
            # results: [ Failure Crash ]
            crbug.com/123 [ Debug Win7 ] failures/expected/text.html?\* [ Crash ]  # DCHECK triggered
            crbug.com/123 [ Debug Win10 ] failures/expected/text.html?\* [ Crash ]
            """)
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.merge_versions(
            'failures/expected/text.html?*')
        self.assertEqual(len(change.lines_removed), 2)
        self.assertEqual(len(change.lines_added), 1)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            textwrap.dedent("""\
                # tags: [ Win7 Win10 Win ]
                # tags: [ Debug Release ]
                # results: [ Failure Crash ]
                crbug.com/123 [ Debug Win ] failures/expected/text.html?\* [ Crash ]  # DCHECK triggered
                """))

    def test_merge_versions_skip_with_different_results(self):
        raw_expectations = textwrap.dedent("""\
            # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac10.13 Mac Win7 Win10 Win Precise Trusty Linux ]
            # results: [ Failure Crash ]
            # The Win and Linux expectations should be merged.
            # The Mac results prevent merging to a generic expectation.
            crbug.com/123 [ Win7 ] failures/expected/text.html?\* [ Failure ]  # comment
            crbug.com/123 [ Win10 ] failures/expected/text.html?\* [ Failure ]  # comment
            crbug.com/123 [ Mac ] failures/expected/text.html?\* [ Crash Failure ]  # comment
            crbug.com/123 [ Precise ] failures/expected/text.html?\* [ Failure ]  # comment
            crbug.com/123 [ Trusty ] failures/expected/text.html?\* [ Failure ]  # comment
            """)
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.merge_versions(
            'failures/expected/text.html?*')
        self.assertEqual(len(change.lines_removed), 4)
        self.assertEqual(len(change.lines_added), 2)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            textwrap.dedent("""\
                # tags: [ Mac10.10 Mac10.11 Mac10.12 Mac10.13 Mac Win7 Win10 Win Precise Trusty Linux ]
                # results: [ Failure Crash ]
                # The Win and Linux expectations should be merged.
                # The Mac results prevent merging to a generic expectation.
                crbug.com/123 [ Win ] failures/expected/text.html?\* [ Failure ]  # comment
                crbug.com/123 [ Mac ] failures/expected/text.html?\* [ Crash Failure ]  # comment
                crbug.com/123 [ Linux ] failures/expected/text.html?\* [ Failure ]  # comment
                """))

    def test_merge_versions_skip_with_disjoint_specifiers(self):
        raw_expectations = textwrap.dedent("""\
            # tags: [ Win7 Win10 Win ]
            # tags: [ Debug Release ]
            # results: [ Failure Crash ]
            # Debug and Release describe disjoint test configurations.
            crbug.com/123 [ Debug Win7 ] failures/expected/text.html?\* [ Failure ]
            crbug.com/123 [ Release Win10 ] failures/expected/text.html?\* [ Failure ]
            """)
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.merge_versions(
            'failures/expected/text.html?*')
        self.assertEqual(len(change.lines_removed), 0)
        self.assertEqual(len(change.lines_added), 0)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            textwrap.dedent("""\
                # tags: [ Win7 Win10 Win ]
                # tags: [ Debug Release ]
                # results: [ Failure Crash ]
                # Debug and Release describe disjoint test configurations.
                crbug.com/123 [ Debug Win7 ] failures/expected/text.html?\* [ Failure ]
                crbug.com/123 [ Release Win10 ] failures/expected/text.html?\* [ Failure ]
                """))

    def test_remove_mac_version_from_mac_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be split\n'
            '[ Mac ] failures/expected/text.html?\* [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.remove_os_versions(
            'failures/expected/text.html?*', set(['Mac10.10']))
        self.assertEqual(len(change.lines_removed), 1)
        self.assertEqual(len(change.lines_added), 2)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            ('# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]\n'
             '# results: [ Failure ]\n'
             '# Below Expectation should be split\n'
             '[ Mac10.11 ] failures/expected/text.html?\* [ Failure ]\n'
             '[ Mac10.12 ] failures/expected/text.html?\* [ Failure ]\n'))

    def test_remove_mac_version_from_linux_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Linux ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be unaffected\n'
            '[ Linux ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.remove_os_versions(
            'failures/expected/text.html', set(['Mac10.10']))
        self.assertEqual(len(change.lines_removed), 0)
        self.assertEqual(len(change.lines_added), 0)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(updated_exps, raw_expectations)

    def test_remove_mac_version_from_all_config_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be split\n'
            'failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.remove_os_versions(
            'failures/expected/text.html', set(['Mac10.10']))
        self.assertEqual(len(change.lines_removed), 1)
        self.assertEqual(len(change.lines_added), 4)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            ('# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
             '# results: [ Failure ]\n'
             '# Below Expectation should be split\n'
             '[ Linux ] failures/expected/text.html [ Failure ]\n'
             '[ Mac10.11 ] failures/expected/text.html [ Failure ]\n'
             '[ Mac10.12 ] failures/expected/text.html [ Failure ]\n'
             '[ Win ] failures/expected/text.html [ Failure ]\n'))

    def test_remove_all_mac_versions_from_mac_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]\n'
            '# results: [ Failure ]\n'
            '# The expectation below and this comment block should be deleted\n'
            '[ Mac ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.remove_os_versions(
            'failures/expected/text.html',
            {'Mac10.10', 'Mac10.11', 'Mac10.12'})
        self.assertEqual(len(change.lines_removed), 1)
        self.assertEqual(len(change.lines_added), 0)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(updated_exps,
                         ('# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac ]\n'
                          '# results: [ Failure ]\n'))

    def test_remove_all_mac_versions_from_all_config_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be split\n'
            'failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.remove_os_versions(
            'failures/expected/text.html',
            {'Mac10.10', 'Mac10.11', 'Mac10.12'})
        self.assertEqual(len(change.lines_removed), 1)
        self.assertEqual(len(change.lines_added), 2)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            ('# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
             '# results: [ Failure ]\n'
             '# Below Expectation should be split\n'
             '[ Linux ] failures/expected/text.html [ Failure ]\n'
             '[ Win ] failures/expected/text.html [ Failure ]\n'))

    def test_remove_all_mac_versions_from_linux_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be unaffected\n'
            '[ Linux ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.remove_os_versions(
            'failures/expected/text.html',
            {'Mac10.10', 'Mac10.11', 'Mac10.12'})
        self.assertEqual(len(change.lines_removed), 0)
        self.assertEqual(len(change.lines_added), 0)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(updated_exps, raw_expectations)

    def test_remove_all_configs(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation and this comment should be deleted\n'
            'failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        all_versions = reduce(
            lambda x, y: x + y,
            list(self._port.configuration_specifier_macros_dict.values()))
        change = self._system_config_remover.remove_os_versions(
            'failures/expected/text.html', all_versions)
        self.assertEqual(len(change.lines_removed), 1)
        self.assertEqual(len(change.lines_added), 0)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            ('# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
             '# results: [ Failure ]\n'))

    def test_remove_all_configs2(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation and this comment should be deleted\n'
            '[ Mac ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        all_versions = reduce(
            lambda x, y: x + y,
            list(self._port.configuration_specifier_macros_dict.values()))
        change = self._system_config_remover.remove_os_versions(
            'failures/expected/text.html', all_versions)
        self.assertEqual(len(change.lines_removed), 1)
        self.assertEqual(len(change.lines_added), 0)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(
            updated_exps,
            ('# tags: [ Mac10.10 Mac10.11 Mac10.12 Mac Linux Win ]\n'
             '# results: [ Failure ]\n'))

    def test_remove_mac_version_from_another_mac_version_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Linux ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation should be unaffected\n'
            '[ Mac10.11 ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.remove_os_versions(
            'failures/expected/text.html', set(['Mac10.10']))
        self.assertEqual(len(change.lines_removed), 0)
        self.assertEqual(len(change.lines_added), 0)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(updated_exps, raw_expectations)

    def test_remove_mac_version_from_same_mac_version_expectation(self):
        raw_expectations = (
            '# tags: [ Mac10.10 Mac10.11 Linux ]\n'
            '# results: [ Failure ]\n'
            '# Below Expectation as well as this comment should be deleted\n'
            '[ Mac10.10 ] failures/expected/text.html [ Failure ]\n')
        self.set_up_using_raw_expectations(raw_expectations)
        change = self._system_config_remover.remove_os_versions(
            'failures/expected/text.html', set(['Mac10.10']))
        self.assertEqual(len(change.lines_removed), 1)
        self.assertEqual(len(change.lines_added), 0)
        self._system_config_remover.update_expectations()
        updated_exps = self._port.host.filesystem.read_text_file(
            self._general_exp_filename)
        self.assertEqual(updated_exps, ('# tags: [ Mac10.10 Mac10.11 Linux ]\n'
                                        '# results: [ Failure ]\n'))


class MiscTests(Base):
    def test_parse_mac_legacy_names(self):
        host = MockHost()
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = (
            '# tags: [ Mac10.10 ]\n'
            '# results: [ Failure ]\n'
            '[ Mac10.10 ] failures/expected/text.html [ Failure ]\n')

        port = host.port_factory.get('test-mac-mac10.10', None)
        port.expectations_dict = lambda: expectations_dict
        expectations = TestExpectations(port)
        self.assertEqual(
            expectations.get_expectations('failures/expected/text.html').
            results, {ResultType.Failure})

        port = host.port_factory.get('test-win-win7', None)
        port.expectations_dict = lambda: expectations_dict
        expectations = TestExpectations(port)
        self.assertEqual(
            expectations.get_expectations('failures/expected/text.html').
            results, {ResultType.Pass})

    def test_get_test_with_expected_result(self):
        test_expectations = (
            '# tags: [ win7 linux ]\n'
            '# results: [ Failure ]\n'
            '[ win7 ] failures/expected/text.html [ Failure ]\n'
            '[ linux ] failures/expected/image_checksum.html [ Failure ]\n')
        self.parse_exp(test_expectations)
        self.assertEqual(
            self._exp.get_tests_with_expected_result(ResultType.Failure),
            set(['failures/expected/text.html']))

    def test_multiple_results(self):
        self.parse_exp(
            '# results: [ Crash Failure ]\nfailures/expected/text.html [ Crash Failure ]'
        )
        self.assertEqual(
            self._exp.get_expectations('failures/expected/text.html').results,
            {ResultType.Failure, ResultType.Crash})

    def test_overrides_include_slow(self):
        self.parse_exp(
            '# results: [ Failure ]\nfailures/expected/text.html [ Failure ]',
            '# results: [ Slow ]\nfailures/expected/text.html [ Slow ]')
        self.assert_exp_list('failures/expected/text.html',
                             set([ResultType.Failure]))
        self.assertTrue(
            self._exp.get_expectations('failures/expected/text.html').
            is_slow_test)

    def test_overrides(self):
        self.parse_exp(
            '# results: [ Failure ]\nfailures/expected/text.html [ Failure ]',
            '# results: [ Timeout ]\nfailures/expected/text.html [ Timeout ]')
        self.assert_exp_list('failures/expected/text.html',
                             {ResultType.Failure, ResultType.Timeout})

    def test_more_specific_override_resets_skip(self):
        self.parse_exp(
            '# results: [ Skip ]\nfailures/expected* [ Skip ]',
            '# results: [ Failure ]\nfailures/expected/text.html [ Failure ]')
        self.assert_exp_list('failures/expected/text.html',
                             {ResultType.Failure, ResultType.Skip})

    def test_bot_test_expectations(self):
        """Test that expectations are merged rather than overridden when using flaky option 'unexpected'."""
        test_name1 = 'failures/expected/text.html'
        test_name2 = 'passes/text.html'

        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = (
            '# results: [ Failure Crash ]\n%s [ Failure ]\n%s [ Crash ]\n' %
            (test_name1, test_name2))
        self._port.expectations_dict = lambda: expectations_dict

        expectations = TestExpectations(self._port)
        self.assertEqual(
            expectations.get_expectations(test_name1).results,
            set([ResultType.Failure]))
        self.assertEqual(
            expectations.get_expectations(test_name2).results,
            set([ResultType.Crash]))

        def bot_expectations():
            return {
                test_name1: [ResultType.Pass, ResultType.Timeout],
                test_name2: [ResultType.Crash]
            }

        self._port.bot_expectations = bot_expectations
        self._port._options.ignore_flaky_tests = 'unexpected'

        expectations = TestExpectations(self._port)
        self.assertEqual(
            expectations.get_expectations(test_name1).results,
            set([ResultType.Pass, ResultType.Failure, ResultType.Timeout]))
        self.assertEqual(
            expectations.get_expectations(test_name2).results,
            set([ResultType.Crash]))


class RemoveExpectationsTest(Base):
    def test_remove_expectation(self):
        port = MockHost().port_factory.get('test-win-win7')
        raw_expectations = ('# tags: [ Mac Win ]\n'
                            '# results: [ Failure ]\n'
                            '\n'
                            '# This comment will be deleted\n'
                            '[ mac ] test1 [ Failure ]\n')
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = ''
        expectations_dict['/tmp/TestExpectations2'] = raw_expectations
        test_expectations = TestExpectations(port, expectations_dict)
        test_to_exps = test_expectations._expectations[1].individual_exps
        test_expectations.remove_expectations('/tmp/TestExpectations2',
                                              [test_to_exps['test1'][0]])
        test_expectations.commit_changes()
        content = port.host.filesystem.read_text_file('/tmp/TestExpectations2')
        self.assertEqual(content, ('# tags: [ Mac Win ]\n'
                                   '# results: [ Failure ]\n'))

    def test_readd_removed_expectation_instance(self):
        port = MockHost().port_factory.get('test-win-win7')
        raw_expectations = ('# tags: [ Mac Win ]\n'
                            '# results: [ Failure ]\n'
                            '\n'
                            '# This comment will not be deleted\n'
                            '[ mac ] test1 [ Failure ]\n'
                            '[ mac ] test2 [ Failure ]\n'
                            '[ mac ] test3 [ Failure ]\n'
                            '[ mac ] test4 [ Failure ]\n'
                            '[ mac ] test5 [ Failure ]\n'
                            '[ mac ] test6 [ Failure ]\n'
                            '[ mac ] test7 [ Failure ]\n')
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = ''
        expectations_dict['/tmp/TestExpectations2'] = raw_expectations
        test_expectations = TestExpectations(port, expectations_dict)
        test_to_exps = test_expectations._expectations[1].individual_exps
        exp = test_expectations._expectations[1].individual_exps['test1'][0]
        exps_to_remove = [test_to_exps[
            'test%d' % case_no][0] for case_no in range(1, 8)]
        test_expectations.remove_expectations(
            '/tmp/TestExpectations2', exps_to_remove)
        test_expectations.add_expectations(
            '/tmp/TestExpectations2',[exp],
            lineno=4)
        test_expectations.commit_changes()
        content = port.host.filesystem.read_text_file('/tmp/TestExpectations2')
        self.assertEqual(content, ('# tags: [ Mac Win ]\n'
                                   '# results: [ Failure ]\n'
                                   '\n'
                                   '# This comment will not be deleted\n'
                                   '[ mac ] test1 [ Failure ]\n'))

    def test_remove_added_expectations(self):
        port = MockHost().port_factory.get('test-win-win7')
        raw_expectations = ('# tags: [ Mac Win ]\n'
                            '# results: [ Failure ]\n'
                            '\n'
                            '# This comment will be deleted\n'
                            '[ mac ] test1 [ Failure ]\n')
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = ''
        expectations_dict['/tmp/TestExpectations2'] = raw_expectations
        test_expectations = TestExpectations(port, expectations_dict)
        test_expectations.add_expectations('/tmp/TestExpectations2', [
            Expectation(test='test2', results=set([ResultType.Failure])),
            Expectation(
                test='test3',
                results=set([ResultType.Crash]),
                tags=set(['win']))
        ], 5)
        test_expectations.remove_expectations('/tmp/TestExpectations2', [
            Expectation(
                test='test2', results=set([ResultType.Failure]), lineno=5)
        ])
        test_expectations.commit_changes()
        content = port.host.filesystem.read_text_file('/tmp/TestExpectations2')
        self.assertEqual(content, ('# tags: [ Mac Win ]\n'
                                   '# results: [ Failure ]\n'
                                   '\n'
                                   '# This comment will be deleted\n'
                                   '[ Win ] test3 [ Crash ]\n'
                                   '[ mac ] test1 [ Failure ]\n'))

    def test_remove_after_add(self):
        port = MockHost().port_factory.get('test-win-win7')
        raw_expectations = ('# tags: [ Mac Win ]\n'
                            '# results: [ Failure Crash ]\n'
                            '\n'
                            '# This comment will not be deleted\n'
                            '[ mac ] test1 [ Failure ]\n')
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = ''
        expectations_dict['/tmp/TestExpectations2'] = raw_expectations
        test_expectations = TestExpectations(port, expectations_dict)
        test_to_exps = test_expectations._expectations[1].individual_exps
        test_expectations.add_expectations('/tmp/TestExpectations2', [
            Expectation(test='test2', results=set([ResultType.Failure])),
            Expectation(
                test='test3',
                results=set([ResultType.Crash]),
                tags=set(['mac']))
        ], 5)
        test_expectations.remove_expectations('/tmp/TestExpectations2',
                                              [test_to_exps['test1'][0]])
        test_expectations.commit_changes()
        content = port.host.filesystem.read_text_file('/tmp/TestExpectations2')
        self.assertEqual(content, ('# tags: [ Mac Win ]\n'
                                   '# results: [ Failure Crash ]\n'
                                   '\n'
                                   '# This comment will not be deleted\n'
                                   '[ Mac ] test3 [ Crash ]\n'
                                   'test2 [ Failure ]\n'))


class AddExpectationsTest(Base):
    def test_add_expectation_with_negative_lineno(self):
        port = MockHost().port_factory.get('test-win-win7')
        raw_expectations = ('# tags: [ Mac Win ]\n'
                            '# tags: [ release ]\n'
                            '# results: [ Failure ]\n'
                            '\n'
                            '# this is a block of expectations\n'
                            'test [ Failure ]\n')
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = ''
        expectations_dict['/tmp/TestExpectations2'] = raw_expectations
        test_expectations = TestExpectations(port, expectations_dict)

        with self.assertRaises(ValueError) as ctx:
            test_expectations.add_expectations(
                '/tmp/TestExpectations2',
                [Expectation(test='test3',
                             results=set([ResultType.Failure]))],
                lineno=-1)
            test_expectations.commit_changes()
        self.assertIn('cannot be negative', str(ctx.exception))

    def test_add_expectation_outside_file_size_range(self):
        port = MockHost().port_factory.get('test-win-win7')
        raw_expectations = ('# tags: [ Mac Win ]\n'
                            '# tags: [ release ]\n'
                            '# results: [ Failure ]\n'
                            '\n'
                            '# this is a block of expectations\n'
                            'test [ Failure ]\n')
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = ''
        expectations_dict['/tmp/TestExpectations2'] = raw_expectations
        test_expectations = TestExpectations(port, expectations_dict)

        with self.assertRaises(ValueError) as ctx:
            test_expectations.add_expectations(
                '/tmp/TestExpectations2',
                [Expectation(test='test3',
                             results=set([ResultType.Failure]))],
                lineno=100)
            test_expectations.commit_changes()
        self.assertIn('greater than the total line count', str(ctx.exception))

    def test_add_expectations_to_end_of_file(self):
        port = MockHost().port_factory.get('test-win-win7')
        raw_expectations = ('# tags: [ Mac Win ]\n'
                            '# tags: [ release ]\n'
                            '# results: [ Failure ]\n'
                            '\n'
                            '# this is a block of expectations\n'
                            'test [ Failure ]\n')
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = ''
        expectations_dict['/tmp/TestExpectations2'] = raw_expectations
        test_expectations = TestExpectations(port, expectations_dict)
        test_expectations.add_expectations(
            '/tmp/TestExpectations2',
            [Expectation(test='test3', results=set([ResultType.Failure]))])
        test_expectations.add_expectations('/tmp/TestExpectations2', [
            Expectation(test='test2',
                        tags={'mac', 'release'},
                        results={ResultType.Crash, ResultType.Failure})
        ])
        test_expectations.add_expectations(
            '/tmp/TestExpectations2',
            [Expectation(test='test1', results=set([ResultType.Pass]))])
        test_expectations.commit_changes()
        content = port.host.filesystem.read_text_file('/tmp/TestExpectations2')
        self.assertEqual(content, ('# tags: [ Mac Win ]\n'
                                   '# tags: [ release ]\n'
                                   '# results: [ Failure ]\n'
                                   '\n'
                                   '# this is a block of expectations\n'
                                   'test [ Failure ]\n'
                                   '\n'
                                   'test1 [ Pass ]\n'
                                   '[ Mac Release ] test2 [ Crash Failure ]\n'
                                   'test3 [ Failure ]\n'))

    def test_add_after_remove(self):
        port = MockHost().port_factory.get('test-win-win7')
        raw_expectations = ('# tags: [ Mac Win ]\n'
                            '# results: [ Failure Crash ]\n'
                            'test1 [ Failure ]\n')
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = ''
        expectations_dict['/tmp/TestExpectations2'] = raw_expectations
        test_expectations = TestExpectations(port, expectations_dict)
        test_expectations.remove_expectations('/tmp/TestExpectations2', [
            Expectation(
                test='test1', results=set([ResultType.Failure]), lineno=3)
        ])
        test_expectations.add_expectations(
            '/tmp/TestExpectations2',
            [Expectation(test='test2', results=set([ResultType.Crash]))], 3)
        test_expectations.commit_changes()
        content = port.host.filesystem.read_text_file('/tmp/TestExpectations2')
        self.assertEqual(content, ('# tags: [ Mac Win ]\n'
                                   '# results: [ Failure Crash ]\n'
                                   'test2 [ Crash ]\n'))

    def test_add_expectation_at_line(self):
        port = MockHost().port_factory.get('test-win-win7')
        raw_expectations = ('# tags: [ Mac Win ]\n'
                            '# results: [ Failure Crash ]\n'
                            '\n'
                            '# add expectations after this line\n'
                            'test1 [ Failure ]\n'
                            '\n')
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = raw_expectations
        test_expectations = TestExpectations(port, expectations_dict)
        test_expectations.add_expectations('/tmp/TestExpectations', [
            Expectation(
                test='test2',
                results=set([ResultType.Crash]),
                tags=set(['win']))
        ], 4)
        test_expectations.remove_expectations('/tmp/TestExpectations', [
            Expectation(
                test='test1', results=set([ResultType.Failure]), lineno=5)
        ])
        test_expectations.commit_changes()
        content = port.host.filesystem.read_text_file('/tmp/TestExpectations')
        self.assertEqual(content, ('# tags: [ Mac Win ]\n'
                                   '# results: [ Failure Crash ]\n'
                                   '\n'
                                   '# add expectations after this line\n'
                                   '[ Win ] test2 [ Crash ]\n'
                                   '\n'))


class ExpectationsConflictResolutionTest(Base):
    def test_remove_expectation(self):
        port = MockHost().port_factory.get('test-win-win7')
        raw_expectations_1 = ('# tags: [ Mac Win ]\n'
                            '# results: [ Failure Pass ]\n'
                            '\n'
                            'crbug.com/2432 [ Win ] test1 [ Failure ]\n')
        raw_expectations_2 = ('# tags: [ Mac Win ]\n'
                            '# results: [ Failure Pass ]\n'
                            '\n'
                            'crbug.com/2432 [ Win ] test1 [ Pass ]\n')
        raw_expectations_3 = ('# tags: [ Mac Win ]\n'
                            '# results: [ Failure Pass ]\n'
                            '# conflict_resolution: Override \n'
                            '\n'
                            'crbug.com/2432 [ Win ] test1 [ Pass ]\n')
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = raw_expectations_1
        expectations_dict['/tmp/TestExpectations2'] = raw_expectations_2
        test_expectations = TestExpectations(port, expectations_dict)
        self.assertEqual(test_expectations.get_expectations('test1'),
                         Expectation(
                             test='test1', results=set([ResultType.Pass, ResultType.Failure]),
                             is_slow_test=False, reason='crbug.com/2432'
                         ))
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = raw_expectations_1
        expectations_dict['/tmp/TestExpectations2'] = raw_expectations_3
        test_expectations = TestExpectations(port, expectations_dict)
        self.assertEqual(test_expectations.get_expectations('test1'),
                         Expectation(
                             test='test1', results=set([ResultType.Pass]),
                             is_slow_test=False, reason='crbug.com/2432'
                         ))


class CommitChangesTests(Base):
    def test_commit_changes_without_modifications(self):
        port = MockHost().port_factory.get('test-win-win7')
        raw_expectations = ('# tags: [ Mac Win ]\n'
                            '# results: [ Failure Crash ]\n'
                            '\n'
                            '# add expectations after this line\n'
                            'test1 [ Failure ]\n'
                            '\n')
        expectations_dict = OrderedDict()
        expectations_dict['/tmp/TestExpectations'] = raw_expectations
        test_expectations = TestExpectations(port, expectations_dict)
        test_expectations.commit_changes()
        content = port.host.filesystem.read_text_file('/tmp/TestExpectations')
        self.assertEqual(content, ('# tags: [ Mac Win ]\n'
                                   '# results: [ Failure Crash ]\n'
                                   '\n'
                                   '# add expectations after this line\n'
                                   'test1 [ Failure ]\n'
                                   '\n'))


class SkippedTests(Base):
    def check(self,
              expectations,
              overrides,
              ignore_tests,
              lint=False,
              expected_results=None):
        expected_results = expected_results or [
            ResultType.Skip, ResultType.Failure
        ]
        port = MockHost().port_factory.get(
            'test-win-win7',
            options=optparse.Values({
                'ignore_tests': ignore_tests
            }))
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.web_tests_dir(),
                                      'failures/expected/text.html'), 'foo')
        expectations_dict = OrderedDict()
        expectations_dict['expectations'] = expectations
        if overrides:
            expectations_dict['overrides'] = overrides
        port.expectations_dict = lambda: expectations_dict
        expectations_to_lint = expectations_dict if lint else None
        exp = TestExpectations(port, expectations_dict=expectations_to_lint)
        self.assertEqual(
            exp.get_expectations('failures/expected/text.html').results,
            set(expected_results))

    def test_skipped_file_overrides_expectations(self):
        self.check(
            expectations=
            '# results: [ Failure Skip ]\nfailures/expected/text.html [ Failure ]\n',
            overrides=None,
            ignore_tests=['failures/expected/text.html'])

    def test_skipped_file_overrides_overrides(self):
        self.check(
            expectations='# results: [ Skip Failure ]\n',
            overrides=
            '# results: [ Skip Failure ]\nfailures/expected/text.html [ Failure ]\n',
            ignore_tests=['failures/expected/text.html'])


class PrecedenceTests(Base):
    def test_file_over_directory(self):
        # This tests handling precedence of specific lines over directories
        # and tests expectations covering entire directories.
        exp_str = """
# results: [ Failure Crash ]
failures/expected/text.html [ Failure ]
failures/expected* [ Crash ]
"""
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', ResultType.Failure)
        self.assert_exp_list('failures/expected/crash.html',
                             [ResultType.Crash])

        exp_str = """
# results: [ Failure Crash ]
failures/expected* [ Crash ]
failures/expected/text.html [ Failure ]
"""
        self.parse_exp(exp_str)
        self.assert_exp('failures/expected/text.html', ResultType.Failure)
        self.assert_exp_list('failures/expected/crash.html',
                             [ResultType.Crash])

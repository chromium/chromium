# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import json
import subprocess
import textwrap
from unittest import mock
from typing import List

from blinkpy.common import path_finder
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.tool.mock_tool import MockBlinkTool
from blinkpy.tool.commands.lint_wpt import LintError, LintWPT
from blinkpy.w3c.wpt_metadata import TestConfigurations

path_finder.bootstrap_wpt_imports()
from wptrunner import metadata


class LintWPTTest(LoggingTestCase):
    def setUp(self):
        super().setUp()
        self.maxDiff = None
        self.tool = MockBlinkTool()
        self.fs = self.tool.filesystem
        self.finder = path_finder.PathFinder(self.fs)
        configs = [{
            'os': 'mac',
            'flag_specific': None,
            'product': 'content_shell',
        }, {
            'os': 'win',
            'flag_specific': None,
            'product': 'content_shell',
        }, {
            'os': 'linux',
            'flag_specific': None,
            'product': 'content_shell',
        }, {
            'os': 'linux',
            'flag_specific': None,
            'product': 'chrome',
        }, {
            'os': 'linux',
            'flag_specific': 'fake-flag',
            'product': 'content_shell',
        }]
        configs = {
            metadata.RunInfo(config):
            self.tool.port_factory.get('test-linux-trusty')
            for config in configs
        }
        self.command = LintWPT(self.tool, TestConfigurations(self.fs, configs))
        self.fs.write_text_file(self.finder.path_from_wpt_tests('lint.ignore'),
                                '')
        self.fs.write_text_file(
            self.finder.path_from_wpt_tests('MANIFEST.json'),
            json.dumps({
                'version': 8,
                'items': {
                    'reftest': {
                        'reftest.html': [
                            'd933fd981d4a33ba82fb2b000234859bdda1494e',
                            [None, {}],
                        ],
                    },
                    'testharness': {
                        'dir': {
                            'multiglob.https.any.js': [
                                'd6498c3e388e0c637830fa080cca78b0ab0e5305',
                                ['dir/multiglob.https.any.html', {}],
                                ['dir/multiglob.https.any.worker.html', {}],
                            ],
                        },
                        'variant.html': [
                            'b8db5972284d1ac6bbda0da81621d9bca5d04ee7',
                            ['variant.html?foo=bar/abc', {
                                'timeout': 'long'
                            }],
                            ['variant.html?foo=baz', {
                                'timeout': 'long'
                            }],
                        ],
                    },
                },
            }))

    @contextlib.contextmanager
    def _patch_builtins(self):
        with contextlib.ExitStack() as stack:
            # Absorb standard library calls to ensure no real resources are
            # consumed.
            stack.enter_context(
                mock.patch('tools.lint.lint.multiprocessing.cpu_count',
                           return_value=1))
            stack.enter_context(
                mock.patch('tools.lint.lint.subprocess',
                           side_effect=subprocess.CalledProcessError))
            stack.enter_context(
                mock.patch('tools.lint.lint.changed_files', return_value=[]))
            stack.enter_context(self.tool.filesystem.patch_builtins())
            stack.enter_context(self.tool.executive.patch_builtins())
            yield stack

    def test_execute_basic_ok(self):
        path = self.finder.path_from_wpt_tests('good_python.py')
        self.fs.write_text_file(path, 'import os')
        with self._patch_builtins():
            exit_code = self.command.main(['--no-manifest-update', path])
        self.assertEqual(exit_code, 0)
        self.assertLog(['INFO: All files OK.\n'])

    def test_execute_basic_test_file_error(self):
        path = self.finder.path_from_wpt_tests('bad_python.py')
        self.fs.write_text_file(path, 'invalid syntax should be detected')
        with self._patch_builtins():
            exit_code = self.command.main(['--no-manifest-update', path])
        self.assertNotEqual(exit_code, 0)
        self.assertLog([
            'ERROR: bad_python.py:1: Unable to parse file (PARSE-FAILED)\n',
            'INFO: \n',
            'INFO: There was 1 error (PARSE-FAILED: 1)\n',
            'INFO: \n',
            'INFO: You must address all errors; for details on how to fix '
            'them, see\n',
            'INFO: https://web-platform-tests.org/writing-tests/'
            'lint-tool.html\n',
            'INFO: \n',
            "INFO: However, for errors in test files, it's sometimes OK "
            'to add lines to\n',
            'INFO: `external/wpt/lint.ignore` to ignore them.\n',
            'INFO: \n',
            'INFO: For example, to make the lint tool ignore all '
            "'PARSE-FAILED' errors in\n",
            'INFO: the bad_python.py file, you could add the following line '
            'to the\n',
            'INFO: lint.ignore file:\n',
            'INFO: \n',
            'INFO: PARSE-FAILED: bad_python.py\n',
        ])

    def test_execute_basic_metadata_file_error(self):
        path = self.finder.path_from_wpt_tests('reftest.html.ini')
        self.fs.write_text_file(path, '[reftest.html]')
        with self._patch_builtins():
            exit_code = self.command.main(['--no-manifest-update', path])
        self.assertNotEqual(exit_code, 0)
        self.assertLog([
            'ERROR: reftest.html.ini: Empty section should be removed: '
            "'[reftest.html]' (META-EMPTY-SECTION)\n",
            'INFO: \n',
            'INFO: There was 1 error (META-EMPTY-SECTION: 1)\n',
            'INFO: \n',
            'INFO: You must address all errors; for details on how to fix '
            'them, see\n',
            'INFO: https://web-platform-tests.org/writing-tests/'
            'lint-tool.html\n',
            'INFO: \n',
            'INFO: Errors for `*.ini` metadata files cannot be ignored and '
            'must be fixed.\n',
        ])

    def _check_metadata(self,
                        contents: str,
                        path: str = 'reftest.html.ini') -> List[LintError]:
        return self.command.check_metadata(
            self.finder.path_from_wpt_tests(), path,
            io.BytesIO(textwrap.dedent(contents).encode()))

    def test_non_metadata_ini_skipped(self):
        errors = self._check_metadata(
            'Not all .ini files are metadata; this should not be checked',
            path='wptrunner.blink.ini')
        self.assertEqual(errors, [])

    def test_valid_test_metadata(self):
        errors = self._check_metadata(
            """\
            [variant.html?foo=bar/abc]
              bug: crbug.com/12
              expected:
                if os == "linux" and product == "chrome": [ERROR, TIMEOUT]
                if os == "linux": ERROR
                OK
            [variant.html?foo=baz]
              disabled: never completes
              expected: TIMEOUT
              [subtest 1]
                expected: TIMEOUT
              [subtest 2]
                expected: NOTRUN
            """, 'variant.html.ini')
        self.assertEqual(errors, [])

    def test_allow_reftest_error(self):
        errors = self._check_metadata(
            """\
            [reftest.html]
              # ERROR indicates a failure mode in the harness, whereas FAIL
              # indicates a product issue.
              expected: [FAIL, ERROR]
            """, 'reftest.html.ini')
        self.assertEqual(errors, [])

    def test_valid_dir_metadata(self):
        errors = self._check_metadata(
            """\
            disabled: neverfix
            """, 'dir/__dir__.ini')
        self.assertEqual(errors, [])

    def test_metadata_bad_syntax(self):
        (error, ) = self._check_metadata("""\
            [reftest.html]
              [subtest with [literal] unescaped square brackets]
            """)
        name, description, path, line = error
        self.assertEqual(name, 'META-BAD-SYNTAX')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description,
            'WPT metadata file could not be parsed: Junk before EOL u')
        # Note the 1-indexed convention.
        self.assertEqual(line, 2)

    def test_disable_one_rule(self):
        (error, ) = self._check_metadata(
            """\
            [variant.html?foo=baz]
              [subtest2]  # lint-wpt: disable=META-SINGLE-ELEM-LIST
                expected: [FAIL]
              [subtest1]
                expected: FAIL
            """, 'variant.html.ini')
        name, description, path, _ = error
        self.assertEqual(name, 'META-UNSORTED-SECTION')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description,
            'Section contains unsorted keys or subsection headings: '
            "'[subtest1]' should precede '[subtest2]'")

    def test_disable_all_rules(self):
        errors = self._check_metadata(
            """\
            # lint-wpt:  disable= *; TODO(crbug.com/1): reformat file
            [variant.html?foo=baz]
              [subtest2]
                expected: [FAIL]
              [subtest1]
                expected: FAIL
            """, 'variant.html.ini')
        self.assertEqual(errors, [])

    def test_metadata_unsorted_sections(self):
        out_of_order_subtests, out_of_order_tests = self._check_metadata(
            """\
            [variant.html?foo=baz]
              expected: PRECONDITION_FAILED
              [subtest 2]
                expected: NOTRUN
              [subtest 1]
                expected: PRECONDITION_FAILED

            [variant.html?foo=bar/abc]
              expected: PRECONDITION_FAILED
            """, 'variant.html.ini')
        name, description, path, _ = out_of_order_tests
        self.assertEqual(name, 'META-UNSORTED-SECTION')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description,
            'Section contains unsorted keys or subsection headings: '
            "'[variant.html?foo=bar/abc]' should precede "
            "'[variant.html?foo=baz]'")
        name, description, path, _ = out_of_order_subtests
        self.assertEqual(name, 'META-UNSORTED-SECTION')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description,
            'Section contains unsorted keys or subsection headings: '
            "'[subtest 1]' should precede '[subtest 2]'")

    def test_metadata_empty_sections(self):
        empty_subtest, empty_test = self._check_metadata(
            """\
            [variant.html?foo=bar/abc]
              [empty subtest]

            [variant.html?foo=baz]
            """, 'variant.html.ini')
        name, description, path, _ = empty_subtest
        self.assertEqual(name, 'META-EMPTY-SECTION')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(description,
                         "Empty section should be removed: '[empty subtest]'")
        name, description, path, _ = empty_test
        self.assertEqual(name, 'META-EMPTY-SECTION')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description,
            "Empty section should be removed: '[variant.html?foo=baz]'")

    def test_metadata_nonexistent_test(self):
        error1, error2 = self._check_metadata(
            """\
            [multiglob.https.any.html]
              expected: ERROR
            [multiglob.https.any.serviceworker.html]
              expected: ERROR
            [multiglob.https.any.sharedworker.html]
              expected: ERROR
            [multiglob.https.any.worker.html]
              expected: ERROR
            """, 'dir/multiglob.https.any.js.ini')
        name, description, path, _ = error1
        self.assertEqual(name, 'META-UNKNOWN-TEST')
        self.assertEqual(path, 'dir/multiglob.https.any.js.ini')
        self.assertEqual(
            description, 'Test ID does not exist: '
            "'dir/multiglob.https.any.serviceworker.html'")
        name, description, path, _ = error2
        self.assertEqual(name, 'META-UNKNOWN-TEST')
        self.assertEqual(path, 'dir/multiglob.https.any.js.ini')
        self.assertEqual(
            description, 'Test ID does not exist: '
            "'dir/multiglob.https.any.sharedworker.html'")

    def test_reftest_metadata_section_too_deep(self):
        (error, ) = self._check_metadata("""\
            [reftest.html]
              [subtest]
                expected: FAIL
            """)
        name, description, path, _ = error
        self.assertEqual(name, 'META-SECTION-TOO-DEEP')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description,
            "Test section '[reftest.html]' should not contain subheadings")

    def test_testharness_metadata_section_too_deep(self):
        (error, ) = self._check_metadata(
            """\
            [variant.html?foo=baz]
              [subtest]
                expected: FAIL
                [bad indentation]
                  expected: FAIL
            """, 'variant.html.ini')
        name, description, path, _ = error
        self.assertEqual(name, 'META-SECTION-TOO-DEEP')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description,
            "Subtest section '[subtest]' should not contain subheadings")

    def test_dir_metadata_section_too_deep(self):
        (error, ) = self._check_metadata(
            """\
            [multiglob.https.any.js]
              expected: ERROR
            """, 'dir/__dir__.ini')
        name, description, path, _ = error
        self.assertEqual(name, 'META-SECTION-TOO-DEEP')
        self.assertEqual(path, 'dir/__dir__.ini')
        self.assertEqual(description,
                         "Directory section should not contain subheadings")

    def test_metadata_unknown_keys(self):
        root_error, subtest_error, test_error = self._check_metadata(
            """\
            expected: OK
            [variant.html?foo=baz]
              fuzzy: 0-1;0-300
              [subtest]
                disabld: won't work, key is misspelled
            """, 'variant.html.ini')
        name, description, path, _ = root_error
        self.assertEqual(name, 'META-UNKNOWN-KEY')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(description,
                         "Root section should not have key 'expected'")
        name, description, path, _ = test_error
        self.assertEqual(name, 'META-UNKNOWN-KEY')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description, "Test section '[variant.html?foo=baz]' "
            "should not have key 'fuzzy'")
        name, description, path, _ = subtest_error
        self.assertEqual(name, 'META-UNKNOWN-KEY')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description,
            "Subtest section '[subtest]' should not have key 'disabld'")

    def test_reftest_metadata_bad_values(self):
        errors = self._check_metadata("""\
            [reftest.html]
              expected: OK
              fuzzy: [0-1:0-2, reftest-ref.html:20;200-300, @False]
              implementation-status: [implementing]
            """)
        exp_error, fuzzy_error1, fuzzy_error2, impl_error = errors
        name, description, path, _ = exp_error
        self.assertEqual(name, 'META-BAD-VALUE')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(description,
                         "Test key 'expected' has invalid value 'OK'")
        name, description, path, _ = fuzzy_error1
        self.assertEqual(name, 'META-BAD-VALUE')
        self.assertEqual(path, 'reftest.html.ini')
        # Note that the colon is a typo of what should be a semicolon.
        self.assertEqual(description,
                         "Test key 'fuzzy' has invalid value '0-1:0-2'")
        name, description, path, _ = fuzzy_error2
        self.assertEqual(name, 'META-BAD-VALUE')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(description,
                         "Test key 'fuzzy' has invalid value '@False'")
        name, description, path, _ = impl_error
        self.assertEqual(name, 'META-BAD-VALUE')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description, "Test key 'implementation-status' "
            "has invalid value '[implementing]'")

    def test_testharness_metadata_bad_values(self):
        subtest_error, bug_error, test_error = self._check_metadata(
            """\
            [variant.html?foo=baz]
              bug: crbug.com/nan
              expected:
                if os == "mac": [FAIL, CRASH]
                FAIL
              [subtest]
                expected:
                  if os == "mac": CRASH
            """, 'variant.html.ini')
        name, description, path, _ = bug_error
        self.assertEqual(name, 'META-BAD-VALUE')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(description,
                         "Test key 'bug' has invalid value 'crbug.com/nan'")
        name, description, path, _ = test_error
        self.assertEqual(name, 'META-BAD-VALUE')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(description,
                         "Test key 'expected' has invalid value 'FAIL'")
        name, description, path, _ = subtest_error
        self.assertEqual(name, 'META-BAD-VALUE')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(description,
                         "Subtest key 'expected' has invalid value 'CRASH'")

    def test_metadata_conditions_unnecessary(self):
        exp_error, fuzzy_error, restart_error = self._check_metadata("""\
            [reftest.html]
              disabled:
                # Does not partition all configurations ('linux' falls through).
                if os == "mac": flaky
                if os == "win": flaky
              expected:
                # Partition all configurations, but without using a default.
                if os == "mac": FAIL
                if os == "win": FAIL
                if os == "linux": FAIL
              fuzzy:
                # Partition all configurations using a default.
                if os == "win": [0-1;0-2, reftest-ref.html:20;200-300]
                [0-1;0-2, reftest-ref.html:20;200-300]
              restart-after:
                # Captures all configurations, but not using an unconditional
                # value.
                if os == "mac" or os != "mac": @True
            """)
        name, description, path, _ = exp_error
        self.assertEqual(name, 'META-CONDITIONS-UNNECESSARY')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(description,
                         "Test key 'expected' always has value 'FAIL'")
        name, description, path, _ = fuzzy_error
        self.assertEqual(name, 'META-CONDITIONS-UNNECESSARY')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description, "Test key 'fuzzy' always has value "
            "'[0-1;0-2, reftest-ref.html:20;200-300]'")
        name, description, path, _ = restart_error
        self.assertEqual(name, 'META-CONDITIONS-UNNECESSARY')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(description,
                         "Test key 'restart-after' always has value '@True'")

    def test_metadata_section_checks_exclusive(self):
        (error, ) = self._check_metadata(
            """\
            [variant.html?does-not-exist]
              [subtest]
                expected: FAIL
            """, 'variant.html.ini')
        # `META-SECTION-TOO-DEEP` should not be shown, since the test type
        # cannot be determined.
        name, description, path, _ = error
        self.assertEqual(name, 'META-UNKNOWN-TEST')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description,
            "Test ID does not exist: 'variant.html?does-not-exist'")

    def test_metadata_condition_checks_exclusive(self):
        always_flaky, always_ok, unreachable_value = self._check_metadata(
            """\
            [variant.html?foo=baz]
              disabled:
                if os == "win": flaky
                if os == "win": flaky
                flaky
              expected:
                if os == "win": OK
                if os == "win": OK
                OK
              [subtest]
                expected:
                  if os == "win": PASS
                  if os == "win": [FAIL, PASS]
                  FAIL
            """, 'variant.html.ini')
        # Since the author should rewrite this key unconditionally as
        # `disabled: flaky` anyway, there's no need to say that the second
        # condition is unreachable. Similar reasoning applies to skipping
        # condition errors for `META-UNNECESSARY-KEY`.
        name, description, path, _ = always_flaky
        self.assertEqual(name, 'META-CONDITIONS-UNNECESSARY')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(description,
                         "Test key 'disabled' always has value 'flaky'")
        name, description, path, _ = always_ok
        self.assertEqual(name, 'META-UNNECESSARY-KEY')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description, "Test '[variant.html?foo=baz]' key 'expected' "
            "always resolves to an implied 'OK' and should be removed")
        # `META-CONDITIONS-UNNECESSARY` should determine necessity using all
        # values, even unreachable ones, as unreachable values may become
        # reachable if fixed.
        name, description, path, _ = unreachable_value
        self.assertEqual(name, 'META-UNREACHABLE-VALUE')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description, "Subtest key 'expected' has "
            "an unused condition 'if os == \"win\"'")

    def test_metadata_unreachable_value(self):
        shadowed_narrow, always_false, unused_default = self._check_metadata(
            """\
            [reftest.html]
              expected:
                if os == "win" and os == "mac": FAIL  # Meant to say `or`.
                # This branch shadows the next one, which is narrower.
                if os == "linux": FAIL
                if os == "linux" and product == "chrome": PASS
                if os != "linux": FAIL
                FAIL
            """)
        name, description, path, _ = always_false
        self.assertEqual(name, 'META-UNREACHABLE-VALUE')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description, "Test key 'expected' has an unused condition "
            "'if (os == \"win\") and (os == \"mac\")'")
        name, description, path, _ = shadowed_narrow
        self.assertEqual(name, 'META-UNREACHABLE-VALUE')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description, "Test key 'expected' has an unused condition "
            "'if (os == \"linux\") and (product == \"chrome\")'")
        name, description, path, _ = unused_default
        self.assertEqual(name, 'META-UNREACHABLE-VALUE')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description, "Test key 'expected' has an unused default condition")

    def test_metadata_unknown_prop(self):
        os_error, product_error = self._check_metadata("""\
            [reftest.html]
              disabled:
                if oss == "win": wontfix
                if os == "mac": wontfix
              expected:
                if os != "linux": PASS
                # Should be detected even if the expression short-circuits.
                if os == "linux" or prduct == "chrome": FAIL
            """)
        name, description, path, _ = os_error
        self.assertEqual(name, 'META-UNKNOWN-PROP')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description, "Test key 'disabled' condition 'if oss == \"win\"' "
            "uses unrecognized property 'oss'")
        name, description, path, _ = product_error
        self.assertEqual(name, 'META-UNKNOWN-PROP')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description, "Test key 'expected' condition "
            "'if (os == \"linux\") or (prduct == \"chrome\")' "
            "uses unrecognized property 'prduct'")

    def test_metadata_unknown_prop_value(self):
        unrecognized_os, unrecognized_product = self._check_metadata("""\
            [reftest.html]
              # Note that both keys are reachable by some test configuration.
              disabled:
                if os == "mac" or os == "fuchsia": wontfix
              expected:
                if os == "mac" or (os == "linux" and "contentshell" != product): FAIL
            """)
        name, description, path, _ = unrecognized_os
        self.assertEqual(name, 'META-UNKNOWN-PROP-VALUE')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description, "Test key 'disabled' condition "
            "'if (os == \"mac\") or (os == \"fuchsia\")' "
            "compares 'os' against unrecognized value 'fuchsia'")
        name, description, path, _ = unrecognized_product
        self.assertEqual(name, 'META-UNKNOWN-PROP-VALUE')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description,
            "Test key 'expected' condition 'if (os == \"mac\") or "
            "((os == \"linux\") and (\"contentshell\" != product))' "
            "compares 'product' against unrecognized value 'contentshell'")

    def test_metadata_unnecessary_key(self):
        always_enabled, always_pass, always_ok = self._check_metadata(
            """\
            disabled: @False
            [variant.html?foo=baz]
              expected:
                if os == "mac": OK
              [subtest]
                expected:
                  if os == "mac": PASS
                  if os == "linux": PASS
                  if os == "win": PASS
            """, 'variant.html.ini')
        name, description, path, _ = always_enabled
        self.assertEqual(name, 'META-UNNECESSARY-KEY')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description, "Root key 'disabled' "
            "always resolves to an implied '@False' and should be removed")
        name, description, path, _ = always_ok
        self.assertEqual(name, 'META-UNNECESSARY-KEY')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description, "Test '[variant.html?foo=baz]' key 'expected' "
            "always resolves to an implied 'OK' and should be removed")
        name, description, path, _ = always_pass
        self.assertEqual(name, 'META-UNNECESSARY-KEY')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description, "Subtest '[subtest]' key 'expected' "
            "always resolves to an implied 'PASS' and should be removed")

    def test_metadata_single_element_lists(self):
        unwrap_fuzzy, unwrap_bug, unwrap_exp = self._check_metadata("""\
            fuzzy: [0;1]
            [reftest.html]
              bug: [crbug.com/123]
              expected:
                if os == "mac": [FAIL]
            """)
        name, description, path, _ = unwrap_fuzzy
        self.assertEqual(name, 'META-SINGLE-ELEM-LIST')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description, "Root key 'fuzzy' has a single-element list "
            "that should be unwrapped to '0;1'")
        name, description, path, _ = unwrap_bug
        self.assertEqual(name, 'META-SINGLE-ELEM-LIST')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description, "Test '[reftest.html]' key 'bug' has a "
            "single-element list that should be unwrapped to 'crbug.com/123'")
        name, description, path, _ = unwrap_exp
        self.assertEqual(name, 'META-SINGLE-ELEM-LIST')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description, "Test '[reftest.html]' key 'expected' has "
            "a single-element list that should be unwrapped to 'FAIL'")

    def test_metadata_long_timeout(self):
        (error, ) = self._check_metadata(
            """\
            [variant.html?foo=bar/abc]
              disabled:
                if os == "mac": slow
              expected:
                # Already disabled
                if os == "mac": TIMEOUT
                # Does not need to be disabled because the test sometimes runs OK.
                [TIMEOUT, OK]
            [variant.html?foo=baz]
              expected:
                if os == "mac": TIMEOUT
            """, 'variant.html.ini')
        name, description, path, _ = error
        self.assertEqual(name, 'META-LONG-TIMEOUT')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description, "'variant.html?foo=baz' should be disabled when "
            "it consistently times out even with 'timeout=long'")

    def test_metadata_long_timeout_already_disabled(self):
        self.fs.write_text_file(
            self.finder.path_from_wpt_tests('__dir__.ini'),
            'disabled: bulk disable of slow tests that timeout\n')
        with self._patch_builtins():
            errors = self._check_metadata("""\
                [variant.html?foo=baz]
                  expected: TIMEOUT
                """)
        self.assertEqual(errors, [])

    def test_invalid_rules_in_ignorelist(self):
        self.fs.write_text_file(
            self.finder.path_from_wpt_tests('variant.html'), '')
        self.fs.write_text_file(
            self.finder.path_from_wpt_tests('lint.ignore'),
            textwrap.dedent("""\
                DOES-NOT-EXIST: variant.html
                META-EMPTY-SECTION: variant.html
                """))
        nonexistent_error, must_fix_error = self.command.check_ignorelist(
            self.finder.path_from_wpt_tests())
        name, description, path, _ = nonexistent_error
        self.assertEqual(name, 'IGNORELIST-BAD-RULE')
        self.assertEqual(path, 'lint.ignore')
        self.assertEqual(
            description, "Rule 'DOES-NOT-EXIST' cannot be ignored "
            'or does not exist')
        name, description, path, _ = must_fix_error
        self.assertEqual(name, 'IGNORELIST-BAD-RULE')
        self.assertEqual(path, 'lint.ignore')
        self.assertEqual(
            description, "Rule 'META-EMPTY-SECTION' cannot be ignored "
            'or does not exist')

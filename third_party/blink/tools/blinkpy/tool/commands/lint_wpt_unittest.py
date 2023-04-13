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
from blinkpy.tool.mock_tool import MockBlinkTool
from blinkpy.tool.commands.lint_wpt import LintError, LintWPT
from blinkpy.common.system.log_testing import LoggingTestCase

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
        self.command = LintWPT(self.tool, set(map(metadata.RunInfo, configs)))
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
                            ['variant.html?foo=bar/abc', {}],
                            ['variant.html?foo=baz', {}],
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

    def test_execute_basic(self):
        path = self.finder.path_from_wpt_tests('bad_python.py')
        self.fs.write_text_file(path, 'invalid syntax should be detected')
        with self._patch_builtins():
            exit_code = self.command.main([path])
        self.assertNotEqual(exit_code, 0)
        self.assertIn(
            'ERROR: bad_python.py:1: Unable to parse file (PARSE-FAILED)\n',
            self.logMessages())
        self.assertIn('INFO: There was 1 error (PARSE-FAILED: 1)\n',
                      self.logMessages())

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

    def test_metadata_unsorted_sections(self):
        out_of_order_subtests, out_of_order_tests = self._check_metadata(
            """\
            [variant.html?foo=baz]
              expected: TIMEOUT
              [subtest 2]
                expected: NOTRUN
              [subtest 1]
                expected: TIMEOUT

            [variant.html?foo=bar/abc]
              expected: TIMEOUT
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
                         "Empty section can be removed: '[empty subtest]'")
        name, description, path, _ = empty_test
        self.assertEqual(name, 'META-EMPTY-SECTION')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description,
            "Empty section can be removed: '[variant.html?foo=baz]'")

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
              expected: ERROR
              fuzzy: [0-1:0-2, reftest-ref.html:20;200-300, @False]
              implementation-status: [implementing]
            """)
        exp_error, fuzzy_error1, fuzzy_error2, impl_error = errors
        name, description, path, _ = exp_error
        self.assertEqual(name, 'META-BAD-VALUE')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(description,
                         "Test key 'expected' has invalid value 'ERROR'")
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

    def test_metadata_condition_checks_exclusive(self):
        conds_unnecessary, unreachable_value = self._check_metadata("""\
            [reftest.html]
              disabled:
                if os == "win": flaky
                if os == "win": flaky
                flaky
              expected:
                if os == "win": FAIL
                if os == "win": [FAIL, PASS]
                FAIL
            """)
        # Since the author should rewrite this key unconditionally as
        # `disabled: flaky` anyway, there's no need to say that the second
        # condition is unreachable.
        name, description, path, _ = conds_unnecessary
        self.assertEqual(name, 'META-CONDITIONS-UNNECESSARY')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(description,
                         "Test key 'disabled' always has value 'flaky'")
        # `META-CONDITIONS-UNNECESSARY` should determine necessity using all
        # values, even unreachable ones, as unreachable values may become
        # reachable if fixed.
        name, description, path, _ = unreachable_value
        self.assertEqual(name, 'META-UNREACHABLE-VALUE')
        self.assertEqual(path, 'reftest.html.ini')
        self.assertEqual(
            description,
            "Test key 'expected' has an unused condition 'if os == \"win\"'")

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

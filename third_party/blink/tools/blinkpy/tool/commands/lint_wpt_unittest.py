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


class LintWPTTest(LoggingTestCase):
    def setUp(self):
        super().setUp()
        self.maxDiff = None
        self.tool = MockBlinkTool()
        self.fs = self.tool.filesystem
        self.finder = path_finder.PathFinder(self.fs)
        self.command = LintWPT(self.tool)
        self.fs.write_text_file(self.finder.path_from_wpt_tests('lint.ignore'),
                                '')
        self.fs.write_text_file(
            self.finder.path_from_wpt_tests('MANIFEST.json'),
            json.dumps({
                'version': 8,
                'items': {
                    'testharness': {
                        'test.html': [
                            'd933fd981d4a33ba82fb2b000234859bdda1494e',
                            [None, {}],
                        ],
                        'multiglob.https.any.js': [
                            'd6498c3e388e0c637830fa080cca78b0ab0e5305',
                            ['dir/multiglob.https.any.html', {}],
                            ['dir/multiglob.https.any.worker.html', {}],
                        ],
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
                        path: str = 'test.html.ini') -> List[LintError]:
        return self.command.check_metadata(
            self.finder.path_from_wpt_tests(), path,
            io.BytesIO(textwrap.dedent(contents).encode()))

    def test_non_metadata_ini_skipped(self):
        errors = self._check_metadata(
            'Not all .ini files are metadata; this should not be checked',
            path='wptrunner.blink.ini')
        self.assertEqual(errors, [])

    def test_metadata_valid(self):
        errors = self._check_metadata(
            """\
            [variant.html?foo=bar/abc]
              expected: FAIL
            [variant.html?foo=baz]
              disabled: never completes
              expected: TIMEOUT
              [subtest 1]
                expected: TIMEOUT
              [subtest 2]
                expected: NOTRUN
            """, 'variant.html.ini')
        self.assertEqual(errors, [])

    def test_metadata_bad_syntax(self):
        (error, ) = self._check_metadata("""\
            [test.html]
              [subtest with [literal] unescaped square brackets]
            """)
        name, description, path, line = error
        self.assertEqual(name, 'META-BAD-SYNTAX')
        self.assertEqual(path, 'test.html.ini')
        self.assertEqual(
            description,
            'WPT metadata file could not be parsed: Junk before EOL u')
        # Note the 1-indexed convention.
        self.assertEqual(line, 2)

    def test_metadata_unsorted_sections(self):
        out_of_order_tests, out_of_order_subtests = self._check_metadata(
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
        name, description, path, line = out_of_order_tests
        self.assertEqual(name, 'META-UNSORTED-SECTION')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description,
            'Section contains unsorted keys or subsection headings: '
            "'[variant.html?foo=bar/abc]' should precede "
            "'[variant.html?foo=baz]'")
        name, description, path, line = out_of_order_subtests
        self.assertEqual(name, 'META-UNSORTED-SECTION')
        self.assertEqual(path, 'variant.html.ini')
        self.assertEqual(
            description,
            'Section contains unsorted keys or subsection headings: '
            "'[subtest 1]' should precede '[subtest 2]'")

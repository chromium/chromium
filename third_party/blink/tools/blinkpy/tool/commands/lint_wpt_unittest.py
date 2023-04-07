# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import subprocess
from unittest import mock

from blinkpy.common import path_finder
from blinkpy.tool.mock_tool import MockBlinkTool
from blinkpy.tool.commands.lint_wpt import LintWPT
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

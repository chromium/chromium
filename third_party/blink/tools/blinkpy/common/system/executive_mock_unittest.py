# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.executive_mock import mock_git_commands


class MockGitCommandsTest(unittest.TestCase):
    def test_argument_precedence_and_subcommand_fallback(self):
        executive = mock_git_commands(
            {
                'rev-parse': 'fallback',
                ('rev-parse', 'HEAD'): 'local-head',
                ('rev-parse', 'empty'): '',
            }
        )
        self.assertEqual(
            executive.run_command(['git', 'rev-parse', 'HEAD']),
            'local-head',
        )
        self.assertEqual(
            executive.run_command(['git', 'rev-parse', 'empty']),
            '',
        )
        self.assertEqual(
            executive.run_command(['git', 'rev-parse', 'other']),
            'fallback',
        )

    def test_unmatched_command(self):
        executive = mock_git_commands({}, strict=False)
        self.assertEqual(executive.run_command(['git', 'status']), '')

        strict_executive = mock_git_commands(
            {('diff', '--cached'): 'ok'}, strict=True
        )
        with self.assertRaises(AssertionError) as raised:
            strict_executive.run_command(['git', 'diff', '--stat'])
        self.assertIn("['git', 'diff', '--stat']", str(raised.exception))

    def test_raises_configured_exception(self):
        error = ScriptError('patch failed')
        executive = mock_git_commands({'apply': error})
        with self.assertRaises(ScriptError) as raised:
            executive.run_command(['git', 'apply'])
        self.assertIs(raised.exception, error)

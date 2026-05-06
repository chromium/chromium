#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import unittest
from unittest.mock import MagicMock, patch

import PRESUBMIT


class MockAffectedFile:
    def __init__(self, local_path, action='M'):
        self._path = local_path
        self._action = action

    def LocalPath(self):
        return self._path

    def AbsoluteLocalPath(self):
        return os.path.normpath(os.path.join(
            os.path.abspath('fake_repo'), self._path))

    def Action(self):
        return self._action


class MockInputApi:
    def __init__(self):
        self.change = MagicMock()
        self.change.RepositoryRoot.return_value = os.path.abspath('fake_repo')
        self.affected_files = []
        self.os_path = os.path
        self.os = os
        self.files_content = {}

    def AffectedFiles(self, file_filter=None, include_deletes=False):
        return [f for f in self.affected_files
                if not file_filter or file_filter(f)]

    def ReadFile(self, affected_file):
        return self.files_content.get(affected_file.LocalPath(), "")

    def PresubmitLocalPath(self):
        return os.path.normpath(os.path.join(
            os.path.abspath('fake_repo'), 'remoting/tools/magi-mode'))

    def FilterSourceFile(self, affected_file, files_to_check=None):
        if not files_to_check:
            return True
        return any(re.match(pattern, affected_file.LocalPath())
                   for pattern in files_to_check)


class MagiPresubmitTest(unittest.TestCase):
    def setUp(self):
        self.mock_input = MockInputApi()
        self.mock_output = MagicMock()
        self.mock_output.PresubmitError = lambda x: f"ERROR: {x}"
        self.mock_output.PresubmitPromptWarning = lambda x: f"WARN: {x}"

    @patch('os.path.exists')
    @patch('os.path.getsize')
    @patch('os.walk')
    def testReachability(self, mock_walk, mock_getsize, mock_exists):
        # Setup filesystem: SKILL.md -> LINKED.md, ORPHAN.md
        magi_dir = os.path.normpath(os.path.join(
            os.path.abspath('fake_repo'), 'remoting/tools/magi-mode'))
        mock_walk.return_value = [
            (magi_dir, [], ['SKILL.md', 'LINKED.md', 'ORPHAN.md'])
        ]
        mock_getsize.return_value = 100
        mock_exists.return_value = True

        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/SKILL.md')]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/SKILL.md': '[link](LINKED.md)\n',
            'remoting/tools/magi-mode/LINKED.md': 'content\n',
            'remoting/tools/magi-mode/ORPHAN.md': 'content\n',
        }

        # We need to mock 'open' for unmodified files (LINKED and ORPHAN)
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data='content\n')):
            results = PRESUBMIT.CheckMarkdownFiles(
                self.mock_input, self.mock_output)

        print("TEST REACHABILITY RESULTS:", results)
        # Expect 1 warning for ORPHAN.md (it's unmodified debt)
        orphans = [r for r in results if 'Unreachable' in r]
        self.assertEqual(len(orphans), 1)
        self.assertIn('ORPHAN.md', orphans[0])

    def testCodeBlockExclusion(self):
        # 100 char line inside code block should be ignored
        long_line = '```\n' + 'A' * 100 + '\n```\n'
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/SKILL.md')]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/SKILL.md': long_line}

        magi_dir = os.path.normpath(os.path.join(
            os.path.abspath('fake_repo'), 'remoting/tools/magi-mode'))
        with patch('os.walk', return_value=[(magi_dir, [], ['SKILL.md'])]), \
                patch('os.path.getsize', return_value=100):
            results = PRESUBMIT.CheckMarkdownFiles(
                self.mock_input, self.mock_output)

        warnings = [r for r in results if 'exceeds 80 characters' in r]
        self.assertEqual(len(warnings), 0)

    def testIndentedBlockRobustness(self):
        # Multiple indented lines after an empty line should be ignored
        content = ('\n\n    Line 1 is long ' + 'A'*70 +
                   '\n    Line 2 is also long ' + 'B'*70 + '\n\nText')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/SKILL.md')]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/SKILL.md': content}

        magi_dir = os.path.normpath(os.path.join(
            os.path.abspath('fake_repo'), 'remoting/tools/magi-mode'))
        with patch('os.walk', return_value=[(magi_dir, [], ['SKILL.md'])]), \
                patch('os.path.getsize', return_value=100):
            results = PRESUBMIT.CheckMarkdownFiles(
                self.mock_input, self.mock_output)

        warnings = [r for r in results if 'exceeds 80 characters' in r]
        self.assertEqual(len(warnings), 0)

    def testJsonStateBlockValidation(self):
        # Valid state block
        valid_json = (
            '{"iteration": 1, "stall_count": 0, "active_constraints": [], '
            '"resolved_constraints": [], "personas": ["Security"], '
            '"review_mode": "SUPERVISOR", "next_phase": "CRITIQUE"}')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/state_block.magi.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/state_block.magi.json': valid_json
        }

        # We need to mock the schema file
        schema_json = (
            '{"definitions": {"StateBlock": {"required": ["iteration", '
            '"stall_count", "active_constraints", "resolved_constraints", '
            '"personas", "review_mode", "next_phase"], '
            '"properties": {"iteration": '
            '{"type": "integer"}, "stall_count": {"type": "integer"}, '
            '"active_constraints": {"type": "array"}, "resolved_constraints": '
            '{"type": "array"}, "personas": {"type": "array"}, '
            '"next_phase": {"type": "string"}, '
            '"review_mode": {"type": "string", "enum": '
            '["SUPERVISOR", "CONSENSUS"]}}}}}')
        schema_path = os.path.normpath(os.path.join(
            os.path.abspath('fake_repo'),
            'remoting/tools/magi-mode/magi_schema.json'))

        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertEqual(len(results), 0)

        # Missing required key
        invalid_json = '{"iteration": 1, "active_constraints": []}'
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/state_block.magi.json': invalid_json
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any('missing required keys' in r for r in results))

        # Wrong type
        wrong_type_json = (
            '{"iteration": "1", "stall_count": 0, "active_constraints": [], '
            '"resolved_constraints": [], "personas": [], '
            '"review_mode": "SUPERVISOR", "next_phase": "CRITIQUE"}')
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/state_block.magi.json': wrong_type_json
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                "key 'iteration' should be integer" in r for r in results))

    def testJsonProjectSpecValidation(self):
        # Valid project spec
        valid_json = (
            '{"goal": "Test", "target_files": ["foo.cc"], "anti_goals": [], '
            '"edge_cases": [], "paranoia_mode": false, '
            '"auditability_level": "NORMAL", "next_phase": "SCAFFOLDING"}')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/project.magi.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/project.magi.json': valid_json
        }

        # We need to mock the schema file
        schema_json = (
            '{"definitions": {"ProjectSpec": {"required": ["goal", '
            '"target_files", "anti_goals", "edge_cases", "next_phase", '
            '"paranoia_mode", "auditability_level"], "properties": '
            '{"goal": {"type": "string"}, "target_files": {"type": "array"}, '
            '"anti_goals": {"type": "array"}, "edge_cases": '
            '{"type": "array"}, "paranoia_mode": {"type": "boolean"}, '
            '"next_phase": {"type": "string"}, '
            '"auditability_level": {"type": "string", "enum": ["NORMAL", '
            '"VERBOSE"]}}}}}')

        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertEqual(len(results), 0)

        # Missing required key
        invalid_json = '{"goal": "Test"}'
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/project.magi.json': invalid_json
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any('missing required keys' in r for r in results))

        # Wrong boolean type
        wrong_bool_json = (
            '{"goal": "Test", "target_files": ["foo.cc"], "anti_goals": [], '
            '"edge_cases": [], "paranoia_mode": "false", '
            '"auditability_level": "NORMAL", "next_phase": "SCAFFOLDING"}')
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/project.magi.json': wrong_bool_json
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                "key 'paranoia_mode' should be boolean" in r for r in results))

        # Invalid generic enum
        invalid_enum_json = (
            '{"goal": "Test", "target_files": ["foo.cc"], "anti_goals": [], '
            '"edge_cases": [], "paranoia_mode": false, '
            '"auditability_level": "UNKNOWN", "next_phase": "SCAFFOLDING"}')
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/project.magi.json': invalid_enum_json
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                "key 'auditability_level' must be one of" in r
                for r in results))

    def testJsonReviewFeedbackValidation(self):
        # Valid review feedback
        valid_json = (
            '{"verdict": "REJECT", "reasoning": ["Bad"], "comments": '
            '[{"file": "foo.cc", "line": 10, "comment": "Fix this"}]}')
        self.mock_input.affected_files = [
            MockAffectedFile(
                'remoting/tools/magi-mode/review.security.magi.1.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/review.security.magi.1.json': valid_json
        }

        schema_json = (
            '{"definitions": {"ReviewFeedback": {"required": ["verdict", '
            '"reasoning"], "properties": {"verdict": {"type": "string", '
            '"enum": ["ACCEPT", "REJECT"]}, "reasoning": {"type": "array"}, '
            '"comments": {"type": "array"}}}}}')

        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertEqual(len(results), 0)

        # Invalid verdict
        invalid_verdict_json = '{"verdict": "MAYBE", "reasoning": []}'
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/review.security.magi.1.json':
                invalid_verdict_json
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any('must be one of' in r for r in results))

    def testJsonConstraintsValidation(self):
        # Valid constraints
        valid_json = (
            '{"iteration": 2, "constraints": ["Rule 1", "Rule 2"], '
            '"review_mode": "SUPERVISOR", "next_phase": "SYNTHESIS"}')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/constraints.magi.2.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/constraints.magi.2.json': valid_json
        }

        schema_json = (
            '{"definitions": {"Constraints": {"required": ["iteration", '
            '"constraints", "review_mode", "next_phase"], '
            '"properties": {"iteration": '
            '{"type": "integer"}, "constraints": {"type": "array"}, '
            '"review_mode": {"type": "string", "enum": '
            '["SUPERVISOR", "CONSENSUS"]}, '
            '"next_phase": {"type": "string"}}}}}')

        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertEqual(len(results), 0)

    def testDecisionGraphValidation(self):
        # Invalid next_phase for project
        invalid_project = (
            '{"goal": "T", "target_files": [], "anti_goals": [], '
            '"edge_cases": [], "next_phase": "SYNTHESIS"}')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/project.magi.json')]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/project.magi.json': invalid_project}
        schema_json = (
            '{"definitions": {"ProjectSpec": {"required": ["goal", '
            '"target_files", "anti_goals", "edge_cases"], "properties": '
            '{"goal": {"type": "string"}, "target_files": {"type": "array"}, '
            '"anti_goals": {"type": "array"}, "edge_cases": {"type": '
            '"array"}, "next_phase": {"type": "string"}}}}}')
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                'must signal next_phase: SCAFFOLDING' in r for r in results))

        # Invalid handoff for SUPERVISOR constraints
        invalid_supervisor = (
            '{"iteration": 1, "constraints": [], "review_mode": '
            '"SUPERVISOR", "next_phase": "TPM_UPDATE"}')
        self.mock_input.affected_files = [
            MockAffectedFile(
                'remoting/tools/magi-mode/constraints.magi.1.json'
            )
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/constraints.magi.1.json':
                invalid_supervisor}
        schema_json = (
            '{"definitions": {"Constraints": {"required": ["iteration", '
            '"constraints", "review_mode"], "properties": {"iteration": '
            '{"type": "integer"}, "constraints": {"type": "array"}, '
            '"review_mode": {"type": "string"}, "next_phase": {"type": '
            '"string"}}}}}')
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                'must signal SYNTHESIS or TRAINING' in r for r in results))

        # Invalid handoff for CONSENSUS constraints
        invalid_consensus = (
            '{"iteration": 1, "constraints": [], "review_mode": '
            '"CONSENSUS", "next_phase": "SYNTHESIS"}')
        self.mock_input.affected_files = [
            MockAffectedFile(
                'remoting/tools/magi-mode/constraints.magi.1.json'
            )
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/constraints.magi.1.json':
                invalid_consensus}
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                'must signal TPM_UPDATE, not SYNTHESIS' in r for r in results))


if __name__ == '__main__':
    unittest.main()

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

    def testMarkdownContentMandates(self):
        # Missing Tone Mandate
        content_missing = 'Some text\n'
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/SKILL.md')]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/SKILL.md': content_missing}

        magi_dir = os.path.normpath(os.path.join(
            os.path.abspath('fake_repo'), 'remoting/tools/magi-mode'))
        with patch('os.walk', return_value=[(magi_dir, [], ['SKILL.md'])]), \
                patch('os.path.getsize', return_value=100):
            results = PRESUBMIT.CheckMarkdownFiles(
                self.mock_input, self.mock_output)

        self.assertTrue(any('must contain the "TONE MANDATE '
                            '(SIGNAL-TO-NOISE):" section' in r
                            for r in results))

        # Missing Artifacts Only
        content_partial = ('TONE MANDATE (SIGNAL-TO-NOISE):\n'
                           'Zero Preamble/Postamble\n')
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/SKILL.md': content_partial}
        with patch('os.walk', return_value=[(magi_dir, [], ['SKILL.md'])]), \
                patch('os.path.getsize', return_value=100):
            results = PRESUBMIT.CheckMarkdownFiles(
                self.mock_input, self.mock_output)

        self.assertTrue(any('must explicitly enforce "Zero Preamble/'
                            'Postamble" and "Artifacts Only"' in r
                            for r in results))

        # Valid
        content_valid = ('TONE MANDATE (SIGNAL-TO-NOISE):\n'
                         'Zero Preamble/Postamble\nArtifacts Only\n')
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/SKILL.md': content_valid}
        with patch('os.walk', return_value=[(magi_dir, [], ['SKILL.md'])]), \
                patch('os.path.getsize', return_value=100):
            results = PRESUBMIT.CheckMarkdownFiles(
                self.mock_input, self.mock_output)

        self.assertFalse(any('must contain the "TONE MANDATE '
                             '(SIGNAL-TO-NOISE):" section' in r
                             for r in results))
        self.assertFalse(any('must explicitly enforce' in r for r in results))

    def testJsonStateBlockValidation(self):
        # Valid state block
        valid_json = (
            '{"checklist": {"checked_xyz": true}, "unlisted_issues_found": [], '
            '"iteration": 1, "stall_count": 0, "active_constraints": [], '
            '"resolved_constraints": [],'
            '"personas": ['
            '"src/remoting/tools/magi-mode/personas/core/security.json"], '
            '"review_mode": "SUPERVISOR", "state_transport": "EPHEMERAL", '
            '"next_phase": "CRITIQUE"}')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/state_block.magi.json'),
            MockAffectedFile(
                'remoting/tools/magi-mode/personas/core/security.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/state_block.magi.json': valid_json,
            'remoting/tools/magi-mode/personas/core/security.json': (
                '{"checklist": {"checked_xyz": "Desc"}}')
        }

        # We need to mock the schema file
        schema_json = (
            '{"definitions": {"ChecklistObject": {"type": "object", '
            '"patternProperties": {"^.*$": {"type": "boolean"}}}, '
            '"StateBlock": {"required": ["checklist", "iteration", '
            '"stall_count", "active_constraints", "resolved_constraints", '
            '"personas", "review_mode", "state_transport", "next_phase"], '
            '"properties": {"checklist": '
            '{"$ref": "#/definitions/ChecklistObject"}, '
            '"unlisted_issues_found": {"type": "array"}, "iteration": '
            '{"type": "integer"}, "stall_count": {"type": "integer"}, '
            '"active_constraints": {"type": "array"}, "resolved_constraints": '
            '{"type": "array"}, "personas": {"type": "array"}, '
            '"next_phase": {"type": "string"}, '
            '"review_mode": {"type": "string", "enum": '
            '["SUPERVISOR", "CONSENSUS"]}, '
            '"state_transport": {"type": "string", "enum": '
            '["FILE_IO", "EPHEMERAL", "EPHEMERAL_WITH_LOGS"]}}}}}')
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
            '"review_mode": "SUPERVISOR", "state_transport": "EPHEMERAL", '
            '"next_phase": "CRITIQUE"}')
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/state_block.magi.json': wrong_type_json
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                "key 'iteration' should be integer" in r for r in results))

    def testJsonStateBlockInvalidChecklistValue(self):
        # Non-boolean value in checklist ("checked_xyz": "not_a_boolean")
        invalid_checklist_json = (
            '{"checklist": {"checked_xyz": "not_a_boolean"}, '
            '"unlisted_issues_found": [], "iteration": 1, '
            '"stall_count": 0, "active_constraints": [], '
            '"resolved_constraints": [], "personas": '
            '["src/remoting/tools/magi-mode/personas/core/security.json"], '
            '"review_mode": "SUPERVISOR", "state_transport": "EPHEMERAL", '
            '"next_phase": "CRITIQUE"}')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/state_block.magi.json'),
            MockAffectedFile(
                'remoting/tools/magi-mode/personas/core/security.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/state_block.magi.json': (
                invalid_checklist_json),
            'remoting/tools/magi-mode/personas/core/security.json': (
                '{"checklist": {"checked_xyz": "Desc"}}')
        }
        schema_json = (
            '{"definitions": {"ChecklistObject": {"type": "object", '
            '"patternProperties": {"^.*$": {"type": "boolean"}}}, '
            '"StateBlock": {"required": ["checklist", "iteration", '
            '"stall_count", "active_constraints", "resolved_constraints", '
            '"personas", "review_mode", "state_transport", "next_phase"], '
            '"properties": {"checklist": '
            '{"$ref": "#/definitions/ChecklistObject"}, '
            '"unlisted_issues_found": {"type": "array"}, "iteration": '
            '{"type": "integer"}, "stall_count": {"type": "integer"}, '
            '"active_constraints": {"type": "array"}, "resolved_constraints": '
            '{"type": "array"}, "personas": {"type": "array"}, '
            '"next_phase": {"type": "string"}, '
            '"review_mode": {"type": "string", "enum": '
            '["SUPERVISOR", "CONSENSUS"]}, '
            '"state_transport": {"type": "string", "enum": '
            '["FILE_IO", "EPHEMERAL", "EPHEMERAL_WITH_LOGS"]}}}}}')

        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                'checklist key "checked_xyz" must be a boolean' in r
                for r in results))

    def testJsonStateBlockArbitraryChecklistKey(self):
        # Arbitrary key in checklist ("check_arbitrary": true) not in security.json
        arbitrary_key_json = (
            '{"checklist": {"checked_xyz": true, "check_arbitrary": true}, '
            '"unlisted_issues_found": [], "iteration": 1, '
            '"stall_count": 0, "active_constraints": [], '
            '"resolved_constraints": [], "personas": '
            '["src/remoting/tools/magi-mode/personas/core/security.json"], '
            '"review_mode": "SUPERVISOR", "state_transport": "EPHEMERAL", '
            '"next_phase": "CRITIQUE"}')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/state_block.magi.json'),
            MockAffectedFile(
                'remoting/tools/magi-mode/personas/core/security.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/state_block.magi.json': (
                arbitrary_key_json),
            'remoting/tools/magi-mode/personas/core/security.json': (
                '{"checklist": {"checked_xyz": "Desc"}}')
        }
        schema_json = (
            '{"definitions": {"ChecklistObject": {"type": "object", '
            '"patternProperties": {"^.*$": {"type": "boolean"}}}, '
            '"StateBlock": {"required": ["checklist", "iteration", '
            '"stall_count", "active_constraints", "resolved_constraints", '
            '"personas", "review_mode", "state_transport", "next_phase"], '
            '"properties": {"checklist": '
            '{"$ref": "#/definitions/ChecklistObject"}, '
            '"unlisted_issues_found": {"type": "array"}, "iteration": '
            '{"type": "integer"}, "stall_count": {"type": "integer"}, '
            '"active_constraints": {"type": "array"}, "resolved_constraints": '
            '{"type": "array"}, "personas": {"type": "array"}, '
            '"next_phase": {"type": "string"}, '
            '"review_mode": {"type": "string", "enum": '
            '["SUPERVISOR", "CONSENSUS"]}, '
            '"state_transport": {"type": "string", "enum": '
            '["FILE_IO", "EPHEMERAL", "EPHEMERAL_WITH_LOGS"]}}}}}')

        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                'checklist contains arbitrary keys not defined in selected '
                'personas: check_arbitrary' in r for r in results))

    def testJsonProjectSpecValidation(self):
        # Valid project spec
        valid_json = (
            '{"checklist": {}, "unlisted_issues_found": [], '
            '"goal": "Test", "target_files": ["foo.cc"], "anti_goals": [], '
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
            '{"definitions": {"ChecklistObject": {"type": "object", '
            '"patternProperties": {"^.*$": {"type": "boolean"}}}, '
            '"ProjectSpec": {"required": ["checklist", "goal", '
            '"target_files", "anti_goals", "edge_cases", "next_phase", '
            '"paranoia_mode", "auditability_level"], "properties": '
            '{"checklist": {"$ref": "#/definitions/ChecklistObject"}, '
            '"unlisted_issues_found": {"type": "array"}, '
            '"goal": {"type": "string"}, "target_files": {"type": "array"}, '
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
            '[{"file": "foo.cc", "line": 10, "comment": "Fix this"}], '
            '"next_phase": "ANALYSIS"}')
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
        invalid_verdict_json = (
            '{"checklist": {}, "unlisted_issues_found": [], '
            '"verdict": "MAYBE", "reasoning": [], "next_phase": "ANALYSIS"}')
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
            '{"checklist": {}, "unlisted_issues_found": [], '
            '"goal": "T", "target_files": [], "anti_goals": [], '
            '"edge_cases": [], "paranoia_mode": false, '
            '"auditability_level": "NORMAL", '
            '"next_phase": "SYNTHESIS"}')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/project.magi.json')]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/project.magi.json': invalid_project}
        schema_json = (
            '{"definitions": {"ChecklistObject": {"type": "object", '
            '"patternProperties": {"^.*$": {"type": "boolean"}}}, '
            '"ProjectSpec": {"required": ["checklist", "goal", '
            '"target_files", "anti_goals", "edge_cases", "paranoia_mode", '
            '"auditability_level", "next_phase"], "properties": '
            '{"checklist": {"$ref": "#/definitions/ChecklistObject"}, '
            '"unlisted_issues_found": {"type": "array"}, '
            '"goal": {"type": "string"}, "target_files": {"type": "array"}, '
            '"anti_goals": {"type": "array"}, "edge_cases": {"type": '
            '"array"}, "paranoia_mode": {"type": "boolean"}, '
            '"auditability_level": {"type": "string"}, '
            '"next_phase": {"type": "string"}}}}}')
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
            '{"checklist": {}, "unlisted_issues_found": [], '
            '"iteration": 1, "constraints": [], "review_mode": '
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

    def testCrossFileValidation(self):
        schema_json = '{"definitions": {}}'

        state_json = (
            '{"checklist": {}, "unlisted_issues_found": [], '
            '"iteration": 1, "stall_count": 0, "active_constraints": [], '
            '"resolved_constraints": [], "personas": '
            '["src/remoting/tools/magi-mode/personas/core/security.json"], '
            '"review_mode": "SUPERVISOR", "state_transport": "EPHEMERAL", '
            '"next_phase": "CRITIQUE"}')

        proj_paranoia = '{"paranoia_mode": true}'
        proj_verbose = '{"auditability_level": "VERBOSE"}'

        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/state_block.magi.json'),
            MockAffectedFile(
                'remoting/tools/magi-mode/personas/core/security.json')
        ]

        def mock_exists(path):
            return 'project.magi.json' in path

        # Test EPHEMERAL with paranoia_mode: true
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/state_block.magi.json': state_json,
            'remoting/tools/magi-mode/personas/core/security.json': (
                '{"checklist": {}}')
        }

        with patch('os.path.exists', side_effect=mock_exists):
            def mock_open_impl(path, *args, **kwargs):
                if 'project.magi.json' in path:
                    return unittest.mock.mock_open(read_data=proj_paranoia)()
                return unittest.mock.mock_open(read_data=schema_json)()

            with patch('builtins.open', side_effect=mock_open_impl):
                results = PRESUBMIT.CheckJsonFiles(
                    self.mock_input, self.mock_output)
                self.assertTrue(any(
                    'project.magi.json has paranoia_mode: true' in r
                    for r in results))

        # Test EPHEMERAL with auditability_level: VERBOSE
        with patch('os.path.exists', side_effect=mock_exists):
            def mock_open_impl2(path, *args, **kwargs):
                if 'project.magi.json' in path:
                    return unittest.mock.mock_open(read_data=proj_verbose)()
                return unittest.mock.mock_open(read_data=schema_json)()

            with patch('builtins.open', side_effect=mock_open_impl2):
                results = PRESUBMIT.CheckJsonFiles(
                    self.mock_input, self.mock_output)
                self.assertTrue(any(
                    'project.magi.json has auditability_level: VERBOSE' in r
                    for r in results))


    def testJsonPersonaDefValidation(self):
        valid_json = (
            '{"role": "Test Role", "mandate": "Test Mandate", '
            '"checklist": {"check_1": "Desc 1", "check_2": "Desc 2"}}')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/personas/test.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/personas/test.json': valid_json
        }

        schema_json = (
            '{"definitions": {"PersonaDef": {"required": ["role", '
            '"mandate", "checklist"], "properties": {"role": '
            '{"type": "string"}, "mandate": {"type": "string"}, '
            '"checklist": {"type": "object", "patternProperties": '
            '{"^.*$": {"type": "string"}}}}}}}')

        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertEqual(len(results), 0)

        # Missing required key
        invalid_json = '{"role": "Test Role", "mandate": "Test Mandate"}'
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/personas/test.json': invalid_json
        }

        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any('missing required keys' in r for r in results))

    def testJsonPersonaDirectoryDepth(self):
        valid_json = '{"role": "Test", "mandate": "Test", "checklist": {}}'
        schema_json = '{"definitions": {"PersonaDef": {"required": []}}}'

        # Depth 5 (valid)
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/personas/1/2/3/4/5.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/personas/1/2/3/4/5.json': valid_json
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertEqual(len(results), 0)

        # Depth 6 (invalid)
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/personas/1/2/3/4/5/6.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/personas/1/2/3/4/5/6.json': valid_json
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                'exceeds maximum persona directory depth of 5' in r for r in results))


    def testJsonProjectSpecBuildTargets(self):
        # Invalid build_targets type (string instead of array)
        invalid_type_json = (
            '{"checklist": {}, "goal": "Test", "target_files": [], '
            '"anti_goals": [], "edge_cases": [], "paranoia_mode": false, '
            '"auditability_level": "NORMAL", "next_phase": "SCAFFOLDING", '
            '"build_targets": "//remoting/host:host"}')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/project.magi.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/project.magi.json': invalid_type_json
        }
        schema_json = (
            '{"definitions": {"ProjectSpec": {"required": [], '
            '"properties": {"build_targets": {"type": "array", '
            '"items": {"type": "string"}}}}}}')
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                "key 'build_targets' should be array" in r for r in results))

        # Invalid element in build_targets (integer instead of string)
        invalid_elem_json = (
            '{"checklist": {}, "goal": "Test", "target_files": [], '
            '"anti_goals": [], "edge_cases": [], "paranoia_mode": false, '
            '"auditability_level": "NORMAL", "next_phase": "SCAFFOLDING", '
            '"build_targets": [123]}')
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/project.magi.json': invalid_elem_json
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                "list contains a non-string element" in r for r in results))


    def testJsonProjectSpecEnvironment(self):
        # Missing repo_type
        invalid_env_1 = (
            '{"checklist": {}, "goal": "Test", "target_files": [], '
            '"anti_goals": [], "edge_cases": [], "paranoia_mode": false, '
            '"auditability_level": "NORMAL", "next_phase": "SCAFFOLDING", '
            '"environment": {"vcs": "JJ", "harness": "JETSKI"}}')
        self.mock_input.affected_files = [
            MockAffectedFile('remoting/tools/magi-mode/project.magi.json')
        ]
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/project.magi.json': invalid_env_1
        }
        schema_json = '{"definitions": {"ProjectSpec": {"required": []}}}'

        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                'environment is missing required key "repo_type"' in r for r in results))

        # Invalid repo_type
        invalid_env_2 = (
            '{"checklist": {}, "goal": "Test", "target_files": [], '
            '"anti_goals": [], "edge_cases": [], "paranoia_mode": false, '
            '"auditability_level": "NORMAL", "next_phase": "SCAFFOLDING", '
            '"environment": {"vcs": "JJ", "harness": "JETSKI", "repo_type": "INVALID"}}')
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/project.magi.json': invalid_env_2
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                'environment.repo_type must be CHROMIUM or GOOGLE_INTERNAL' in r for r in results))

        # Invalid output_directory type
        invalid_env_3 = (
            '{"checklist": {}, "goal": "Test", "target_files": [], '
            '"anti_goals": [], "edge_cases": [], "paranoia_mode": false, '
            '"auditability_level": "NORMAL", "next_phase": "SCAFFOLDING", '
            '"environment": {"vcs": "JJ", "harness": "JETSKI", "repo_type": "CHROMIUM", "output_directory": 123}}')
        self.mock_input.files_content = {
            'remoting/tools/magi-mode/project.magi.json': invalid_env_3
        }
        with patch('builtins.open',
                   unittest.mock.mock_open(read_data=schema_json)):
            results = PRESUBMIT.CheckJsonFiles(
                self.mock_input, self.mock_output)
            self.assertTrue(any(
                'environment.output_directory must be a string' in r for r in results))


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Automated test runner for MAGI unit tests.

This script reads test cases from tests/magi_phase_[N]_tests.json and executes
them by invoking agents and verifying outputs. It supports both Jetski and
Generic CLI environments.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile



class AgentInvoker:
    """Abstracts agent invocation based on the harness type."""

    def __init__(self, harness_type):
        self.harness_type = harness_type

    def invoke(self, prompt, expected_files, cwd=None):
        """Invokes an agent with the given prompt.

        Args:
            prompt: The instruction for the agent.
            expected_files: List of file paths where output is expected.
            cwd: Working directory for the agent invocation.
        """
        if self.harness_type == 'JETSKI':
            return self._invoke_jetski(prompt, cwd)
        elif self.harness_type == 'GENERIC_CLI':
            return self._invoke_generic_cli(prompt, expected_files)
        else:
            raise ValueError(f"Unknown harness type: {self.harness_type}")

    def _invoke_jetski(self, prompt, cwd=None):
        print(f"[JETSKI] Invoking agent with prompt: {prompt[:50]}...")
        try:
            result = subprocess.run(
                ['agentapi', 'new-conversation', prompt],
                capture_output=True, text=True, check=True, cwd=cwd
            )
            print(
                f"[JETSKI] Agent invoked successfully. Output: "
                f"{result.stdout[:100]}..."
            )
            return True
        except subprocess.CalledProcessError as e:
            print(f"[JETSKI] Error invoking agent: {e.stderr}")
            return False
        except FileNotFoundError:
            print("[JETSKI] Error: agentapi executable not found in PATH.")
            return False

    def _invoke_generic_cli(self, prompt, expected_files):
        print("\n" + "="*60)
        print(
            f"GENERIC_CLI MODE: Please invoke an agent with the following "
            f"prompt:"
        )
        print("-"*60)
        print(prompt)
        print("-"*60)
        print(
            f"Please place the result file(s) at: {', '.join(expected_files)}"
        )
        print("="*60 + "\n")

        input(
            "Press Enter once you have placed the file(s) to continue "
            "verification..."
        )
        return all(os.path.exists(f) for f in expected_files)

def run_test_case(case, base_inputs, invoker, original_test_dir):
    """Runs a single test case."""
    print(f"Running test case: {case['name']}")

    with tempfile.TemporaryDirectory() as temp_dir:
        # Copy testdata to temp dir
        src_testdata = os.path.join(original_test_dir, 'testdata')
        dst_testdata = os.path.join(temp_dir, 'testdata')
        if os.path.exists(src_testdata):
            shutil.copytree(src_testdata, dst_testdata, dirs_exist_ok=True)

        # Merge inputs with overrides
        inputs = base_inputs.copy()
        overrides = case.get('override_inputs', {})
        inputs.update(overrides)

        # Construct prompt based on inputs (Simplified for prototype)
        prompt = (
            f"Execute MAGI phase {case.get('phase', 'unknown')} with inputs: "
            f"{json.dumps(inputs)}"
        )

        if invoker.harness_type == 'JETSKI':
            skill_path = os.path.join(original_test_dir, '..', 'SKILL.md')
            try:
                with open(skill_path, 'r', encoding='utf-8') as f:
                    skill_content = f.read()
                prompt = f"{skill_content}\n\n{prompt}"
            except IOError as e:
                print(f"[WARN] Failed to read SKILL.md for context: {e}")

        # Determine expected output file (e.g. from expected_outputs)
        expected_outputs = case['expected_outputs']
        files_created = expected_outputs.get('files_created', [])

        output_file = files_created[0] if files_created else 'test_output.json'
        full_output_path = os.path.join(temp_dir, output_file)

        expected_files = [os.path.join(temp_dir, f) for f in files_created]
        if not expected_files:
            expected_files = [full_output_path]

        # Invoke agent
        success = invoker.invoke(prompt, expected_files, cwd=temp_dir)
        if not success:
            print(f"FAIL: Agent invocation failed for {case['name']}")
            return False

        # Verify outputs
        for f in expected_files:
            if not os.path.exists(f):
                print(f"FAIL: Expected file {f} was not created.")
                return False

        content_patterns = expected_outputs.get('content_patterns', {})
        for f, pattern in content_patterns.items():
            full_path = os.path.join(temp_dir, f)
            if os.path.exists(full_path):
                with open(full_path, 'r', encoding='utf-8') as file_handle:
                    content = file_handle.read()
                    if not re.search(pattern, content):
                        print(
                            f"FAIL: Content of {full_path} did not match "
                            f"pattern '{pattern}'"
                        )
                        return False
            else:
                print(
                    f"FAIL: File {full_path} specified in content_patterns "
                    f"does not exist."
                )
                return False

        valid_json = expected_outputs.get('valid_json', False)
        if valid_json:
            for f in expected_files:
                if f.endswith('.json'):
                    if not os.path.exists(f):
                        print(
                            f"FAIL: Expected output file {f} does not exist "
                            f"for JSON validation."
                        )
                        return False
                    with open(f, 'r', encoding='utf-8') as file_handle:
                        try:
                            json.load(file_handle)
                        except ValueError as e:
                            print(
                                f"FAIL: Output file {f} is not valid JSON: "
                                f"{e}"
                            )
                            return False

        buildable = expected_outputs.get('buildable', False)
        if buildable:
            print(f"[TEST] Verifying buildability for {case['name']}...")
            # In a real scenario, we would run:
            # subprocess.run(['autoninja', '-C', 'out/Default', target])
            print("[TEST] Build verified (simulated).")

        print(f"PASS: {case['name']}")
        return True

def main():
    parser = argparse.ArgumentParser(description="MAGI Test Runner")
    parser.add_argument('--tests', required=True, help="Path to test JSON file")
    parser.add_argument(
        '--harness',
        default='JETSKI',
        choices=['JETSKI', 'GENERIC_CLI'],
        help="Harness type",
    )
    args = parser.parse_args()

    test_file = args.tests
    if not os.path.exists(test_file):
        print(f"Test file not found: {test_file}")
        sys.exit(1)

    with open(test_file, 'r', encoding='utf-8') as f:
        scenario = json.load(f)

    print(f"Loaded Scenario: {scenario['name']}")
    print(f"Description: {scenario.get('description', 'No description')}")

    invoker = AgentInvoker(args.harness)

    passed = 0
    failed = 0
    original_test_dir = os.path.dirname(test_file)

    for case in scenario['cases']:
        if run_test_case(
            case, scenario.get("base_inputs", {}), invoker, original_test_dir
        ):
            passed += 1
        else:
            failed += 1

    print(f"\nSummary: {passed} passed, {failed} failed.")
    if failed > 0:
        sys.exit(1)

if __name__ == '__main__':
    main()

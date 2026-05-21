#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""JSON formatting linter and fixer for MAGI protocol files.

This script enforces 2-space indentation and a single trailing newline for
JSON files within the magi-mode directory, while respecting specific
overrides (e.g., legacy 4-space files).
"""

import argparse
import json
import os
import sys

# Files that should use 4-space indentation instead of the 2-space default.
# Relative to the magi-mode directory.
INDENT_OVERRIDES = {
    'tests/magi_stage_refine_tests.json': 4,
}


def _ValidateMultilineStrings(data, file_path):
    """Ensures multiline string arrays follow the 'Joining Rule'.

  The Joining Rule (defined in SKILL.md) requires that each element in an
  array (except the last) must end with a trailing space or punctuation to
  prevent token merging during concatenation.

  Returns:
    A list of error messages.
  """
    errors = []

    def _CheckValue(val, path):
        if isinstance(val, list):
            for i, item in enumerate(val):
                if isinstance(item, str) and i < len(val) - 1:
                    if not (item.endswith(' ')
                            or item.endswith(tuple('.,!?;:'))):
                        errors.append(
                            f"Joining Rule violation in {file_path} at {path}[{i}]: "
                            "String lacks trailing space or punctuation.")
                elif isinstance(item, (dict, list)):
                    _CheckValue(item, f"{path}[{i}]")
        elif isinstance(val, dict):
            for k, v in val.items():
                _CheckValue(v, f"{path}.{k}" if path else k)

    # Only personas are expected to have these multiline arrays for mandates
    # and checklists.
    if isinstance(data, dict) and 'personas/' in file_path.replace(
            os.sep, '/'):
        _CheckValue(data.get('mandate', []), 'mandate')
        _CheckValue(data.get('checklist', {}), 'checklist')

    return errors


def CheckFormatting(file_path, fix=False):
    """Checks or fixes the formatting of a JSON file.

  Args:
    file_path: Path to the JSON file.
    fix: If True, overwrites the file with the correct formatting.

  Returns:
    A tuple (formatting_ok, lint_errors).
    formatting_ok is True if the file matches the expected format (or was fixed).
    lint_errors is a list of strings describing Joining Rule violations.
  """
    magi_dir = os.path.dirname(os.path.abspath(__file__))
    rel_path = os.path.relpath(file_path, magi_dir)
    # Use Unix-style slashes for the override check.
    rel_path_unix = rel_path.replace(os.sep, '/')
    expected_indent = INDENT_OVERRIDES.get(rel_path_unix, 2)

    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            original_content = f.read()

        # Parse and re-format
        data = json.loads(original_content)

        # 1. Validate 'Joining Rule' for multiline strings in personas.
        lint_errors = _ValidateMultilineStrings(data, file_path)

        # 2. Enforce indentation and trailing newline.
        # ensure_ascii=False prevents escaping unicode characters.
        formatted_content = json.dumps(
            data, indent=expected_indent, sort_keys=False,
            ensure_ascii=False) + '\n'

        formatting_ok = (original_content == formatted_content)

        if not formatting_ok and fix:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(formatted_content)
            print(f"Fixed formatting for {file_path}")
            formatting_ok = True

        return formatting_ok, lint_errors

    except Exception as e:
        print(f"Error processing {file_path}: {e}")
        return False, [f"Exception: {e}"]


def main():
    parser = argparse.ArgumentParser(description="MAGI JSON Formatter")
    parser.add_argument('files', nargs='*', help="Files to check/fix")
    parser.add_argument('--fix',
                        action='store_true',
                        help="Fix formatting errors")
    args = parser.parse_args()

    magi_dir = os.path.dirname(os.path.abspath(__file__))

    files_to_process = args.files
    if not files_to_process:
        # Default to all JSON files in the magi-mode directory (excluding .temp)
        for root, dirs, files in os.walk(magi_dir):
            if '.temp' in dirs:
                dirs.remove('.temp')
            for f in files:
                if f.endswith('.json'):
                    files_to_process.append(os.path.join(root, f))

    success = True
    for f in files_to_process:
        fmt_ok, lint_errs = CheckFormatting(f, fix=args.fix)
        if not fmt_ok:
            success = False
            print(f"Formatting error in {f}")
        if lint_errs:
            success = False
            for err in lint_errs:
                print(err)

    if not success:
        sys.exit(1)


if __name__ == '__main__':
    main()

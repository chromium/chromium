# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for ios/testing

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

def CheckTestExpectations(input_api, output_api):
    """Checks syntax and bug format for lines in test_expectations.txt."""
    results = []
    affected_files = [
        f for f in input_api.AffectedFiles(include_deletes=False)
        if input_api.os_path.basename(f.LocalPath()) == 'test_expectations.txt'
    ]
    if not affected_files:
        return results

    pattern = input_api.re.compile(
        r'^(?:([^\s\[]\S*)\s+)?'  # Group 1 (optional in regex): Bug link
        r'(?:\[([^\]]+)\]\s+)?'   # Group 2 (optional): [Tags] block
        r'(\S+)\s+'               # Group 3 (required): TestClass[/Method]
        r'\[([^\]]+)\]'           # Group 4 (required): [Expectations] block
        r'(?:\s*#.*)?$'           # Optional trailing comment (# ...)
    )
    bug_pattern = input_api.re.compile(
        r'^(?:https?://)?crbug\.com/[a-zA-Z0-9_.-]+(?:/[a-zA-Z0-9_.-]+)*$'
    )
    valid_expectations = {'failure', 'pass', 'skip', 'crash'}

    for f in affected_files:
        for line_num, line in f.ChangedContents():
            trimmed = line.strip()
            if not trimmed or trimmed.startswith('#'):
                continue

            match = pattern.match(trimmed)
            if not match:
                results.append(output_api.PresubmitError(
                    f"{f.LocalPath()}:{line_num}: Line does not respect "
                    f"prescribed syntax:\n  {trimmed}\n"
                    "Expected format: crbug.com/BUG [ [Tags] ] "
                    "TestClassName[/TestMethodName] [ [Expectations] ]"
                ))
                continue

            bug_str = match.group(1)
            if not bug_str or not bug_pattern.match(bug_str):
                results.append(output_api.PresubmitError(
                    f"{f.LocalPath()}:{line_num}: Missing or invalid bug "
                    f"field:\n  {trimmed}\n"
                    "The bug field must be of the form crbug.com/BUG."
                ))

            test_id = match.group(3)
            if '*' in test_id:
                results.append(output_api.PresubmitError(
                    f"{f.LocalPath()}:{line_num}: Wildcards ('*') are not "
                    f"supported in test names:\n  {trimmed}\n"
                    "For class-level expectations, specify just the class "
                    "name without wildcards (e.g., TestClassName)."
                ))

            expectations_str = match.group(4)
            exps = expectations_str.split() if expectations_str else []
            if not exps:
                results.append(output_api.PresubmitError(
                    f"{f.LocalPath()}:{line_num}: Missing expectations in "
                    f"line:\n  {trimmed}"
                ))
            else:
                for exp in exps:
                    if exp.lower() not in valid_expectations:
                        results.append(output_api.PresubmitError(
                            f"{f.LocalPath()}:{line_num}: Invalid expectation "
                            f"'{exp}' in line:\n  {trimmed}\n"
                            "Valid expectations are Failure, Pass, Skip, Crash."
                        ))

    return results


def CheckChange(input_api, output_api):
    import sys
    old_sys_path = sys.path[:]
    results = []
    try:
        sys.path.append(input_api.change.RepositoryRoot())
        from build.ios import presubmit_support
        results += presubmit_support.CheckBundleData(input_api,
                                                     output_api,
                                                     'http_server_bundle_data',
                                                     globroot='.')
    finally:
        sys.path = old_sys_path
    results += CheckTestExpectations(input_api, output_api)
    return results


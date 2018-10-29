# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Blink.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import os


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    # We should figure out what license checks we actually want to use.
    license_header = r'.*'

    results = []
    results.extend(input_api.canned_checks.PanProjectChecks(
        input_api, output_api, maxlen=800, license_header=license_header))
    return results


def _CheckStyle(input_api, output_api):
    style_checker_path = input_api.os_path.join(input_api.PresubmitLocalPath(), '..', 'blink',
                                                'tools', 'check_blink_style.py')
    args = [input_api.python_executable, style_checker_path, '--diff-files']
    files = []
    for f in input_api.AffectedFiles():
        file_path = f.LocalPath()
        # Filter out changes in LayoutTests.
        if 'LayoutTests' + input_api.os_path.sep in file_path and 'TestExpectations' not in file_path:
            continue
        files.append(input_api.os_path.join('..', '..', file_path))
    # Do not call check_blink_style.py with empty affected file list if all
    # input_api.AffectedFiles got filtered.
    if not files:
        return []
    args += files
    results = []

    try:
        child = input_api.subprocess.Popen(args,
                                           stderr=input_api.subprocess.PIPE)
        _, stderrdata = child.communicate()
        if child.returncode != 0:
            results.append(output_api.PresubmitError(
                'check_blink_style.py failed', [stderrdata]))
    except Exception as e:
        results.append(output_api.PresubmitNotifyResult(
            'Could not run check_blink_style.py', [str(e)]))

    return results


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    results.extend(_CheckStyle(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    results.extend(input_api.canned_checks.CheckTreeIsOpen(
        input_api, output_api,
        json_url='http://chromium-status.appspot.com/current?format=json'))
    results.extend(input_api.canned_checks.CheckChangeHasDescription(
        input_api, output_api))
    return results

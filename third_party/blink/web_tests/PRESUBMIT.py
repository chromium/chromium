# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""LayoutTests/ presubmit script for Blink.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import filecmp
import inspect
import sys

USE_PYTHON3 = True


def _CheckTestharnessResults(input_api, output_api):
    """Checks for all-PASS generic baselines for testharness.js tests.

    These files are unnecessary because for testharness.js tests, if there is no
    baseline file then the test is considered to pass when the output is all
    PASS. Note that only generic baselines are checked because platform specific
    and virtual baselines might be needed to prevent fallback.
    """
    baseline_files = _TestharnessGenericBaselinesToCheck(input_api)
    if not baseline_files:
        return []

    checker_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
        '..', 'tools', 'check_testharness_expected_pass.py')

    args = [input_api.python_executable, checker_path]
    args.extend(baseline_files)
    _, errs = input_api.subprocess.Popen(args,
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE).communicate()
    if errs:
        return [output_api.PresubmitError(errs)]
    return []


def _TestharnessGenericBaselinesToCheck(input_api):
    """Returns a list of paths of generic baselines for testharness.js tests."""
    baseline_files = []
    this_dir = input_api.PresubmitLocalPath()
    for f in input_api.AffectedFiles():
        if f.Action() == 'D':
            continue
        path = f.AbsoluteLocalPath()
        if not path.endswith('-expected.txt'):
            continue
        if (input_api.os_path.join(this_dir, 'platform') in path or
            input_api.os_path.join(this_dir, 'virtual') in path or
            input_api.os_path.join(this_dir, 'flag-specific') in path):
            continue
        baseline_files.append(path)
    return baseline_files


def _CheckFilesUsingEventSender(input_api, output_api):
    """Check if any new layout tests still use eventSender. If they do, we encourage replacing them with
       chrome.gpuBenchmarking.pointerActionSequence.
    """
    results = []
    actions = ["eventSender.touch", "eventSender.mouse", "eventSender.gesture"]
    for f in input_api.AffectedFiles():
        if f.Action() == 'A':
            for line_num, line in f.ChangedContents():
                if line.find("eventSender.beginDragWithFiles") != -1:
                    break
                if any(action in line for action in actions):
                    results.append(output_api.PresubmitPromptWarning(
                        'eventSender is deprecated, please use chrome.gpuBenchmarking.pointerActionSequence instead ' +
                        '(see https://crbug.com/711340 and http://goo.gl/BND75q).\n' +
                        'Files: %s:%d %s ' % (f.LocalPath(), line_num, line)))
    return results


def _CheckTestExpectations(input_api, output_api):
    results = []
    os_path = input_api.os_path
    sys.path.append(
        os_path.join(
            os_path.dirname(
                os_path.abspath(inspect.getfile(_CheckTestExpectations))),
                '..', 'tools'))
    from blinkpy.web_tests.lint_test_expectations_presubmit import (
        PresubmitCheckTestExpectations)
    results.extend(PresubmitCheckTestExpectations(input_api, output_api))
    return results


def _CheckForJSTest(input_api, output_api):
    """'js-test.js' is the past, 'testharness.js' is our glorious future"""
    jstest_re = input_api.re.compile(r'resources/js-test.js')

    def source_file_filter(path):
        return input_api.FilterSourceFile(path, files_to_check=[r'\.(html|js|php|pl|svg)$'])

    errors = input_api.canned_checks._FindNewViolationsOfRule(
        lambda _, x: not jstest_re.search(x), input_api, source_file_filter)
    errors = ['  * %s' % violation for violation in errors]
    if errors:
        return [output_api.PresubmitPromptOrNotify(
            '"resources/js-test.js" is deprecated; please write new layout '
            'tests using the assertions in "resources/testharness.js" '
            'instead, as these can be more easily upstreamed to Web Platform '
            'Tests for cross-vendor compatibility testing. If you\'re not '
            'already familiar with this framework, a tutorial is available at '
            'https://darobin.github.io/test-harness-tutorial/docs/using-testharness.html'
            '\n\n%s' % '\n'.join(errors))]
    return []


def _CheckForInvalidPreferenceError(input_api, output_api):
    pattern = input_api.re.compile('Invalid name for preference: (.+)')
    results = []

    for f in input_api.AffectedFiles():
        if not f.LocalPath().endswith('-expected.txt'):
            continue
        for line_num, line in f.ChangedContents():
            error = pattern.search(line)
            if error:
                results.append(output_api.PresubmitError('Found an invalid preference %s in expected result %s:%s' % (error.group(1), f, line_num)))
    return results


def _CheckRunAfterLayoutAndPaintJS(input_api, output_api):
    """Checks if resources/run-after-layout-and-paint.js and
       http/tests/resources/run-after-layout-and-paint.js are the same."""
    js_file = input_api.os_path.join(input_api.PresubmitLocalPath(),
        'resources', 'run-after-layout-and-paint.js')
    http_tests_js_file = input_api.os_path.join(input_api.PresubmitLocalPath(),
        'http', 'tests', 'resources', 'run-after-layout-and-paint.js')
    for f in input_api.AffectedFiles():
        path = f.AbsoluteLocalPath()
        if path == js_file or path == http_tests_js_file:
            if not filecmp.cmp(js_file, http_tests_js_file):
                return [output_api.PresubmitError(
                    '%s and %s must be kept exactly the same' %
                    (js_file, http_tests_js_file))]
            break
    return []


def _CheckForUnlistedTestFolder(input_api, output_api):
    """Checks all the test folders under web_tests are listed in BUILD.gn.
    """
    this_dir = input_api.PresubmitLocalPath()
    possible_new_dirs = []
    for f in input_api.AffectedFiles():
        if f.Action() == 'A':
            # We only check added folders. For deleted folders, if BUILD.gn is
            # not updated, the build will fail at upload step. The reason is that
            # we can not know if the folder is deleted as there can be local
            # unchecked in files.
            path = f.AbsoluteLocalPath()
            fns = path[len(this_dir)+1:].split('/')
            if len(fns) > 1:
                possible_new_dirs.append(fns[0])

    if possible_new_dirs:
        path_build_gn = input_api.os_path.join(input_api.change.RepositoryRoot(), 'BUILD.gn')
        dirs_from_build_gn = []
        start_line = '# === List Test Cases folders here ==='
        end_line = '# === Test Case Folders Ends ==='
        find_start_line  = False
        for line in input_api.ReadFile(path_build_gn).splitlines():
            line = line.strip()
            if line.startswith(start_line):
                find_start_line = True
                continue
            if find_start_line:
                if line.startswith(end_line):
                    break
                if len(line.split('/')) > 1:
                    dirs_from_build_gn.append(line.split('/')[-2])
        dirs_from_build_gn.extend(['platform', 'FlagExpectations', 'flag-specific'])

        new_dirs = [x for x in possible_new_dirs if x not in dirs_from_build_gn]
        if new_dirs:
            dir_plural = "directories" if len(new_dirs) > 1 else "directory"
            error_message = (
                'This CL adds new %s(%s) under //third_party/blink/web_tests/, but //BUILD.gn '
                'is not updated. Please add the %s to BUILD.gn.' % (dir_plural, ', '.join(new_dirs), dir_plural))
            if input_api.is_committing:
                return [output_api.PresubmitError(error_message)]
            else:
                return [output_api.PresubmitPromptWarning(error_message)]
    return []


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckTestharnessResults(input_api, output_api))
    results.extend(_CheckFilesUsingEventSender(input_api, output_api))
    results.extend(_CheckTestExpectations(input_api, output_api))
    results.extend(_CheckForJSTest(input_api, output_api))
    results.extend(_CheckForInvalidPreferenceError(input_api, output_api))
    results.extend(_CheckRunAfterLayoutAndPaintJS(input_api, output_api))
    results.extend(_CheckForUnlistedTestFolder(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CheckTestharnessResults(input_api, output_api))
    results.extend(_CheckFilesUsingEventSender(input_api, output_api))
    results.extend(_CheckTestExpectations(input_api, output_api))
    results.extend(_CheckForUnlistedTestFolder(input_api, output_api))
    return results

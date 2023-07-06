# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""LayoutTests/ presubmit script for Blink.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import filecmp
import inspect
import os
import sys
import tempfile
import re
from html.parser import HTMLParser

USE_PYTHON3 = True
WPT_IMPORTER_EMAIL = "wpt-autoroller@chops-service-accounts.iam.gserviceaccount.com"


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

    # When running git cl presubmit --all this presubmit may be asked to check
    # ~19,000 files. Passing these on the command line would far exceed Windows
    # limits, so we use --path-files instead.

    # We have to set delete=False and then let the object go out of scope so
    # that the file can be opened by name on Windows.
    with tempfile.NamedTemporaryFile('w+', newline='', delete=False) as f:
        for path in baseline_files:
            f.write('%s\n' % path)
        paths_name = f.name

    args = [
        input_api.python3_executable, checker_path, '--path-files', paths_name
    ]
    _, errs = input_api.subprocess.Popen(
        args,
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE,
        universal_newlines=True).communicate()

    os.remove(paths_name)
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
        if (input_api.os_path.join(this_dir, 'platform') in path
                or input_api.os_path.join(this_dir, 'virtual') in path
                or input_api.os_path.join(this_dir, 'flag-specific') in path):
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
    possible_new_dirs = set()
    for f in input_api.AffectedFiles():
        if f.Action() == 'A':
            # We only check added folders. For deleted folders, if BUILD.gn is
            # not updated, the build will fail at upload step. The reason is that
            # we can not know if the folder is deleted as there can be local
            # unchecked in files.
            path = f.AbsoluteLocalPath()
            fns = path[len(this_dir)+1:].split('/')
            if len(fns) > 1:
                possible_new_dirs.add(fns[0])

    if possible_new_dirs:
        path_build_gn = input_api.os_path.join(input_api.change.RepositoryRoot(), 'BUILD.gn')
        dirs_from_build_gn = []
        start_line = '# === List Test Cases folders here ==='
        end_line = '# === Test Case Folders Ends ==='
        end_line_count = 0
        find_start_line  = False
        for line in input_api.ReadFile(path_build_gn).splitlines():
            line = line.strip()
            if line.startswith(start_line):
                find_start_line = True
                continue
            if find_start_line:
                if line.startswith(end_line):
                    find_start_line = False
                    end_line_count += 1
                    if end_line_count == 2:
                        break
                    continue
                if len(line.split('/')) > 1:
                    dirs_from_build_gn.append(line.split('/')[-2])
        dirs_from_build_gn.extend(
            ['platform', 'FlagExpectations', 'flag-specific', 'SmokeTests'])

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


def _CheckForExtraVirtualBaselines(input_api, output_api):
    """Checks that expectations in virtual test suites are for virtual test suites that exist
    """
    os_path = input_api.os_path

    local_dir = os_path.relpath(
        os_path.normpath('{0}/'.format(input_api.PresubmitLocalPath().replace(
            os_path.sep, '/'))), input_api.change.RepositoryRoot())

    check_all = False
    check_files = []
    for f in input_api.AffectedFiles(include_deletes=False):
        local_path = f.LocalPath()
        assert local_path.startswith(local_dir)
        local_path = os_path.relpath(local_path, local_dir)
        path_components = local_path.split(os_path.sep)
        if f.Action() == 'A':
            if len(path_components) > 2 and path_components[0] == 'virtual':
                check_files.append((local_path, path_components[1]))
            elif (len(path_components) > 4 and path_components[2] == 'virtual'
                  and (path_components[0] == 'platform'
                       or path_components[0] == 'flag-specific')):
                check_files.append((local_path, path_components[3]))
        elif local_path == 'VirtualTestSuites':
            check_all = True

    if not check_all and len(check_files) == 0:
        return []

    # The rest of this test fails on Windows because win32pipe is not available
    # and other errors.
    if os.name == 'nt':
        return []

    from blinkpy.common.host import Host
    port_factory = Host().port_factory
    known_virtual_suites = [
        suite.full_prefix[8:-1] for suite in port_factory.get(
            port_factory.all_port_names()[0]).virtual_test_suites()
    ]

    results = []
    if check_all:
        for f in input_api.change.AllFiles(
                os_path.join(input_api.PresubmitLocalPath(), "virtual")):
            suite = f.split('/')[0]
            if not suite in known_virtual_suites:
                path = os_path.relpath(
                    os_path.join(input_api.PresubmitLocalPath(), "virtual", f),
                    input_api.change.RepositoryRoot())
                results.append(
                    output_api.PresubmitError(
                        "Baseline %s exists, but %s is not a known virtual test suite."
                        % (path, suite)))
        for subdir in ["platform", "flag-specific"]:
            for f in input_api.change.AllFiles(
                    os_path.join(input_api.PresubmitLocalPath(), subdir)):
                path_components = f.split('/')
                if len(path_components) < 3 or path_components[1] != 'virtual':
                    continue
                suite = path_components[2]
                if not suite in known_virtual_suites:
                    path = os_path.relpath(
                        os_path.join(input_api.PresubmitLocalPath(), subdir,
                                     f), input_api.change.RepositoryRoot())
                    results.append(
                        output_api.PresubmitError(
                            "Baseline %s exists, but %s is not a known virtual test suite."
                            % (path, suite)))
    else:
        for (f, suite) in check_files:
            if not suite in known_virtual_suites:
                path = os_path.relpath(
                    os_path.join(input_api.PresubmitLocalPath(), f),
                    input_api.change.RepositoryRoot())
                results.append(
                    output_api.PresubmitError(
                        "This CL adds a new baseline %s, but %s is not a known virtual test suite."
                        % (path, suite)))
    return results


def _CheckWebViewExpectations(input_api, output_api):
    src_dir = os.path.join(input_api.PresubmitLocalPath(), os.pardir,
                           os.pardir, os.pardir)
    webview_data_dir = input_api.os_path.join(src_dir, 'android_webview',
                                              'tools', 'system_webview_shell',
                                              'test', 'data', 'webexposed')
    if webview_data_dir not in sys.path:
        sys.path.append(webview_data_dir)

    # pylint: disable=import-outside-toplevel
    from exposed_webview_interfaces_presubmit import (
        CheckNotWebViewExposedInterfaces)
    return CheckNotWebViewExposedInterfaces(input_api, output_api)


class _DoctypeParser(HTMLParser):
    """Parses HTML to check if there exists a DOCTYPE declaration before all other tags.
    """

    def __init__(self):
        super().__init__()
        self.encountered_tag = False
        self.doctype = ""

    def handle_starttag(self, *_):
        self.encountered_tag = True

    def handle_startendtag(self, *_):
        self.encountered_tag = True

    def handle_decl(self, decl):
        if not self.encountered_tag:
            self.doctype = decl
            self.encountered_tag = True


def _IsDoctypeHTMLSet(lines):
    """Returns true if the given HTML file starts with <!DOCTYPE html>.
    """
    parser = _DoctypeParser()
    for l in lines:
        parser.feed(l)

    return re.match("DOCTYPE\s*html\s*$", parser.doctype, re.IGNORECASE)


def _CheckForDoctypeHTML(input_api, output_api):
    """Checks that all changed HTML files start with the correct <!DOCTYPE html> tag.
    """
    results = []

    if input_api.no_diffs:
        return results

    # These tests are being imported from WPT, so <!DOCTYPE html> is not required yet.
    no_errors = (input_api.change.author_email == WPT_IMPORTER_EMAIL)

    for f in input_api.AffectedFiles(include_deletes=False):
        path = f.LocalPath()
        fname = os.path.basename(path)

        if not fname.endswith(".html") or "quirk" in fname:
            continue

        if not _IsDoctypeHTMLSet(f.NewContents()):
            error = "HTML file \"%s\" does not start with <!DOCTYPE html>. " \
                    "If you really intend to test in quirks mode, add \"quirk\" " \
                    "to the name of your test." % path

            if f.Action() == "A" or _IsDoctypeHTMLSet(f.OldContents()):
                if no_errors:
                    results.append(output_api.PresubmitPromptWarning(error))
                else:
                    results.append(output_api.PresubmitError(error))

    return results


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckTestharnessResults(input_api, output_api))
    results.extend(_CheckFilesUsingEventSender(input_api, output_api))
    results.extend(_CheckTestExpectations(input_api, output_api))
    results.extend(_CheckForJSTest(input_api, output_api))
    results.extend(_CheckForInvalidPreferenceError(input_api, output_api))
    results.extend(_CheckRunAfterLayoutAndPaintJS(input_api, output_api))
    results.extend(_CheckForUnlistedTestFolder(input_api, output_api))
    results.extend(_CheckForExtraVirtualBaselines(input_api, output_api))
    results.extend(_CheckWebViewExpectations(input_api, output_api))
    results.extend(_CheckForDoctypeHTML(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CheckTestharnessResults(input_api, output_api))
    results.extend(_CheckFilesUsingEventSender(input_api, output_api))
    results.extend(_CheckTestExpectations(input_api, output_api))
    results.extend(_CheckForUnlistedTestFolder(input_api, output_api))
    results.extend(_CheckForExtraVirtualBaselines(input_api, output_api))
    results.extend(_CheckWebViewExpectations(input_api, output_api))
    results.extend(_CheckForDoctypeHTML(input_api, output_api))
    return results

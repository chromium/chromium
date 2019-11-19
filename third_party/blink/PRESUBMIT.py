# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Blink.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import imp
import inspect
import os

try:
    # pylint: disable=C0103
    audit_non_blink_usage = imp.load_source(
        'audit_non_blink_usage',
        os.path.join(os.path.dirname(inspect.stack()[0][1]),
                     'tools/blinkpy/presubmit/audit_non_blink_usage.py'))
except IOError:
    # One of the presubmit upload tests tries to exec this script, which doesn't interact so well
    # with the import hack... just ignore the exception here and hope for the best.
    pass


_EXCLUDED_PATHS = (
    # These are third-party dependencies that we don't directly control.
    r'^third_party[\\/]blink[\\/]tools[\\/]blinkpy[\\/]third_party[\\/]wpt[\\/]wpt[\\/].*',
    r'^third_party[\\/]blink[\\/]web_tests[\\/]external[\\/]wpt[\\/]tools[\\/].*',
    r'^third_party[\\/]blink[\\/]web_tests[\\/]external[\\/]wpt[\\/]resources[\\/]webidl2[\\/].*',
)


def _CheckForWrongMojomIncludes(input_api, output_api):
    # In blink the code should either use -blink.h or -shared.h mojom
    # headers, except in public where only -shared.h headers should be
    # used to avoid exporting Blink types outside Blink.
    def source_file_filter(path):
        return input_api.FilterSourceFile(path,
                                          black_list=[r'third_party/blink/common/', r'third_party/blink/public/common'])

    pattern = input_api.re.compile(r'#include\s+.+\.mojom(.*)\.h[>"]')
    public_folder = input_api.os_path.normpath('third_party/blink/public/')
    non_blink_mojom_errors = []
    public_blink_mojom_errors = []
    for f in input_api.AffectedFiles(file_filter=source_file_filter):
        for line_num, line in f.ChangedContents():
            error_list = None
            # This is not super precise as we don't check endif, but allow
            # including Blink variant mojom files if the file has
            # '#if INSIDE_BLINK'.
            match = pattern.match(line)
            if match:
                if match.group(1) != '-shared':
                    if f.LocalPath().startswith(public_folder):
                        error_list = public_blink_mojom_errors
                    elif match.group(1) not in ('-blink', '-blink-forward', '-blink-test-utils'):
                        # Neither -shared.h, -blink.h, -blink-forward.h nor -blink-test-utils.h.
                        error_list = non_blink_mojom_errors

            if error_list is not None:
                error_list.append('    %s:%d %s' % (
                    f.LocalPath(), line_num, line))

    results = []
    if non_blink_mojom_errors:
        results.append(output_api.PresubmitError(
            'Files that include non-Blink variant mojoms found. '
            'You must include .mojom-blink.h or .mojom-shared.h instead:',
            non_blink_mojom_errors))

    if public_blink_mojom_errors:
        results.append(output_api.PresubmitError(
            'Public blink headers using Blink variant mojoms found. '
            'You must include .mojom-shared.h instead:',
            public_blink_mojom_errors))

    return results


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    # We should figure out what license checks we actually want to use.
    license_header = r'.*'

    results = []
    results.extend(input_api.canned_checks.PanProjectChecks(
        input_api, output_api, excluded_paths=_EXCLUDED_PATHS,
        maxlen=800, license_header=license_header))
    results.extend(_CheckForWrongMojomIncludes(input_api, output_api))
    return results


def _FilterPaths(input_api):
    """Returns input files with certain paths removed."""
    files = []
    for f in input_api.AffectedFiles():
        file_path = f.LocalPath()
        # Filter out changes in web_tests/.
        if ('web_tests' + input_api.os_path.sep in file_path
            and 'TestExpectations' not in file_path):
            continue
        if '/PRESUBMIT' in file_path:
            continue
        files.append(input_api.os_path.join('..', '..', file_path))
    return files


def _CheckStyle(input_api, output_api):
    files = _FilterPaths(input_api)
    # Do not call check_blink_style.py with empty affected file list if all
    # input_api.AffectedFiles got filtered.
    if not files:
        return []

    style_checker_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                                'tools', 'check_blink_style.py')
    args = [input_api.python_executable, style_checker_path, '--diff-files']
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


def _CheckForPrintfDebugging(input_api, output_api):
    """Generally speaking, we'd prefer not to land patches that printf
    debug output.
    """
    printf_re = input_api.re.compile(r'^\s*(printf\(|fprintf\(stderr,)')
    errors = input_api.canned_checks._FindNewViolationsOfRule(
        lambda _, x: not printf_re.search(x),
        input_api, None)
    errors = ['  * %s' % violation for violation in errors]
    if errors:
        return [output_api.PresubmitPromptOrNotify(
            'printf debugging is best debugging! That said, it might '
            'be a good idea to drop the following occurences from '
            'your patch before uploading:\n%s' % '\n'.join(errors))]
    return []


def _CheckForForbiddenChromiumCode(input_api, output_api):
    """Checks that Blink uses Chromium classes and namespaces only in permitted code."""
    # TODO(dcheng): This is pretty similar to _FindNewViolationsOfRule. Unfortunately, that can;'t
    # be used directly because it doesn't give the callable enough information (namely the path to
    # the file), so we duplicate the logic here...
    results = []
    for f in input_api.AffectedFiles():
        path = f.LocalPath()
        errors = audit_non_blink_usage.check(path, [(i + 1, l) for i, l in enumerate(f.NewContents())])
        if errors:
            errors = audit_non_blink_usage.check(path, f.ChangedContents())
            if errors:
                for error in errors:
                    msg = '%s:%d uses disallowed identifier %s' % (
                        path, error.line, error.identifier)
                    if error.advice:
                        msg += ". Advice: %s" % "\n".join(error.advice)
                    results.append(output_api.PresubmitError(msg))
    return results


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    results.extend(_CheckStyle(input_api, output_api))
    results.extend(_CheckForPrintfDebugging(input_api, output_api))
    results.extend(_CheckForForbiddenChromiumCode(input_api, output_api))
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

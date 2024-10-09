# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for Blink.

See https://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import importlib
import inspect
import os
import re

try:
    # pylint: disable=C0103
    module_name = 'audit_non_blink_usage'
    module_path = os.path.join(
        os.path.dirname(inspect.stack()[0][1]),
        'tools/blinkpy/presubmit/audit_non_blink_usage.py')
    audit_non_blink_usage = importlib.machinery.SourceFileLoader(
        module_name, module_path).load_module()
except IOError:
    # One of the presubmit upload tests tries to exec this script, which
    # doesn't interact so well with the import hack... just ignore the
    # exception here and hope for the best.
    pass

_EXCLUDED_PATHS = (
    # These are third-party dependencies that we don't directly control.
    r'^third_party[\\/]blink[\\/]tools[\\/]blinkpy[\\/]third_party[\\/]wpt[\\/]wpt[\\/].*',
    r'^third_party[\\/]blink[\\/]web_tests[\\/]external[\\/]wpt[\\/]resources[\\/]webidl2[\\/].*',
)


def _CheckForWrongMojomIncludes(input_api, output_api):
    # In blink the code should either use -blink.h or -shared.h mojom
    # headers, except in public where only -shared.h headers should be
    # used to avoid exporting Blink types outside Blink.
    def source_file_filter(path):
        return input_api.FilterSourceFile(
            path,
            files_to_skip=[
                r'.*_test.*\.(cc|h)$',
                r'third_party[\\/]blink[\\/]common[\\/]',
                r'third_party[\\/]blink[\\/]public[\\/]common[\\/]',
                r'third_party[\\/]blink[\\/]renderer[\\/]platform[\\/]loader[\\/]fetch[\\/]url_loader[\\/]',
                r'third_party[\\/]blink[\\/]renderer[\\/]core[\\/]frame[\\/]web_view_impl.*\.(cc|h)$',
                r'third_party[\\/]blink[\\/]renderer[\\/]core[\\/]frame[\\/]web.*frame.*\.(cc|h)$',
            ])

    pattern = input_api.re.compile(r'#include\s+[<"](.+)\.mojom(.*)\.h[>"]')
    public_folder = input_api.os_path.normpath('third_party/blink/public/')
    non_blink_mojom_errors = []
    public_blink_mojom_errors = []

    # Allow including specific non-blink interfaces that are used in the
    # public C++ API. Adding to these allowed interfaces should meet the
    # following conditions:
    # - Its pros/cons is discussed and have consensus on
    #   platform-architecture-dev@ or
    # - It uses POD types that will not import STL (or base string) types into
    #   blink (such as no strings or vectors).
    #
    # So far, non-blink interfaces are allowed only for loading / loader and
    # media interfaces so that we don't need type conversions to get through
    # the boundary between Blink and non-Blink.
    allowed_interfaces = (
        'services/network/public/mojom/cross_origin_embedder_policy',
        'services/network/public/mojom/early_hints',
        'services/network/public/mojom/fetch_api',
        'services/network/public/mojom/load_timing_info',
        'services/network/public/mojom/url_loader',
        'services/network/public/mojom/url_loader_factory',
        'services/network/public/mojom/url_response_head',
        'third_party/blink/public/mojom/blob/blob',
        'third_party/blink/public/mojom/blob/serialized_blob',
        'third_party/blink/public/mojom/browser_interface_broker',
        'third_party/blink/public/mojom/fetch/fetch_api_request',
        'third_party/blink/public/mojom/loader/code_cache',
        'third_party/blink/public/mojom/loader/fetch_later',
        'third_party/blink/public/mojom/loader/local_resource_loader_config',
        'third_party/blink/public/mojom/loader/resource_load_info',
        'third_party/blink/public/mojom/loader/resource_load_info_notifier',
        'third_party/blink/public/mojom/loader/transferrable_url_loader',
        'third_party/blink/public/mojom/navigation/renderer_content_settings',
        'third_party/blink/public/mojom/page/prerender_page_param',
        'third_party/blink/public/mojom/partitioned_popins/partitioned_popin_params',
        'third_party/blink/public/mojom/worker/subresource_loader_updater',
        'third_party/blink/public/mojom/worker/worklet_global_scope_creation_params',
        'media/mojo/mojom/interface_factory', 'media/mojo/mojom/audio_decoder',
        'media/mojo/mojom/audio_encoder', 'media/mojo/mojom/video_decoder',
        'media/mojo/mojom/media_metrics_provider')

    for f in input_api.AffectedFiles(file_filter=source_file_filter):
        for line_num, line in f.ChangedContents():
            error_list = None
            match = pattern.match(line)
            if (match and match.group(1) not in allowed_interfaces):
                if match.group(2) not in ('-shared', '-forward'):
                    if f.LocalPath().startswith(public_folder):
                        error_list = public_blink_mojom_errors
                    elif match.group(2) not in ('-blink', '-blink-forward',
                                                '-blink-test-utils'):
                        # Neither -shared.h, -blink.h, -blink-forward.h nor
                        # -blink-test-utils.h.
                        error_list = non_blink_mojom_errors

            if error_list is not None:
                error_list.append('    %s:%d %s' %
                                  (f.LocalPath(), line_num, line))

    results = []
    if non_blink_mojom_errors:
        results.append(
            output_api.PresubmitError(
                'Files that include non-Blink variant mojoms found. '
                'You must include .mojom-blink.h, .mojom-forward.h or '
                '.mojom-shared.h instead:', non_blink_mojom_errors))

    if public_blink_mojom_errors:
        results.append(
            output_api.PresubmitError(
                'Public blink headers using Blink variant mojoms found. '
                'You must include .mojom-forward.h or .mojom-shared.h '
                'instead:', public_blink_mojom_errors))

    return results


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    # We should figure out what license checks we actually want to use.
    license_header = r'.*'

    results = []
    results.extend(
        input_api.canned_checks.PanProjectChecks(
            input_api,
            output_api,
            excluded_paths=_EXCLUDED_PATHS,
            owners_check=False,
            maxlen=800,
            license_header=license_header,
            global_checks=False))
    results.extend(_CheckForWrongMojomIncludes(input_api, output_api))
    return results


def _FilterPaths(input_api):
    """Returns input files with certain paths removed."""
    files = []
    for f in input_api.AffectedFiles():
        file_path = f.AbsoluteLocalPath()
        # Filter out changes in web_tests/ so they are not linted. Some files
        # are intentionally malformed for testing. Also, external WPTs may have
        # non-Chromium authors.
        if 'web_tests' + input_api.os_path.sep in file_path:
            continue
        if '/PRESUBMIT' in file_path:
            continue
        # Skip files that were generated by bison.
        if re.search(
                'third_party/blink/renderer/'
                'core/xml/xpath_grammar_generated\.(cc|h)$', file_path):
            continue
        files.append(file_path)
    return files


def _CheckStyle(input_api, output_api):
    files = _FilterPaths(input_api)
    # Do not call check_blink_style.py with empty affected file list if all
    # input_api.AffectedFiles got filtered.
    if not files:
        return []

    style_checker_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                                'tools',
                                                'check_blink_style.py')
    # When running git cl presubmit --all this presubmit may be asked to check
    # ~260 files, leading to a command line that is about 17,000 characters.
    # This goes past the Windows 8191 character cmd.exe limit and causes cryptic
    # failures. To avoid these we break the command up into smaller pieces.
    # Depending on how long the command is on Windows the error may be:
    #     The command line is too long.
    # Or it may be:
    #     OSError: Execution failed with error: [WinError 206] The filename or
    #     extension is too long.
    # The latter error comes from CreateProcess hitting its 32768 character
    # limit.
    files_per_command = 40 if input_api.is_windows else 1000
    results = []
    for i in range(0, len(files), files_per_command):
        args = [
            input_api.python3_executable, style_checker_path, '--diff-files'
        ]
        args += files[i:i + files_per_command]

        try:
            child = input_api.subprocess.Popen(
                args, stderr=input_api.subprocess.PIPE)
            _, stderrdata = child.communicate()
            if child.returncode != 0:
                results.append(
                    output_api.PresubmitError('check_blink_style.py failed',
                                              [stderrdata.decode('utf-8')]))
        except Exception as e:
            results.append(
                output_api.PresubmitNotifyResult(
                    'Could not run check_blink_style.py', [str(e)]))

    # By default, the pylint canned check lints all Python files together to
    # check for potential problems between dependencies. This is slow to run
    # across all of Blink (>2 min), so only lint affected files.
    affected_python_files = [
        input_api.os_path.relpath(file_path, input_api.PresubmitLocalPath())
        for file_path in files if input_api.fnmatch.fnmatch(file_path, '*.py')
    ]
    if affected_python_files:
        pylintrc = input_api.os_path.join('tools', 'blinkpy', 'pylintrc')
        results.extend(
            input_api.RunTests(
                input_api.canned_checks.GetPylint(
                    input_api,
                    output_api,
                    files_to_check=[
                        re.escape(path) for path in affected_python_files
                    ],
                    pylintrc=pylintrc)))
    return results


def _CheckForPrintfDebugging(input_api, output_api):
    """Generally speaking, we'd prefer not to land patches that printf
    debug output.
    """
    printf_re = input_api.re.compile(r'^\s*(printf\(|fprintf\(stderr,)')
    errors = input_api.canned_checks._FindNewViolationsOfRule(
        lambda _, x: not printf_re.search(x), input_api, None)
    errors = ['  * %s' % violation for violation in errors]
    if errors:
        return [
            output_api.PresubmitPromptOrNotify(
                'printf debugging is best debugging! That said, it might '
                'be a good idea to drop the following occurences from '
                'your patch before uploading:\n%s' % '\n'.join(errors))
        ]
    return []


def _CheckForForbiddenChromiumCode(input_api, output_api):
    """Checks that Blink uses Chromium classes and namespaces only in
    permitted code.
    """
    # TODO(dcheng): This is pretty similar to _FindNewViolationsOfRule.
    # Unfortunately, that can't be used directly because it doesn't give the
    # callable enough information (namely the path to the file), so we
    # duplicate the logic here...
    results = []
    for f in input_api.AffectedFiles():
        path = f.LocalPath()
        errors = audit_non_blink_usage.check(
            path, [(i + 1, l) for i, l in enumerate(f.NewContents())])
        if errors:
            errors = audit_non_blink_usage.check(path, f.ChangedContents())
            if errors:
                for error in errors:
                    msg = '%s:%d uses disallowed identifier %s' % (
                        path, error.line, error.identifier)
                    if error.advice:
                        msg += ". Advice: %s" % "\n".join(error.advice)
                    if error.warning:
                        results.append(output_api.PresubmitPromptWarning(msg))
                    else:
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
    return results

# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for external/wpt.

See https://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


USE_PYTHON3 = True


def python3_command(input_api):
    if not input_api.is_windows:
        return 'python3'

    # The subprocess module on Windows does not look at PATHEXT, so we cannot
    # rely on 'python3' working. Instead we must check each possible name to
    # find the working one.
    input_api.logging.debug('Searching for Python 3 command name')

    exts = list(filter(len, input_api.environ.get('PATHEXT', '').split(';')))
    for ext in [''] + exts:
        python = 'python3%s' % ext
        input_api.logging.debug('Trying "%s"' % python)
        try:
            input_api.subprocess.check_output([python, '--version'])
            return python
        except WindowsError:
            pass
    raise WindowsError('Unable to find a valid python3 command name')


def _LintWPT(input_api, output_api):
    """Lint functionality duplicated from web-platform-tests upstream.

    This is to catch lint errors that would otherwise be caught in WPT CI.
    See https://web-platform-tests.org/writing-tests/lint-tool.html for more
    information about the lint tool.
    """
    wpt_path = input_api.os_path.join(input_api.PresubmitLocalPath(), 'wpt')
    linter_path = input_api.os_path.join(input_api.change.RepositoryRoot(),
                                         'third_party', 'wpt_tools', 'wpt',
                                         'wpt')

    paths_in_wpt = []
    for abs_path in input_api.AbsoluteLocalPaths():
        if abs_path.endswith(input_api.os_path.relpath(abs_path, wpt_path)):
            paths_in_wpt.append(abs_path)

    # If there are changes in LayoutTests/external that aren't in wpt, e.g.
    # changes to wpt_automation or this presubmit script, then we can return
    # to avoid running the linter on all files in wpt (which is slow).
    if not paths_in_wpt:
        return []

    # When running git cl presubmit --all this presubmit may be asked to check
    # ~65,000 files, leading to a command line that is over 7,000,000 characters.
    # This goes past the Windows 8191 character cmd.exe limit and causes cryptic
    # failures. To avoid these we break the command up into smaller pieces. The
    # non-Windows limit is chosen so that the code that splits up commands will
    # get some exercise on other platforms.
    # Depending on how long the command is on Windows the error may be:
    #     The command line is too long.
    # Or it may be:
    #     OSError: Execution failed with error: [WinError 206] The filename or
    #     extension is too long.
    # I suspect that the latter error comes from CreateProcess hitting its 32768
    # character limit.
    files_per_command = 25 if input_api.is_windows else 1000
    results = []
    for i in range(0, len(paths_in_wpt), files_per_command):
        args = [
            python3_command(input_api),
            linter_path,
            'lint',
            '--repo-root=%s' % wpt_path,
            '--ignore-glob=*-expected.txt',
            '--ignore-glob=*DIR_METADATA',
            '--ignore-glob=*OWNERS',
        ] + paths_in_wpt[i:i + files_per_command]

        proc = input_api.subprocess.Popen(args,
                                          stdout=input_api.subprocess.PIPE,
                                          stderr=input_api.subprocess.PIPE)
        stdout, stderr = proc.communicate()

        if proc.returncode != 0:
            results.append(
                output_api.PresubmitError('wpt lint failed:',
                                          long_text=stdout + stderr))
    return results


def _DontModifyIDLFiles(input_api, output_api):
    """Detect manual modification of the generated files in interfaces/

    These files are generated automatically by reffy based on published web specs.
    Manual modifications will almost always just be reverted by reffy upstream,
    and this has historically caused developers bad surprises.

    See https://crbug.com/1016354
    """
    interfaces_path = input_api.os_path.join(input_api.PresubmitLocalPath(), 'wpt', 'interfaces')

    def is_idl_file(f):
        abs_path = f.AbsoluteLocalPath()
        return abs_path.endswith(input_api.os_path.relpath(abs_path, interfaces_path))

    idl_files = [f.LocalPath() for f in input_api.AffectedSourceFiles(is_idl_file)]

    if not idl_files:
        return []
    return [
        output_api.PresubmitPromptWarning(
            'This CL touches generated IDL files. Manual modifications to these files will\n'
            'likely be overwritten upstream; please contact ecosystem-infra@chromium.org if\n'
            'you wish to change them. Files:',
            items=idl_files)
    ]


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results += _LintWPT(input_api, output_api)
    # We only check IDL file changes on upload as there are occasionally valid
    # reasons to modify these files.
    results += _DontModifyIDLFiles(input_api, output_api)
    return results


def CheckChangeOnCommit(input_api, output_api):
    return _LintWPT(input_api, output_api)

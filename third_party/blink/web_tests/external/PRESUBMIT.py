# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for external/wpt.

See https://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os
import tempfile

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

    # If there are changes in web_tests/external that aren't in wpt, e.g.
    # changes to wpt_automation or this presubmit script, then we can return
    # to avoid running the linter on all files in wpt (which is slow).
    if not paths_in_wpt:
        return []

    # We have to set delete=False and then let the object go out of scope so
    # that the file can be opened by name on Windows.
    with tempfile.NamedTemporaryFile('w+', newline='', delete=False) as f:
        for path in paths_in_wpt:
            f.write('%s\n' % path)
        paths_name = f.name
    args = [
        python3_command(input_api),
        linter_path,
        'lint',
        '--repo-root=%s' % wpt_path,
        # To avoid false positives, do not lint files not upstreamed from
        # Chromium.
        '--ignore-glob=*-expected.txt',
        '--ignore-glob=*.ini',
        '--ignore-glob=*DIR_METADATA',
        '--ignore-glob=*OWNERS',
        '--ignore-glob=config.json',
        '--paths-file=%s' % paths_name,
    ]

    proc = input_api.subprocess.Popen(args,
                                      stdout=input_api.subprocess.PIPE,
                                      stderr=input_api.subprocess.PIPE)
    stdout, stderr = proc.communicate()
    os.remove(paths_name)

    if proc.returncode != 0:
        return [
            output_api.PresubmitError('wpt lint failed:',
                                      long_text=stdout + stderr)
        ]
    return []


def _DontModifyIDLFiles(input_api, output_api):
    """Detect manual modification of the generated files in interfaces/

    These files are generated automatically by reffy based on published web specs.
    Manual modifications will almost always just be reverted by reffy upstream,
    and this has historically caused developers bad surprises.

    See https://crbug.com/1016354
    """
    interfaces_path = input_api.os_path.join(input_api.PresubmitLocalPath(), 'wpt', 'interfaces')

    def is_generated_idl_file(f):
        abs_path = f.AbsoluteLocalPath()
        if not abs_path.endswith(
                input_api.os_path.relpath(abs_path, interfaces_path)):
            return False
        # WPT's tools/ci/interfaces_update.sh replaces all files that end in
        # .idl but do not end in .tentative.idl .
        return (abs_path.endswith(".idl")
                and not abs_path.endswith(".tentative.idl"))

    idl_files = [
        f.LocalPath()
        for f in input_api.AffectedSourceFiles(is_generated_idl_file)
    ]

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

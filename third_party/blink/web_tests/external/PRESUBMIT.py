# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for external/wpt.

See https://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


def _LintWPT(input_api, output_api):
    """Lint functionality encompassing web-platform-tests upstream.

    This is to catch lint errors that would otherwise be caught in WPT CI.
    See https://web-platform-tests.org/writing-tests/lint-tool.html for more
    information about the lint tool.
    """
    tools_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                        input_api.os_path.pardir,
                                        input_api.os_path.pardir, 'tools')
    if tools_path not in input_api.sys.path:
        input_api.sys.path.insert(0, tools_path)
    from blinkpy.presubmit.common_checks import lint_wpt_root
    wpt_path = input_api.os_path.join(input_api.PresubmitLocalPath(), 'wpt')
    return lint_wpt_root(input_api, output_api, wpt_path)


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

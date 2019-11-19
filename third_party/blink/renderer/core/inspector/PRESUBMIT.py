# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _CompileDevtoolsFrontend(input_api, output_api):
    # Need to get all affected files from change (not just within this subtree)
    local_paths = [f.AbsoluteLocalPath() for f in input_api.change.AffectedFiles()]
    devtools = input_api.os_path.realpath(
        input_api.os_path.join(input_api.PresubmitLocalPath(), "..", "..", "..", "..", "devtools-frontend", "src"))

    # If a devtools file is changed, the PRESUBMIT hook in Source/devtools
    # will run closure compiler
    if (any("browser_protocol.pdl" in path for path in local_paths) and
            all(devtools not in path for path in local_paths)):
        compile_path = input_api.os_path.join(
            input_api.PresubmitLocalPath(), "..", "..", "..", "..", "devtools-frontend", "src", "scripts", "test", "run_type_check.py")
        out, _ = input_api.subprocess.Popen(
            [input_api.python_executable, compile_path], stdout=input_api.subprocess.PIPE,
            stderr=input_api.subprocess.STDOUT).communicate()
        if "ERROR" in out or "WARNING" in out:
            return [output_api.PresubmitError(out)]
        if "NOTE" in out:
            return [output_api.PresubmitPromptWarning(out)]
    return []


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CompileDevtoolsFrontend(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    return []

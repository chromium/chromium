# Copyright (C) 2020 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Don't directly or indirectly import blinkpy/web_tests/ports/base.py in this
# script. When running on windows base.py tries to import win32pipe which may
# not be available on machines. Users who call 'git cl upload' run into this
# issue since the command is using python and not vpython. An example bug with
# more context is crbug.com/1103691.


def PresubmitCheckTestExpectations(input_api, output_api):
    os_path = input_api.os_path
    lint_path = os_path.join(os_path.dirname(os_path.abspath(__file__)),
                             os_path.pardir, os_path.pardir,
                             'lint_test_expectations.py')

    subproc = input_api.subprocess.Popen(
        [input_api.python3_executable, lint_path],
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE)
    errs = ''
    try:
        _, errs = subproc.communicate(timeout=300)
        errs = errs.decode('utf-8')
    finally:
        subproc.kill()

    if subproc.returncode == 0:  # Lint succeeded.
        return []
    elif subproc.returncode == 2:  # Lint succeeded with warnings.
        return [output_api.PresubmitPromptWarning(errs)]
    # Lint failed in some way.
    return [output_api.PresubmitError(errs)]

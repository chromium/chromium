# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for gpu.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import sys

def CommonChecks(input_api, output_api):

  output = []
  sys_path_backup = sys.path
  try:
    sys.path = [
        input_api.PresubmitLocalPath()
    ] + sys.path
    disabled_warnings = [
        'anomalous-backslash-in-string',
        'bad-indentation',
        'consider-using-dict-items',
        'consider-using-generator',
        'consider-using-min-builtin',
        'consider-using-with',
        'deprecated-module',
        'duplicate-code',
        'function-redefined',
        'line-too-long',
        'missing-module-docstring',
        'protected-access',
        'singleton-comparison',
        'superfluous-parens',
        'undefined-variable',
        'unnecessary-lambda-assignment',
        'unnecessary-negation',
        'unnecessary-semicolon',
        'unspecified-encoding',
        'unused-import',
        'use-dict-literal',
        'using-constant-test',
    ]
    pylint_checks = input_api.canned_checks.GetPylint(
        input_api,
        output_api,
        disabled_warnings=disabled_warnings,
        version='3.2')
    output.extend(input_api.RunTests(pylint_checks))
  finally:
    sys.path = sys_path_backup

  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)

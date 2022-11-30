#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A binary to enable mocking of binaries.

This enables writing tests to verify that a binary was invoked as well
as controlling the output and exit code of the binary. Tests should
copy or link to this binary so that invocations for different binaries
can be distinguished.

INVOCATIONS_FILE must be set to the location of the file to record
invocations to. The format of the recorded invocations file will be a
json list where each element is the sys.argv for the invocation. After
running the code under test, the test can load the contents of the file
and verify that the expected invocations occurred.

Tests can control the output, error output and exit codes of the
invocations via the environment variable MOCK_DETAILS. If set,
MOCK_DETAILS must be a string containing a json-encoding mapping from
the binary path used for the invocation to a mapping containing any or
all of the following entries:
* stdout -> string containing the program's stdout
* stderr -> string containing the program's stderr
* exit_code -> integer containing the program's exit code
If both 'stdout' and 'stderr' are present in the mapping for a binary
path, the stdout will be output first, no interleaving is supported.
"""

import json
import os
import sys

invocations_file = os.environ['INVOCATIONS_FILE']

if not os.path.exists(invocations_file):
  invocations = []
else:
  with open(invocations_file) as f:
    invocations = json.load(f)

invocations.append(sys.argv)

with open(invocations_file, 'w') as f:
  json.dump(invocations, f)

mock_details = json.loads(os.environ.get('MOCK_DETAILS', '{}'))
mock_details = mock_details.get(sys.argv[0], {})

mock_stdout = mock_details.get('stdout')
if mock_stdout is not None:
  print(mock_stdout)
  sys.stdout.flush()

mock_stderr = mock_details.get('stderr')
if mock_stderr is not None:
  print(mock_stderr, file=sys.stderr)

mock_exit_code = mock_details.get('exit_code')
if mock_exit_code is not None:
  sys.exit(mock_exit_code)

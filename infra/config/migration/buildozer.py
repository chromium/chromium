# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module providing utilities for running buildozer."""

import pathlib
import subprocess
import sys

_THIS_DIR = pathlib.Path(__file__).parent

# Override the buildozer tables with empty tables to avoid buildozer making
# unintended changes such as sorting most lists, including in existing portions
# of the file
_TABLE_JSON_FILE = _THIS_DIR / 'buildozer-tables.json'


def run(*args: str) -> str:
  """Run buildozer.

  buildozer is run with the provided arguments, with the stdout being
  captured. In the case of an error, the process' stderr will be written
  to stderr (exit code 3 indicates success with no changes and isn't
  treated as an error).

  Args:
    args: The commands and targets to buildozer.

  Returns:
    The stdout of the buildozer process.

  Raises:
    subprocess.CalledProcessError if there is an error running
    buildozer.
  """
  # output is always captured to avoid having each edit trigger a repetitive
  # line of output
  ret = subprocess.run(['buildozer', '-tables', _TABLE_JSON_FILE, *args],
                       capture_output=True,
                       encoding='utf-8')
  # buildozer returns exit code of 3 when it makes no changes, the edits should
  # be idempotent, so we have to manually check the return code
  if ret.returncode not in (0, 3):
    sys.stderr.write(ret.stderr)
    ret.check_returncode()
  return ret.stdout

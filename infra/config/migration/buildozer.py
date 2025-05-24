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
_TABLES_JSON_FILE = _THIS_DIR / 'buildozer-tables.json'


def _run(*args: str) -> subprocess.CompletedProcess:
  # output is always captured to avoid having each edit trigger a repetitive
  # line of output
  return subprocess.run(
      [
          'buildozer',
          # Use empty tables
          f'-tables={_TABLES_JSON_FILE}',
          # Add comments above the element being commented instead of at the end
          # of the line
          '-eol-comments=false',
          *args,
      ],
      capture_output=True,
      encoding='utf-8',
  )


def run(*args: str) -> str:
  """Run buildozer.

  buildozer is run with the provided arguments, with the output being
  captured. In the case of a usage error (exit code 1) or command
  failure (exit code 2), the process' stderr will be written to stderr.
  Exit code 3 indicates success with no changes and isn't treated as an
  error.

  Args:
    args: The commands and targets to buildozer.

  Returns:
    The stdout of the buildozer process.

  Raises:
    subprocess.CalledProcessError if there is a usage error or command
    failure when running buildozer.
  """
  ret = _run(*args)
  # buildozer returns exit code of 3 when it makes no changes, the edits should
  # be idempotent, so we have to manually check the return code
  if ret.returncode not in (0, 3):
    sys.stderr.write(ret.stderr)
    ret.check_returncode()
  return ret.stdout


def try_run(*args: str) -> bool:
  """Run buildozer, allowing for command failure.

  buildozer is run with the provided arguments, with the output being
  captured. In the case of a usage error (exit code 1), the process'
  stderr will be written to stderr. A command failure (exit code 2) will
  be reported via a False return value, with no output be written to
  stdout or stderr. Exit code 3 indicates success with no changes and
  isn't treated as an error.

  Args:
    args: The commands and targets to buildozer.

  Returns:
    Whether or not the buildozer commands succeeded.

  Raises:
    subprocess.CalledProcessError if there is a usage error when running
    buildozer.
  """
  ret = _run(*args)
  # buildozer returns exit code 2 when a command fails (e.g the target doesn't
  # exist), as opposed to a usage error (e.g not enough arguments to a command)
  # which returns 1. The caller is expecting that the command might fail, so
  # don't raise an exception, instead just return False to indicate the failure.
  if ret.returncode == 2:
    sys.stderr.write(ret.stderr)
    return False
  # buildozer returns exit code of 3 when it makes no changes, the edits should
  # be idempotent, so we have to manually check the return code
  if ret.returncode not in (0, 3):
    sys.stderr.write(ret.stderr)
    ret.check_returncode()
  return True

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for invoking executables.
"""

import subprocess
import re
import sys

# Regex for matching 7-bit and 8-bit C1 ANSI sequences.
# Credit: https://stackoverflow.com/a/14693789/4692014
_ANSI_ESCAPE_8BIT_REGEX = re.compile(
    r"""
    (?: # either 7-bit C1, two bytes, ESC Fe (omitting CSI)
        \x1B
        [@-Z\\-_]
    |   # or a single 8-bit byte Fe (omitting CSI)
        [\x80-\x9A\x9C-\x9F]
    |   # or CSI + control codes
        (?: # 7-bit CSI, ESC [
            \x1B\[
        |   # 8-bit CSI, 9B
            \x9B
        )
        [0-?]*  # Parameter bytes
        [ -/]*  # Intermediate bytes
        [@-~]   # Final byte
    )
""", re.VERBOSE)


def run_and_tee_output(args):
    """Runs the test executable passing-thru its output to stdout (in a
    terminal-colors-friendly way).  Waits for the executable to exit.

    Returns:
        The full executable output as an UTF-8 string.
    """
    # Capture stdout (where test results are written), but inherit stderr. This
    # way errors related to invalid arguments are printed normally.
    proc = subprocess.Popen(args, stdout=subprocess.PIPE)
    captured_output = b''
    while proc.poll() is None:
        buf = proc.stdout.read()
        # Write captured output directly, so escape sequences are preserved.
        sys.stdout.buffer.write(buf)
        captured_output += buf

    captured_output = _ANSI_ESCAPE_8BIT_REGEX.sub(
        '', captured_output.decode('utf-8'))

    return captured_output

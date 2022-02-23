# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for invoking executables.
"""

import os
import pty
import re

# Regex for matching 7-bit and 8-bit C1 ANSI sequences.
# Credit: https://stackoverflow.com/a/14693789/4692014
_ANSI_ESCAPE_8BIT_REGEX = re.compile(
    br"""
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
    output_bytes = bytearray()

    def read(fd):
        data = os.read(fd, 1024)
        output_bytes.extend(data)
        return data

    pty.spawn(args, read)

    # Strip ANSI / terminal escapes.
    output_bytes = _ANSI_ESCAPE_8BIT_REGEX.sub(b'', output_bytes)

    return output_bytes.decode('utf-8')

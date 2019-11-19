# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import subprocess

_clang_format_command_path = None


def init(command_path):
    """
    Args:
        command_path: Path to the clang_format command.
    """
    assert isinstance(command_path, str)
    assert os.path.exists(command_path)

    global _clang_format_command_path
    assert _clang_format_command_path is None
    _clang_format_command_path = command_path


def clang_format(contents, filename=None):
    command_line = [_clang_format_command_path]
    if filename is not None:
        command_line.append('-assume-filename={}'.format(filename))

    proc = subprocess.Popen(
        command_line, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    stdout_output, stderr_output = proc.communicate(input=contents)
    exit_code = proc.wait()

    return ClangFormatResult(
        stdout_output=stdout_output,
        stderr_output=stderr_output,
        exit_code=exit_code,
        filename=filename)


class ClangFormatResult(object):
    def __init__(self, stdout_output, stderr_output, exit_code, filename):
        self._stdout_output = stdout_output
        self._stderr_output = stderr_output
        self._exit_code = exit_code
        self._filename = filename

    @property
    def did_succeed(self):
        return self._exit_code == 0

    @property
    def contents(self):
        assert self.did_succeed
        return self._stdout_output

    @property
    def error_message(self):
        return self._stderr_output

    @property
    def filename(self):
        return self._filename

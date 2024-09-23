# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import platform as platform_module
import subprocess
import sys

_enable_style_format = None
_clang_format_command_path = None
_gn_command_path = None


def init(root_src_dir, enable_style_format=True):
    assert isinstance(root_src_dir, str)
    assert isinstance(enable_style_format, bool)

    global _enable_style_format
    global _clang_format_command_path
    global _gn_command_path

    assert _enable_style_format is None
    assert _clang_format_command_path is None
    assert _gn_command_path is None

    _enable_style_format = enable_style_format

    root_src_dir = os.path.abspath(root_src_dir)

    # Determine //buildtools/<platform>/ directory
    new_path_platform_suffix = ""
    if sys.platform.startswith("linux"):
        platform = "linux64"
        exe_suffix = ""
    elif sys.platform.startswith("darwin"):
        platform = "mac"
        exe_suffix = ""
        host_arch = platform_module.machine().lower()
        if host_arch == "arm64" or host_arch.startswith("aarch64"):
            new_path_platform_suffix = "_arm64"
    elif sys.platform.startswith(("cygwin", "win")):
        platform = "win"
        exe_suffix = ".exe"
    else:
        assert False, "Unknown platform: {}".format(sys.platform)
    buildtools_platform_dir = os.path.join(root_src_dir, "buildtools",
                                           platform)
    new_buildtools_platform_dir = os.path.join(
        root_src_dir, "buildtools", platform + new_path_platform_suffix)

    # TODO(b/328065301): Remove old paths once clang hooks are migrated
    # //buildtools/<platform>/clang-format
    possible_paths = [
        os.path.join(buildtools_platform_dir,
                     "clang-format{}".format(exe_suffix)),
        # //buildtools/<platform>/format/clang-format
        os.path.join(new_buildtools_platform_dir, "format",
                     "clang-format{}".format(exe_suffix)),
        # //buildtools/<platform>-format/clang-format
        os.path.join(f"{new_buildtools_platform_dir}-format",
                     "clang-format{}".format(exe_suffix)),
    ]
    for path in possible_paths:
        if os.path.isfile(path):
            _clang_format_command_path = path

    # //buildtools/<platform>/gn
    _gn_command_path = os.path.join(buildtools_platform_dir,
                                    "gn{}".format(exe_suffix))


def auto_format(contents, filename):
    assert isinstance(filename, str)

    _, ext = os.path.splitext(filename)
    if ext in (".gn", ".gni"):
        return gn_format(contents, filename)

    return clang_format(contents, filename)


def clang_format(contents, filename=None):
    command_line = [_clang_format_command_path]
    if filename is not None:
        command_line.append('-assume-filename={}'.format(filename))

    return _invoke_format_command(command_line, filename, contents)


def gn_format(contents, filename=None):
    command_line = [_gn_command_path, "format", "--stdin"]
    if filename is not None:
        command_line.append('-assume-filename={}'.format(filename))

    return _invoke_format_command(command_line, filename, contents)


def _invoke_format_command(command_line, filename, contents):
    if not _enable_style_format:
        return StyleFormatResult(stdout_output=contents,
                                 stderr_output="",
                                 exit_code=0,
                                 filename=filename)

    kwargs = {}
    if sys.version_info.major != 2:
        kwargs['encoding'] = 'utf-8'
    proc = subprocess.Popen(command_line,
                            stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE,
                            **kwargs)
    stdout_output, stderr_output = proc.communicate(input=contents)
    exit_code = proc.wait()

    return StyleFormatResult(
        stdout_output=stdout_output,
        stderr_output=stderr_output,
        exit_code=exit_code,
        filename=filename)


class StyleFormatResult(object):
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

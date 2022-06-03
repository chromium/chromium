# Copyright (C) 2013 Google Inc. All rights reserved.
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

import errno
import logging
import os

from blinkpy.web_tests.breakpad.dump_reader import DumpReader

_log = logging.getLogger(__name__)


class DumpReaderWin(DumpReader):
    """DumpReader for windows breakpad."""

    def __init__(self, host, build_dir):
        super(DumpReaderWin, self).__init__(host, build_dir)
        self._cdb_available = None

    def check_is_functional(self):
        return self._check_cdb_available()

    def crash_dumps_directory(self):
        path = super(DumpReaderWin, self).crash_dumps_directory()
        # Crashpad stores finished crash report files in a subdirectory.
        # TODO(crbug.com/782923): This test should not reach directly into the
        # Crashpad database, but instead should use crashpad_database_util.
        return self._host.filesystem.join(path, 'reports')

    def _file_extension(self):
        return 'txt'

    def _get_pid_from_dump(self, dump_file):
        with self._host.filesystem.open_text_file_for_reading(dump_file) as f:
            crash_keys = dict([l.split(':', 1) for l in f.read().splitlines()])
            if 'pid' in crash_keys:
                return crash_keys['pid']
        return None

    def _get_stack_from_dump(self, dump_file):
        minidump = dump_file[:-3] + 'dmp'
        cmd = [
            self._cdb_path, '-y', self._build_dir, '-c', '.lines;.ecxr;k30;q',
            '-z', minidump
        ]
        try:
            stack = self._host.executive.run_command(cmd)
        except:
            _log.warning('Failed to execute "%s"', ' '.join(cmd))
        else:
            return stack
        return None

    def _find_depot_tools_path(self):
        """Attempt to find depot_tools location in PATH."""
        for i in self._host.environ.get('PATH').split(os.pathsep):
            if os.path.isfile(os.path.join(i, 'gclient')):
                return i

    def _check_cdb_available(self):
        """Checks whether we can use cdb to symbolize minidumps."""
        if self._cdb_available is not None:
            return self._cdb_available

        cdb = self._host.filesystem.join(self._build_dir, 'cdb', 'cdb.exe')
        try:
            _ = self._host.executive.run_command([cdb, '-version'])
        except:
            pass
        else:
            self._cdb_path = cdb
            self._cdb_available = True
            return self._cdb_available

        _log.warning("CDB is not installed; can't symbolize minidumps.")
        _log.warning('')
        self._cdb_available = False
        return self._cdb_available

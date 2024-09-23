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

import cgi
import logging
import threading

from six.moves import queue as Queue

from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.breakpad.dump_reader import DumpReader

_log = logging.getLogger(__name__)


class DumpReaderMultipart(DumpReader):
    """Base class for Linux and Android breakpad dump reader."""

    def __init__(self, host, build_dir):
        super(DumpReaderMultipart, self).__init__(host, build_dir)
        self._path_finder = PathFinder(host.filesystem)
        self._breakpad_tools_available = None
        self._generated_symbols = False

    def check_is_functional(self):
        return self._check_breakpad_tools_available()

    def _get_pid_from_dump(self, dump_file):
        dump = self._read_dump(dump_file)
        if not dump:
            return None
        if 'pid' in dump:
            return dump['pid'][0]
        return None

    def _get_stack_from_dump(self, dump_file):
        dump = self._read_dump(dump_file)
        if not dump:
            return None
        if not 'upload_file_minidump' in dump:
            return None

        self._generate_breakpad_symbols_if_necessary()
        f, temp_name = self._host.filesystem.open_binary_tempfile('dmp')
        f.write(b'\r\n'.join(dump['upload_file_minidump']))
        f.close()

        cmd = [
            self._path_to_minidump_stackwalk(), temp_name,
            self._symbols_dir()
        ]
        try:
            stack = self._host.executive.run_command(cmd, stderr=None)
        except:
            _log.warning('Failed to execute "%s"', ' '.join(cmd))
            stack = None
        finally:
            self._host.filesystem.remove(temp_name)
        return stack

    def _read_dump(self, dump_file):
        with self._host.filesystem.open_binary_file_for_reading(
                dump_file) as f:
            boundary = f.readline().strip()[2:]
            f.seek(0)
            try:
                data = cgi.parse_multipart(f, {'boundary': boundary})
                return data
            except:
                pass
        return None

    def _check_breakpad_tools_available(self):
        if self._breakpad_tools_available is not None:
            return self._breakpad_tools_available

        REQUIRED_BREAKPAD_TOOLS = [
            'dump_syms',
            'minidump_stackwalk',
        ]
        result = True
        for binary in REQUIRED_BREAKPAD_TOOLS:
            full_path = self._host.filesystem.join(self._build_dir, binary)
            if not self._host.filesystem.exists(full_path):
                result = False
                _log.error('Unable to find %s', binary)
                _log.error('    at %s', full_path)

        if not result:
            _log.error(
                "    Could not find breakpad tools, unexpected crashes won't be symbolized"
            )
            _log.error('    Did you build the target blink_tests?')
            _log.error('')

        self._breakpad_tools_available = result
        return self._breakpad_tools_available

    def _path_to_minidump_stackwalk(self):
        return self._host.filesystem.join(self._build_dir,
                                          'minidump_stackwalk')

    def _path_to_generate_breakpad_symbols(self):
        return self._path_finder.path_from_chromium_base(
            'components', 'crash', 'content', 'tools',
            'generate_breakpad_symbols.py')

    def _symbols_dir(self):
        return self._host.filesystem.join(self._build_dir,
                                          'content_shell.syms')

    def _generate_breakpad_symbols_if_necessary(self):
        if self._generated_symbols:
            return
        self._generated_symbols = True

        _log.debug('Generating breakpad symbols')
        queue = Queue.Queue()
        thread = threading.Thread(target=_symbolize_keepalive, args=(queue, ))
        thread.start()
        try:
            for binary in self._binaries_to_symbolize():
                _log.debug('  Symbolizing %s', binary)
                full_path = self._host.filesystem.join(self._build_dir, binary)
                cmd = [
                    self._path_to_generate_breakpad_symbols(),
                    '--binary=%s' % full_path,
                    '--symbols-dir=%s' % self._symbols_dir(),
                    '--build-dir=%s' % self._build_dir,
                ]
                try:
                    self._host.executive.run_command(cmd)
                except:
                    _log.error('Failed to execute "%s"', ' '.join(cmd))
        finally:
            queue.put(None)
            thread.join()
        _log.debug('Done generating breakpad symbols')

    def _binaries_to_symbolize(self):
        """This routine must be implemented by subclasses.

        Returns an array of binaries that need to be symbolized.
        """
        raise NotImplementedError()


def _symbolize_keepalive(queue):
    while True:
        _log.debug('waiting for symbolize to complete')
        try:
            queue.get(block=True, timeout=60)
            return
        except Queue.Empty:
            pass


class DumpReaderLinux(DumpReaderMultipart):
    """Linux breakpad dump reader."""

    def _binaries_to_symbolize(self):
        return ['content_shell']

    def _file_extension(self):
        return 'dmp'


class DumpReaderAndroid(DumpReaderMultipart):
    """Android breakpad dump reader."""

    def _binaries_to_symbolize(self):
        return ['lib/libcontent_shell_content_view.so']

    def _file_extension(self):
        return 'dmp'

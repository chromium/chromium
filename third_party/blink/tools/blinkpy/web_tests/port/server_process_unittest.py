# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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
import platform
import sys
import time
import unittest
from unittest import mock

from blinkpy.common.system.system_host import SystemHost
from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.port import server_process
from blinkpy.web_tests.port.factory import PortFactory


class TrivialMockPort(object):
    def __init__(self):
        self.host = MockSystemHost()
        self.host.executive.kill_process = lambda x: None
        self.host.executive.kill_process = lambda x: None

    def results_directory(self):
        return "/mock-results"


class MockFile(object):
    def __init__(self, server_process):
        self._server_process = server_process
        self.closed = False

    def fileno(self):
        return 1

    def write(self, line):
        self._server_process.broken_pipes.append(self)
        raise IOError

    def read(self):
        return ''

    def close(self):
        self.closed = True


class MockProc(object):
    def __init__(self, server_process):
        self.stdin = MockFile(server_process)
        self.stdout = MockFile(server_process)
        self.stderr = MockFile(server_process)
        self.pid = 1
        self.returncode = 1

    def poll(self):
        return self.returncode

    def wait(self):
        return 0


class FakeServerProcess(server_process.ServerProcess):
    def _start(self):
        self._proc = MockProc(self)
        self.stdin = self._proc.stdin
        self.stdout = self._proc.stdout
        self.stderr = self._proc.stderr
        self._pid = self._proc.pid
        self.broken_pipes = []

    def _non_blocking_read_win32(self, handle):
        # MockFile.fileno() returns fd 1 (the test runner's write-only stdout),
        # so avoid calling win32pipe.PeekNamedPipe on it when running on
        # Windows.
        return None


class TestServerProcess(unittest.TestCase):
    @unittest.skipIf(
        platform.mac_ver()[0].startswith('12'),
        "Failing on macOS 12; see crbug.com/474036848",
    )
    def test_basic(self):
        cmd = [
            sys.executable,
            '-c',
            'import sys; import time; time.sleep(0.02); print "stdout"; sys.stdout.flush(); print >>sys.stderr, "stderr"',
        ]
        host = SystemHost()
        factory = PortFactory(host)
        port = factory.get()
        now = time.time()
        proc = server_process.ServerProcess(port, 'python', cmd)
        proc.write(b'')

        self.assertIsNone(proc.poll())
        self.assertFalse(proc.has_crashed())

        # check that doing a read after an expired deadline returns
        # nothing immediately.
        line = proc.read_stdout_line(now - 1)
        self.assertIsNone(line)

        # FIXME: This part appears to be flaky. line should always be non-None.
        # FIXME: https://bugs.webkit.org/show_bug.cgi?id=88280
        line = proc.read_stdout_line(now + 1.0)
        if line:
            self.assertEqual(line.strip(), "stdout")

        line = proc.read_stderr_line(now + 1.0)
        if line:
            self.assertEqual(line.strip(), "stderr")

        proc.stop(0)

    def test_cleanup(self):
        port_obj = TrivialMockPort()
        server_process = FakeServerProcess(
            port_obj=port_obj, name="test", cmd=["test"]
        )
        server_process._start()
        server_process.stop()
        self.assertTrue(server_process.stdin.closed)
        self.assertTrue(server_process.stdout.closed)
        self.assertTrue(server_process.stderr.closed)

    def test_broken_pipe(self):
        port_obj = TrivialMockPort()

        port_obj.host.platform.os_name = 'win'
        server_process = FakeServerProcess(
            port_obj=port_obj, name="test", cmd=["test"]
        )
        server_process.write(b"should break")
        self.assertTrue(server_process.has_crashed())
        self.assertIsNotNone(server_process.pid())
        self.assertIsNone(server_process._proc)
        self.assertEqual(server_process.broken_pipes, [server_process.stdin])

        port_obj.host.platform.os_name = 'mac'
        server_process = FakeServerProcess(
            port_obj=port_obj, name="test", cmd=["test"]
        )
        server_process.write(b"should break")
        self.assertTrue(server_process.has_crashed())
        self.assertIsNone(server_process._proc)
        self.assertEqual(server_process.broken_pipes, [server_process.stdin])

    def test_broken_pipe_einval_on_close(self):
        port_obj = TrivialMockPort()
        port_obj.host.platform.os_name = 'win'

        def raise_einval():
            raise OSError(errno.EINVAL, 'Invalid argument')

        proc = FakeServerProcess(port_obj=port_obj, name='test', cmd=['test'])
        proc._start()
        proc._proc.stdin.close = raise_einval

        # Writing to a dead pipe calls stop(0.0), which closes stdin; on Windows
        # Python 3 this can raise OSError(EINVAL) instead of BrokenPipeError.
        proc.write(b'should break')
        self.assertTrue(proc.has_crashed())
        self.assertIsNone(proc._proc)

        # _reset() should also tolerate OSError(EINVAL) when closing stdin.
        proc._start()
        proc._proc.stdin.close = raise_einval
        proc._reset()
        self.assertIsNone(proc._proc)

        # Unrelated OSErrors should still propagate.
        def raise_eio():
            raise OSError(errno.EIO, 'I/O error')

        proc._start()
        proc._proc.stdin.close = raise_eio
        with self.assertRaises(OSError) as cm:
            proc.stop(0.0)
        self.assertEqual(cm.exception.errno, errno.EIO)

    def test_win32_drains_pipes_after_process_exit(self):
        port_obj = TrivialMockPort()
        port_obj.host.platform.os_name = 'win'
        proc = FakeServerProcess(port_obj=port_obj, name='test', cmd=['test'])
        proc._use_win32_apis = True
        proc._start()
        # Fake stdout/stderr handles so we can distinguish stdout vs stderr.
        proc._proc.stdout.fileno = lambda: 10
        proc._proc.stderr.fileno = lambda: 20

        pipe_data = {
            10: [b'final stdout\n', None],
            20: [b'fatal crash stderr\n', None],
        }

        def fake_read(handle):
            return pipe_data[handle].pop(0) if pipe_data[handle] else None

        fake_msvcrt = mock.Mock()
        fake_msvcrt.get_osfhandle.side_effect = lambda fd: fd

        with (
            mock.patch.object(
                server_process, 'msvcrt', fake_msvcrt, create=True
            ),
            mock.patch.object(
                proc, '_non_blocking_read_win32', side_effect=fake_read
            ),
        ):
            # Even though MockProc.poll() is already 1 (exited), _read() should
            # drain buffered pipe output before reporting the crash.
            self.assertIsNone(proc.read_stdout_line(time.time() + 1.0))
            self.assertTrue(proc.has_crashed())
            self.assertEqual(
                proc.pop_all_buffered_stderr(), b'fatal crash stderr\n'
            )

            # stop() with an already-expired deadline should also drain any
            # remaining pipe bytes on an exited process.
            pipe_data[10] = [b'trailing out']
            pipe_data[20] = [b'trailing err']
            out, err = proc.stop(0.0)
            self.assertEqual(out, b'final stdout\ntrailing out')
            self.assertEqual(err, b'trailing err')


class TestQuoteData(unittest.TestCase):
    def test_plain(self):
        qd = server_process.quote_data
        self.assertEqual(qd("foo"), ["foo"])

    def test_trailing_spaces(self):
        qd = server_process.quote_data
        self.assertEqual(qd("foo  "), ["foo\x20\x20"])

    def test_newlines(self):
        qd = server_process.quote_data
        self.assertEqual(qd("foo \nbar\n"), ["foo\x20\\n", "bar\\n"])

    def test_binary_data(self):
        qd = server_process.quote_data
        self.assertEqual(qd("\x00\x01ab"), ["\\x00\\x01ab"])

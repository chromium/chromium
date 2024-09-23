# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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

import csv
import ctypes
import errno
import logging
import multiprocessing
import os
import signal
import subprocess
import sys
import time

import six

_log = logging.getLogger(__name__)


class ScriptError(Exception):
    def __init__(self,
                 message=None,
                 script_args=None,
                 exit_code=None,
                 output=None,
                 cwd=None,
                 output_limit=500):
        shortened_output = output
        if output and output_limit and len(output) > output_limit:
            shortened_output = 'Last %s characters of output:\n%s' % (
                output_limit, output[-output_limit:])

        if not message:
            message = 'Failed to run "%s"' % repr(script_args)
            if exit_code:
                message += ' exit_code: %d' % exit_code
            if cwd:
                message += ' cwd: %s' % cwd

        if shortened_output:
            message += '\n\noutput: %s' % shortened_output

        Exception.__init__(self, message)
        if six.PY3:
            self.message = message
        self.script_args = script_args  # 'args' is already used by Exception
        self.exit_code = exit_code
        self.output = output
        self.cwd = cwd

    def message_with_output(self):
        return six.text_type(self)

    def command_name(self):
        command_path = self.script_args
        if isinstance(command_path, list):
            command_path = command_path[0]
        return os.path.basename(command_path)


class Executive:
    PIPE = subprocess.PIPE
    STDOUT = subprocess.STDOUT
    DEVNULL = subprocess.DEVNULL

    def __init__(self, error_output_limit=500):
        """Args:
            error_output_limit: The maximum length of output included in the
                message of ScriptError when run_command sees a non-zero exit
                code. None means no limit.
        """
        self.error_output_limit = error_output_limit

    def _should_close_fds(self):
        # We need to pass close_fds=True to work around Python bug #2320
        # (otherwise we can hang when we kill DumpRenderTree when we are running
        # multiple threads). See http://bugs.python.org/issue2320 .
        # Note that close_fds isn't supported on Windows, but this bug only
        # shows up on Mac and Linux.
        return sys.platform != 'win32'

    def cpu_count(self):
        cpu_count = multiprocessing.cpu_count()
        if sys.platform == 'win32':
            # TODO(crbug.com/1190269) - we can't use more than 56
            # cores on Windows or Python3 may hang.
            cpu_count = min(cpu_count, 56)
        return cpu_count

    def kill_process(self, pid, kill_tree=True):
        """Attempts to kill the given pid.

        if kill_tree is True, the whole process group will be killed.

        Will fail silently if pid does not exist.
        """
        if sys.platform == 'win32':
            # Workaround for race condition that occurs when the browser is
            # killed as it's launching a process. This sometimes leaves a child
            # process that is in a suspended state.
            # Follow the Win32 API naming style. pylint: disable=invalid-name
            OpenProcess = ctypes.windll.kernel32.OpenProcess
            CloseHandle = ctypes.windll.kernel32.CloseHandle
            NtSuspendProcess = ctypes.windll.ntdll.NtSuspendProcess
            PROCESS_ALL_ACCESS = 0x1F0FFF
            process_handle = OpenProcess(PROCESS_ALL_ACCESS, False, pid)
            if process_handle != 0:
                NtSuspendProcess(process_handle)
                CloseHandle(process_handle)

            command = ['taskkill.exe', '/f']
            if kill_tree:
                command.append('/t')
            command += ['/pid', pid]
            # taskkill will exit 128 if the process is not found. We should log.
            self.run_command(command, error_handler=self.log_error)
            return

        try:
            if kill_tree:
                os.killpg(os.getpgid(pid), signal.SIGKILL)
            else:
                os.kill(pid, signal.SIGKILL)
            # At this point if no exception has been raised, the kill has
            # succeeded, so we can safely use a blocking wait.
            os.waitpid(pid, 0)
        except OSError as error:
            if error.errno == errno.ESRCH:
                _log.debug("PID %s does not exist.", pid)
                return
            if error.errno == errno.ECHILD:
                # Can't wait on a non-child process, but the kill worked.
                return
            if error.errno == errno.EPERM and \
                    kill_tree and sys.platform == 'darwin':
                # Calling killpg on a process group whose leader is defunct
                # causes a permission error on macOS, in which case we try to
                # collect the defunct process.
                if os.waitpid(pid, os.WNOHANG) == (0, 0):
                    return
            raise

    def _win32_check_running_pid(self, pid):
        class PROCESSENTRY32(ctypes.Structure):
            _fields_ = [('dwSize', ctypes.c_ulong), ('cntUsage',
                                                     ctypes.c_ulong),
                        ('th32ProcessID', ctypes.c_ulong),
                        ('th32DefaultHeapID', ctypes.POINTER(ctypes.c_ulong)),
                        ('th32ModuleID', ctypes.c_ulong),
                        ('cntThreads', ctypes.c_ulong),
                        ('th32ParentProcessID', ctypes.c_ulong),
                        ('pcPriClassBase', ctypes.c_ulong),
                        ('dwFlags', ctypes.c_ulong),
                        ('szExeFile', ctypes.c_char * 260)]

        # Follow the Win32 API naming style. pylint: disable=invalid-name
        CreateToolhelp32Snapshot = ctypes.windll.kernel32.CreateToolhelp32Snapshot
        Process32First = ctypes.windll.kernel32.Process32First
        Process32Next = ctypes.windll.kernel32.Process32Next
        CloseHandle = ctypes.windll.kernel32.CloseHandle
        TH32CS_SNAPPROCESS = 0x00000002  # win32 magic number
        hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
        # pylint does not understand ctypes. pylint: disable=attribute-defined-outside-init
        pe32 = PROCESSENTRY32()
        pe32.dwSize = ctypes.sizeof(PROCESSENTRY32)
        result = False
        if not Process32First(hProcessSnap, ctypes.byref(pe32)):
            _log.debug('Failed getting first process.')
            CloseHandle(hProcessSnap)
            return result
        while True:
            if pe32.th32ProcessID == pid:
                result = True
                break
            if not Process32Next(hProcessSnap, ctypes.byref(pe32)):
                break
        CloseHandle(hProcessSnap)
        return result

    def check_running_pid(self, pid):
        """Return True if pid is alive, otherwise return False."""
        _log.debug('Checking whether pid %d is alive.', pid)
        if sys.platform == 'win32':
            return self._win32_check_running_pid(pid)

        try:
            os.kill(pid, 0)
            return True
        except OSError:
            return False

    def _running_processes(self):
        processes = []
        if sys.platform == 'win32':
            tasklist_process = self.popen(['tasklist', '/fo', 'csv'],
                                          stdout=self.PIPE,
                                          stderr=self.PIPE)
            stdout, _ = tasklist_process.communicate()
            stdout_reader = csv.reader(
                stdout.decode('utf8', 'replace').splitlines())
            for line in stdout_reader:
                processes.append([column for column in line])
        else:
            ps_process = self.popen(['ps', '-eo', 'pid,comm'],
                                    stdout=self.PIPE,
                                    stderr=self.PIPE)
            stdout, _ = ps_process.communicate()
            for line in stdout.splitlines():
                # In some cases the line can contain one or more
                # leading white-spaces, so strip it before split.
                pid, process_name = line.strip().split(b' ', 1)
                processes.append([process_name, pid])
        return processes

    def running_pids(self, process_name_filter=None):
        if not process_name_filter:
            process_name_filter = lambda process_name: True

        running_pids = []
        for line in self._running_processes():
            try:
                process_name = line[0]
                pid = line[1]
                if process_name_filter(process_name):
                    running_pids.append(int(pid))
            except (ValueError, IndexError):
                pass

        return sorted(running_pids)

    def wait_limited(self,
                     pid,
                     limit_in_seconds=None,
                     check_frequency_in_seconds=None):
        seconds_left = limit_in_seconds or 10
        sleep_length = check_frequency_in_seconds or 1
        while seconds_left > 0 and self.check_running_pid(pid):
            seconds_left -= sleep_length
            time.sleep(sleep_length)

    def interrupt(self, pid):
        interrupt_signal = signal.SIGINT
        # Note: The python docs seem to suggest that on Windows, we may want to use
        # signal.CTRL_C_EVENT (http://docs.python.org/2/library/signal.html), but
        # it appears that signal.SIGINT also appears to work on Windows.
        try:
            os.kill(pid, interrupt_signal)
        except OSError:
            # Silently ignore when the pid doesn't exist.
            # It's impossible for callers to avoid race conditions with process shutdown.
            pass

    def terminate(self, pid):
        try:
            os.kill(pid, signal.SIGTERM)
        except OSError:
            # Silently ignore when the pid doesn't exist.
            pass

    # Error handlers do not need to be static methods once all callers are
    # updated to use an Executive object.

    @staticmethod
    def default_error_handler(error):
        raise error

    @staticmethod
    def ignore_error(error):
        pass

    @staticmethod
    def log_error(error):
        _log.debug(error)

    def _compute_stdin(self, user_input):
        """Returns (stdin, string_to_communicate)"""
        # FIXME: We should be returning /dev/null for stdin
        # or closing stdin after process creation to prevent
        # child processes from getting input from the user.
        if not user_input:
            return (None, None)
        if hasattr(user_input, 'read'):  # Check if the user_input is a file.
            # Assume the file is in the right encoding.
            return (user_input, None)

        # Popen in Python 2.5 and before does not automatically encode unicode objects.
        # http://bugs.python.org/issue5290
        # See https://bugs.webkit.org/show_bug.cgi?id=37528
        # for an example of a regression caused by passing a unicode string directly.
        # FIXME: We may need to encode differently on different platforms.
        if isinstance(user_input, six.text_type):
            user_input = user_input.encode(self._child_process_encoding())
        return (self.PIPE, user_input)

    def command_for_printing(self, args):
        """Returns a print-ready string representing command args.
        The string should be copy/paste ready for execution in a shell.
        """
        args = self._stringify_args(args)
        escaped_args = []
        for arg in args:
            if isinstance(arg, six.text_type):
                # Escape any non-ascii characters for easy copy/paste
                arg = arg.encode('unicode_escape')
            # FIXME: Do we need to fix quotes here?
            escaped_args.append(arg.decode(self._child_process_encoding()))
        return ' '.join(escaped_args)

    def run_command(
            self,
            args,
            cwd=None,
            env=None,
            input=None,  # pylint: disable=redefined-builtin
            timeout_seconds=None,
            error_handler=None,
            return_exit_code=False,
            stderr=STDOUT,
            decode_output=True,
            debug_logging=True):
        """Popen wrapper for convenience and to work around python bugs.

        By default, run_command will expect a zero exit code and will return the
        program output in that case, or throw a ScriptError if the program has a
        non-zero exit code. This behavior can be changed by setting the
        appropriate input parameters.

        Args:
            args: the program arguments. Passed to Popen.
            cwd: the current working directory for the program. Passed to Popen.
            env: the environment for the program. Passed to Popen.
            input: input to give to the program on stdin. Accepts either a file
                handler (will be passed directly) or a string (will be passed
                via a pipe).
            timeout_seconds: maximum time in seconds to wait for the program to
                terminate; on a timeout the process will be killed
            error_handler: a custom error handler called with a ScriptError when
                the program fails. The default handler raises the error.
            return_exit_code: instead of returning the program output, return
                the exit code. Setting this makes non-zero exit codes non-fatal
                (the error_handler will not be called).
            stderr: How to handle stderr. See [0] for usage.
            decode_output: whether to decode the program output.
            debug_logging: whether to log details about program execution.

        [0]: https://docs.python.org/3/library/subprocess.html#frequently-used-arguments
        """
        assert isinstance(args, list) or isinstance(args, tuple)
        start_time = time.time()
        stdin, string_to_communicate = self._compute_stdin(input)
        process = self.popen(
            args,
            stdin=stdin,
            stdout=self.PIPE,
            stderr=stderr,
            cwd=cwd,
            env=env,
            close_fds=self._should_close_fds())

        stdout_data, stderr_data = b'', b''
        try:
            stdout_data, stderr_data = process.communicate(
                string_to_communicate, timeout_seconds)
        except subprocess.TimeoutExpired:
            _log.error('Error: Command timed out after %s seconds',
                       timeout_seconds)
        finally:
            process.kill()

        # wait() is not threadsafe and can throw OSError due to:
        # http://bugs.python.org/issue1731717
        exit_code = process.wait()

        if debug_logging:
            _log.debug('"%s" took %.2fs', self.command_for_printing(args),
                       time.time() - start_time)

        if return_exit_code:
            return exit_code

        if exit_code:
            # `stderr_data` may be `None` if `stderr` was not `PIPE`.
            output = stdout_data + (stderr_data or b'')
            script_error = ScriptError(script_args=args,
                                       exit_code=exit_code,
                                       output=output.decode(errors='replace'),
                                       cwd=cwd,
                                       output_limit=self.error_output_limit)
            (error_handler or self.default_error_handler)(script_error)

        # run_command automatically decodes to str() unless explicitly told not to.
        if decode_output:
            return stdout_data.decode(self._child_process_encoding(),
                                      errors='replace')
        return stdout_data

    def _child_process_encoding(self):
        # Win32 Python 2.x uses CreateProcessA rather than CreateProcessW
        # to launch subprocesses, so we have to encode arguments using the
        # current code page.
        if sys.platform == 'win32' and sys.version < '3':
            return 'mbcs'
        # All other platforms use UTF-8.
        # FIXME: Using UTF-8 on Cygwin will confuse Windows-native commands
        # which will expect arguments to be encoded using the current code
        # page.
        return 'utf-8'

    def _should_encode_child_process_arguments(self):
        # Win32 Python 2.x uses CreateProcessA rather than CreateProcessW
        # to launch subprocesses, so we have to encode arguments using the
        # current code page.
        if sys.platform == 'win32' and sys.version < '3':
            return True

        # On other (POSIX) platforms, we need to encode arguments if the system
        # does not use UTF-8 encoding. Otherwise, subprocess.Popen will raise
        # TypeError. Note that macOS always uses UTF-8, while on UNIX it
        # depends on user locale (LC_CTYPE) and sys.getfilesystemencoding() may
        # fail and return None.
        return (sys.getfilesystemencoding() or '').lower() != 'utf-8'

    def _encode_argument_if_needed(self, argument):
        if not self._should_encode_child_process_arguments():
            return argument
        return argument.encode(self._child_process_encoding())

    def _stringify_args(self, args):
        # Popen will throw an exception if args are non-strings (like int())
        string_args = map(six.text_type, args)
        # The Windows implementation of Popen cannot handle unicode strings. :(
        return map(self._encode_argument_if_needed, string_args)

    def popen(self, args, **kwargs):
        assert not kwargs.get('shell')
        string_args = self._stringify_args(args)

        # os.setpgid is required, otherwise, kill_process function will kill
        # the parent processes unexpectedly.
        if sys.platform != 'win32':
            kwargs['preexec_fn'] = lambda: os.setpgid(0, 0)
        return subprocess.Popen(string_args, **kwargs)

    def call(self, args, **kwargs):
        return subprocess.call(self._stringify_args(args), **kwargs)

    def run_in_parallel(self, command_lines_and_cwds, processes=None):
        """Runs a list of (cmd_line list, cwd string) tuples in parallel and returns a list of (retcode, stdout, stderr) tuples."""
        assert len(command_lines_and_cwds)
        return self.map(_run_command_thunk, command_lines_and_cwds, processes)

    def map(self, thunk, arglist, processes=None):
        if sys.platform == 'win32' or len(arglist) == 1:
            return map(thunk, arglist)
        pool = multiprocessing.Pool(processes=(processes or self.cpu_count()))
        try:
            return pool.map(thunk, arglist)
        finally:
            pool.close()
            pool.join()


def _run_command_thunk(cmd_line_and_cwd):
    # Note that this needs to be a bare module (and hence Picklable) method to work with multiprocessing.Pool.
    (cmd_line, cwd) = cmd_line_and_cwd
    proc = subprocess.Popen(
        cmd_line, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    return (proc.returncode, stdout, stderr)

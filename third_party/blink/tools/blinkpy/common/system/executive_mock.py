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

import collections
import logging
import os
import StringIO

from blinkpy.common.system.executive import ScriptError

_log = logging.getLogger(__name__)


class MockProcess(object):

    def __init__(self, stdout='MOCK STDOUT\n', stderr='', returncode=0):
        self.pid = 42
        self.stdout = StringIO.StringIO(stdout)
        self.stderr = StringIO.StringIO(stderr)
        self.stdin = StringIO.StringIO()
        self.returncode = returncode

    def wait(self):
        return

    def poll(self):
        # Consider the process completed when all the stdout and stderr has been read.
        if self.stdout.len != self.stdout.tell() or self.stderr.len != self.stderr.tell():
            return None
        return self.returncode

    def communicate(self, *_):
        return (self.stdout.getvalue(), self.stderr.getvalue())

    def kill(self):
        return

    def terminate(self):
        return


MockCall = collections.namedtuple(
    'MockCall', ('args', 'kwargs'))


class MockExecutive(object):
    PIPE = 'MOCK PIPE'
    STDOUT = 'MOCK STDOUT'
    DEVNULL = 'MOCK_DEVNULL'

    @staticmethod
    def ignore_error(error):
        pass

    def __init__(self, should_log=False, should_throw=False,
                 output='MOCK output of child process', stderr='',
                 exit_code=0, exception=None, run_command_fn=None,
                 proc=None):
        self._should_log = should_log
        self._should_throw = should_throw
        # FIXME: Once executive wraps os.getpid() we can just use a static pid for "this" process.
        self._running_pids = {'run_blinkpy_tests.py': os.getpid()}
        self._output = output
        self._stderr = stderr
        self._exit_code = exit_code
        self._exception = exception
        self._run_command_fn = run_command_fn
        self._proc = proc
        self.full_calls = []

    def _append_call(self, args, **kwargs):
        self.full_calls.append(MockCall(args=args, kwargs=kwargs))

    def check_running_pid(self, pid):
        return pid in self._running_pids.values()

    def running_pids(self, process_name_filter):
        running_pids = []
        for process_name, process_pid in self._running_pids.iteritems():
            if process_name_filter(process_name):
                running_pids.append(process_pid)

        _log.info('MOCK running_pids: %s', running_pids)
        return running_pids

    def command_for_printing(self, args):
        string_args = map(unicode, args)
        return ' '.join(string_args)

    # The argument list should match Executive.run_command, even if
    # some arguments are not used. pylint: disable=unused-argument
    def run_command(self,
                    args,
                    cwd=None,
                    env=None,
                    input=None,  # pylint: disable=redefined-builtin
                    timeout_seconds=None,
                    error_handler=None,
                    return_exit_code=False,
                    return_stderr=True,
                    ignore_stderr=False,
                    decode_output=True,
                    debug_logging=True):
        self._append_call(args, cwd=cwd, input=input, env=env)

        assert isinstance(args, list) or isinstance(args, tuple)

        if self._should_log:
            env_string = ''
            if env:
                env_string = ', env=%s' % env
            input_string = ''
            if input:
                input_string = ', input=%s' % input
            _log.info('MOCK run_command: %s, cwd=%s%s%s', args, cwd, env_string, input_string)

        if self._exception:
            raise self._exception  # pylint: disable=raising-bad-type
        if self._should_throw:
            raise ScriptError('MOCK ScriptError', output=self._output)

        if self._run_command_fn:
            return self._run_command_fn(args)

        if return_exit_code:
            return self._exit_code

        if self._exit_code and error_handler:
            script_error = ScriptError(script_args=args, exit_code=self._exit_code, output=self._output)
            error_handler(script_error)

        output = self._output
        if return_stderr:
            output += self._stderr
        if decode_output and not isinstance(output, unicode):
            output = output.decode('utf-8')

        return output

    def cpu_count(self):
        return 2

    def kill_process(self, pid, kill_tree=True):
        pass

    def interrupt(self, pid):
        pass

    def popen(self, args, cwd=None, env=None, **_):
        assert all(isinstance(arg, basestring) for arg in args)
        self._append_call(args, cwd=cwd, env=env)
        if self._should_log:
            cwd_string = ''
            if cwd:
                cwd_string = ', cwd=%s' % cwd
            env_string = ''
            if env:
                env_string = ', env=%s' % env
            _log.info('MOCK popen: %s%s%s', args, cwd_string, env_string)
        if not self._proc:
            self._proc = MockProcess(stdout=self._output, stderr=self._stderr, returncode=self._exit_code)
        return self._proc

    def call(self, args, **_):
        assert all(isinstance(arg, basestring) for arg in args)
        self._append_call(args)
        _log.info('Mock call: %s', args)

    def run_in_parallel(self, commands):
        assert len(commands)

        num_previous_calls = len(self.full_calls)
        command_outputs = []
        for cmd_line, cwd in commands:
            assert all(isinstance(arg, basestring) for arg in cmd_line)
            command_outputs.append([0, self.run_command(cmd_line, cwd=cwd), ''])

        new_calls = self.full_calls[num_previous_calls:]
        self.full_calls = self.full_calls[:num_previous_calls]
        self.full_calls.append(new_calls)
        return command_outputs

    def map(self, thunk, arglist, processes=None):
        return map(thunk, arglist)

    @property
    def calls(self):
        # TODO(crbug.com/718456): Make self.full_calls always be an array of
        # arrays of MockCalls, rather than a union type, and possibly remove
        # this property in favor of direct "full_calls" access in unit tests.
        def get_args(v):
            if isinstance(v, list):
                return [get_args(e) for e in v]
            elif isinstance(v, MockCall):
                return v.args
            else:
                return TypeError('Unknown full_calls type: %s' % (type(v).__name__,))
        return get_args(self.full_calls)


def mock_git_commands(vals, strict=False):
    # TODO(robertma): Support optional look-up by arguments.
    def run_fn(args):
        sub_command = args[1]
        if strict and sub_command not in vals:
            raise AssertionError('{} not found in sub-command list {}'.format(
                sub_command, vals))
        return vals.get(sub_command, '')
    return MockExecutive(run_command_fn=run_fn)

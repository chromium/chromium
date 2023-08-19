# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2009 Daniel Bates (dbates@intudata.com). All rights reserved.
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

import os
import subprocess
import sys
import unittest
import six

# Since we execute this script directly as part of the unit tests, we need to
# ensure that blink/tools is in sys.path for the next imports to work correctly.
script_dir = os.path.dirname(
    os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

from blinkpy.common.system.executive import Executive, ScriptError


class ScriptErrorTest(unittest.TestCase):
    def test_message_with_output(self):
        error = ScriptError('My custom message!', '', -1)
        self.assertEqual(error.message_with_output(), 'My custom message!')
        error = ScriptError('My custom message!', '', -1, 'My output.')
        self.assertEqual(error.message_with_output(),
                         'My custom message!\n\noutput: My output.')
        error = ScriptError('', 'my_command!', -1, 'My output.',
                            '/Users/username/blah')
        self.assertEqual(
            error.message_with_output(),
            'Failed to run "\'my_command!\'" exit_code: -1 cwd: /Users/username/blah\n\noutput: My output.'
        )
        error = ScriptError('', 'my_command!', -1, 'ab' + '1' * 499)
        self.assertEqual(
            error.message_with_output(),
            'Failed to run "\'my_command!\'" exit_code: -1\n\noutput: Last 500 characters of output:\nb'
            + '1' * 499)

    def test_message_with_tuple(self):
        error = ScriptError('', ('my', 'command'), -1, 'My output.',
                            '/Users/username/blah')
        self.assertEqual(
            error.message_with_output(),
            'Failed to run "(\'my\', \'command\')" exit_code: -1 cwd: /Users/username/blah\n\noutput: My output.'
        )


def never_ending_command():
    """Arguments for a command that will never end (useful for testing process
    killing). It should be a process that is unlikely to already be running
    because all instances will be killed.
    """
    if sys.platform == 'win32':
        return ['wmic']
    return ['yes']


def command_line(cmd, *args):
    return [sys.executable, __file__, '--' + cmd] + list(args)


class ExecutiveTest(unittest.TestCase):
    def test_run_command_with_bad_command(self):
        def run_bad_command():
            Executive().run_command(['foo_bar_command_blah'],
                                    error_handler=Executive.ignore_error,
                                    return_exit_code=True)

        with self.assertRaises(OSError):
            run_bad_command()

    def test_run_command_args_type(self):
        executive = Executive()
        with self.assertRaises(AssertionError):
            executive.run_command('echo')
        with self.assertRaises(AssertionError):
            executive.run_command(u'echo')
        executive.run_command(command_line('echo', 'foo'))
        executive.run_command(tuple(command_line('echo', 'foo')))

    def test_auto_stringify_args(self):
        executive = Executive()
        executive.run_command(command_line('echo', 1))
        executive.popen(command_line('echo', 1), stdout=executive.PIPE).wait()
        self.assertEqual('echo 1', executive.command_for_printing(['echo', 1]))

    def test_print_command_unicode(self):
        executive = Executive()
        # The expected result is different on Windows because the unicode arg
        # first gets encoded using 'mbcs'. This encoding makes it unnecessary to
        # escape any unicode characters in the arg.
        # Elsewhere, the 'mbcs' encoding is skipped, but then we must escape any
        # non-ascii unicode characters by encoding with 'unicode_escape'. This
        # results in an extra \ on non-Win platforms.
        if sys.platform == 'win32' and six.PY2:
            expected_result = u'echo 1 a\xac'
        else:
            expected_result = u'echo 1 a\\xac'
        self.assertEqual(expected_result,
                         executive.command_for_printing(['echo', 1, u'a\xac']))

    def test_popen_args(self):
        executive = Executive()
        # Explicitly naming the 'args' argument should not throw an exception.
        executive.popen(
            args=command_line('echo', 1), stdout=executive.PIPE).wait()

    def test_run_command_with_unicode(self):
        """Validate that it is safe to pass unicode() objects
        to Executive.run* methods, and they will return unicode()
        objects by default unless decode_output=False
        """
        # TODO(crbug/1306209): Needs more investigation. skipping for now.
        if sys.platform == 'win32' and six.PY3:
            return
        unicode_tor_input = u"WebKit \u2661 Tor Arne Vestb\u00F8!"
        if sys.platform == 'win32' and six.PY2:
            encoding = 'mbcs'
        else:
            encoding = 'utf-8'
        encoded_tor = unicode_tor_input.encode(encoding)
        # On Windows, we expect the unicode->mbcs->unicode roundtrip to be
        # lossy. On other platforms, we expect a lossless roundtrip.
        if sys.platform == 'win32' and six.PY2:
            unicode_tor_output = encoded_tor.decode(encoding)
        else:
            unicode_tor_output = unicode_tor_input

        executive = Executive()

        output = executive.run_command(
            command_line('cat'), input=unicode_tor_input)
        self.assertEqual(output, unicode_tor_output)

        output = executive.run_command(command_line('echo', unicode_tor_input))
        self.assertEqual(output, unicode_tor_output)

        output = executive.run_command(
            command_line('echo', unicode_tor_input), decode_output=False)
        self.assertEqual(output, encoded_tor)

        # Make sure that str() input also works.
        output = executive.run_command(
            command_line('cat'), input=encoded_tor, decode_output=False)
        self.assertEqual(output, encoded_tor)

    def test_kill_process(self):
        executive = Executive()
        if sys.platform == 'win32':
            process = subprocess.Popen(
                never_ending_command(), stdout=subprocess.PIPE)
        else:
            process = subprocess.Popen(
                never_ending_command(),
                stdout=subprocess.PIPE,
                preexec_fn=lambda: os.setpgid(0, 0))

        self.assertEqual(process.poll(), None)  # Process is running
        executive.kill_process(process.pid)

        # Killing again should fail silently.
        executive.kill_process(process.pid)

    @unittest.skip('Flaky. See crbug.com/1242429.')
    def test_timeout_exceeded(self):
        executive = Executive()

        def timeout():
            executive.run_command(
                command_line('sleep', 'infinity'), timeout_seconds=0.01)

        with self.assertRaises(ScriptError):
            timeout()

    @unittest.skip('Flaky. See crbug.com/1242429.')
    def test_timeout_exceeded_exit_code(self):
        executive = Executive()
        exit_code = executive.run_command(
            command_line('sleep', 'infinity'),
            timeout_seconds=0.01,
            return_exit_code=True)
        self.assertNotEqual(exit_code, 0)

    def test_timeout_satisfied(self):
        executive = Executive()
        executive.run_command(command_line('sleep', '0'), timeout_seconds=1000)

    def test_check_running_pid(self):
        executive = Executive()
        self.assertTrue(executive.check_running_pid(os.getpid()))
        # According to the proc(5) man page, on 64-bit linux systems,
        # pid_max can be set to any value up to 2^22 (approximately 4 million).
        self.assertFalse(executive.check_running_pid(5000000))

    def test_running_pids(self):
        executive = Executive()
        pids = executive.running_pids()
        self.assertIn(os.getpid(), pids)

    def test_run_in_parallel_assert_nonempty(self):
        with self.assertRaises(AssertionError):
            Executive().run_in_parallel([])


def main(platform, stdin, stdout, cmd, args):
    if platform == 'win32' and hasattr(stdout, 'fileno'):
        import msvcrt
        msvcrt.setmode(stdout.fileno(), os.O_BINARY)
    if cmd == '--cat':
        stdout.write(stdin.read())
    elif cmd == '--echo':
        stdout.write(' '.join(args))
    return 0


if __name__ == '__main__' and len(sys.argv) > 1 and sys.argv[1] in ('--cat',
                                                                    '--echo'):
    sys.exit(
        main(sys.platform, sys.stdin, sys.stdout, sys.argv[1], sys.argv[2:]))

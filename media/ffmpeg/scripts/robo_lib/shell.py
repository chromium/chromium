# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Contains helper methods for writing to and reading from
the console.
'''

import inspect
import re
import shutil
import subprocess
import sys
import threading

# String sequence to clear the current line.
CLEAR_LINE = '\r\033[K'


class Style:
    """ANSI escape codes for terminal formatting."""
    RESET = '\033[0m'
    BOLD = '\033[1m'
    DIM = '\033[2m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'


g_log_file = None


def enable_file_logging(filename):
    """Enables logging to a file in addition to stdout."""
    global g_log_file

    try:
        g_log_file = open(filename, 'w', encoding='utf-8')
        print(f"Logging all output to {Style.BOLD}{filename}")
    except Exception as e:
        print(f"Failed to enable file logging: {e}")


def _strip_ansi(text):
    """Removes ANSI escape codes from text."""
    return re.sub(r'\033\[[0-9;]*m', '', text)


def rolling_log(line):
    """
    Allows line to be outputted to the console by overwriting the previous line.
    """
    # Write to file if enabled
    if g_log_file:
        g_log_file.write(_strip_ansi(line.strip()) + '\n')
        g_log_file.flush()

    # Truncate line to avoid wrapping which breaks \r overwriting
    try:
        width = shutil.get_terminal_size().columns
    except Exception:
        width = 80

    sline = line.strip()
    if len(sline) > width:
        sline = sline[:width]

    # Mark failures
    if 'FAIL' in sline or 'ERROR' in sline:
        # Clear the line first
        print(f'{CLEAR_LINE}', end='')
        log(sline, style=Style.RED)
    else:
        # Rolling log. We use print directly to control cursor.
        print(f'{CLEAR_LINE}{Style.DIM}{sline}{Style.RESET}',
              end='',
              flush=True)


def log(message, style=None, **kwargs):
    """Prints a message to stdout with the caller's filename and line number.

    Args:
        message: The string to print.
        color: Optional ANSI color code to apply to the message.
        **kwargs: Arguments passed to print().
    """
    caller = inspect.stack()[1]
    filename_no_path = caller.filename.split('/')[-1]
    prefix = f'[{filename_no_path}:{caller.lineno}]'

    if g_log_file:
        g_log_file.write(f'{prefix} {_strip_ansi(str(message))}\n')
        g_log_file.flush()

    if sys.stdout.isatty():
        # Style the prefix in Cyan to distinguish it from the message.
        prefix = f'{Style.CYAN}{prefix}{Style.RESET}'
        if style:
            message = f'{style}{message}{Style.RESET}'
        # Clear any rolling log line before printing
        print(CLEAR_LINE, end='')

    print(f'{prefix} {message}', **kwargs)


def run_live(command, on_line, **kwargs):
    """Wraps subprocess.Popen to stream output.

    Args:
        command: Command to run.
        on_line: Callback(line_str) -> None.
        kwargs: Passed to subprocess.Popen.
    """
    popen_kwargs = kwargs.copy()
    popen_kwargs['stdout'] = subprocess.PIPE

    # If stderr is passed as None (or missing), capture it so we can print it
    # cleanly interleaving with the rolling log.
    capture_stderr = False
    if popen_kwargs.get('stderr') is None:
        popen_kwargs['stderr'] = subprocess.PIPE
        capture_stderr = True
    elif 'stderr' not in popen_kwargs:
        # If unspecified, default to STDOUT (merged)
        popen_kwargs['stderr'] = subprocess.STDOUT

    popen_kwargs['encoding'] = popen_kwargs.get('encoding', 'utf-8')
    popen_kwargs['universal_newlines'] = True
    if 'shell' not in popen_kwargs:
        popen_kwargs['shell'] = False

    process = subprocess.Popen(command, **popen_kwargs)

    def pipe_stderr():
        for line in process.stderr:
            l = line.rstrip()
            if l:
                if g_log_file:
                    g_log_file.write(_strip_ansi(l) + '\n')
                    g_log_file.flush()
                # Clear line and print raw stderr output
                print(f'{CLEAR_LINE}{l}')

    stderr_thread = None
    if capture_stderr:
        stderr_thread = threading.Thread(target=pipe_stderr)
        stderr_thread.start()

    try:
        for line in process.stdout:
            on_line(line)
    finally:
        if stderr_thread:
            stderr_thread.join()
        process.wait()

    # Add an empty line at the end of the output.
    print()
    if process.returncode != 0:
        raise RuntimeError(
            f'`{" ".join(command)}` returned {process.returncode}')
    return None


def run(command, **kwargs):
    """Wraps subprocess.run with specific defaults (utf-8, piped output)."""
    return subprocess.run(command,
                          encoding=kwargs.get('encoding', 'utf-8'),
                          shell=kwargs.get('shell', False),
                          stderr=kwargs.get('stderr', subprocess.PIPE),
                          stdout=kwargs.get('stdout', subprocess.PIPE))


def output_or_error(command, error_gen=None, **kwargs):
    ''' Run a command and return the output as a string, or raise error.
    error_gen: a function that could either return or raise an error
             from a given subprocess.CompletedProcess object.
    '''
    result = run(command, **kwargs)
    if result.returncode:
        if error_gen is not None:
            raise error_gen(result)
        raise RuntimeError(
            f'`{" ".join(command)}` ({result.returncode})\n\t'
            f'{result.stderr}\n\t{result.stdout}'
        )

    if kwargs.get('stdout', subprocess.PIPE) == subprocess.PIPE:
        return result.stdout.strip()
    return ''


def check_run(command, error_gen=None, **kwargs):
    ''' Just like output_or_error, but returns None on success.
    '''
    output_or_error(command, error_gen, **kwargs)
    return None


def stdout_fail_ok(command, **kwargs):
    return run(command, **kwargs).stdout.strip()

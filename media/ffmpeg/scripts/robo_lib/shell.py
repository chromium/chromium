# Copyright 2024 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Contains helper methods for writing to and reading from
the console that can be configured by arguments, such as
verbose logging and dryrunning potentially dangerous commands.
'''

import collections
import inspect
import os
import subprocess

LOG_LEVEL = collections.namedtuple('LL', ['NORM', 'VERBOSE', 'DEBUG'])(1, 2, 3)
SET_LOG_LEVEL = LOG_LEVEL.NORM

EXEC_MOD = collections.namedtuple('EM', ['NORM', 'DRY'])
SET_EXEC_MOD = EXEC_MOD.NORM


def log(message, verbosity=LOG_LEVEL.NORM, **kwargs):
    caller = inspect.stack()[1]
    filename_no_path = caller.filename.split('/')[-1]
    if verbosity >= SET_LOG_LEVEL:
        print(f'[{filename_no_path}:{caller.lineno}] {message}', **kwargs)


def run(command, **kwargs):
    if SET_EXEC_MOD == EXEC_MOD.NORM:
        return subprocess.run(command,
                              encoding=kwargs.get('encoding', 'utf-8'),
                              shell=kwargs.get('shell', False),
                              stderr=kwargs.get('stderr', subprocess.PIPE),
                              stdout=kwargs.get('stdout', subprocess.PIPE))
    else:
        # Print it, not 'echo' it, since there might be some funky unescape stuff
        print(' '.join(command))
        return subprocess.CompletedProcess(['echo'] + command, 0)


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
            f'`{" ".join(command)}` ({result.returncode})\n\t{result.stderr}\n\t{result.stdout}'
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

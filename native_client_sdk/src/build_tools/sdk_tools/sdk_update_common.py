# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for sdk_update.py and sdk_update_main.py."""

import errno
import logging
import os
import shutil
import subprocess
import sys
import time


class Error(Exception):
  """Generic error/exception for sdk_update module"""
  pass


def MakeDirs(directory):
  if not os.path.exists(directory):
    logging.info('Making directory %s' % (directory,))
    os.makedirs(directory)


def RemoveDir(outdir):
  """Removes the given directory

  On Unix systems, this just runs shutil.rmtree, but on Windows, this doesn't
  work when the directory contains junctions (as does our SDK installer).
  Therefore, on Windows, it runs rmdir /S /Q as a shell command.  This always
  does the right thing on Windows. If the directory already didn't exist,
  RemoveDir will return successfully without taking any action.

  Args:
    outdir: The directory to delete

  Raises:
    Error - If this operation fails for any reason.
  """

  max_tries = 5
  last_exception = None
  for num_tries in xrange(max_tries):
    try:
      shutil.rmtree(outdir)
      return
    except OSError as e:
      if not os.path.exists(outdir):
        # The directory can't be removed because it doesn't exist.
        return
      last_exception = e

    # On Windows this could be an issue with junctions, so try again with
    # rmdir.
    if sys.platform == 'win32':
      try:
        cmd = ['rmdir', '/S', '/Q', outdir]
        process = subprocess.Popen(cmd, stderr=subprocess.PIPE, shell=True)
        _, stderr = process.communicate()
        if process.returncode != 0:
          raise Error('\"%s\" failed with code %d. Output:\n  %s' % (
            ' '.join(cmd), process.returncode, stderr))
        return
        # Ignore failures, we'll just try again.
      except subprocess.CalledProcessError as e:
        # CalledProcessError has no error message, generate one.
        last_exception = Error('\"%s\" failed with code %d.' % (
          ' '.join(e.cmd), e.returncode))
      except Error as e:
        last_exception = e

    # Didn't work, sleep and try again.
    time.sleep(num_tries + 1)

  # Failed.
  raise Error('Unable to remove directory "%s"\n  %s' % (outdir,
                                                         last_exception))


def RenameDir(srcdir, destdir):
  """Renames srcdir to destdir. Removes destdir before doing the
     rename if it already exists."""

  max_tries = 5
  num_tries = 0
  for num_tries in xrange(max_tries):
    try:
      RemoveDir(destdir)
      shutil.move(srcdir, destdir)
      return
    except OSError as err:
      if err.errno != errno.EACCES:
        raise err
      # If we are here, we didn't exit due to raised exception, so we are
      # handling a Windows flaky access error.  Sleep one second and try
      # again.
      time.sleep(num_tries + 1)

  # end of while loop -- could not RenameDir
  raise Error('Could not RenameDir %s => %s after %d tries.\n'
              'Please check that no shells or applications '
              'are accessing files in %s.'
              % (srcdir, destdir, num_tries + 1, destdir))

#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import signal
import subprocess
import time

class BrowserProcessBase(object):

  def __init__(self, handle):
    self.handle = handle
    print('PID', self.handle.pid)

  def GetReturnCode(self):
    return self.handle.returncode

  def IsRunning(self):
    return self.handle.poll() is None

  def Wait(self, wait_steps, sleep_time):
    try:
      self.term()
    except Exception:
      # Terminating the handle can raise an exception. There is likely no point
      # in waiting if the termination didn't succeed.
      return

    i = 0
    # subprocess.wait() doesn't have a timeout, unfortunately.
    while self.IsRunning() and i < wait_steps:
      time.sleep(sleep_time)
      i += 1

  def Kill(self):
    if self.IsRunning():
      print('KILLING the browser')
      try:
        self.kill()
        # If it doesn't die, we hang.  Oh well.
        self.handle.wait()
      except Exception:
        # If it is already dead, then it's ok.
        # This may happen if the browser dies after the first poll, but
        # before the kill.
        if self.IsRunning():
          raise

class BrowserProcess(BrowserProcessBase):

  def term(self):
    self.handle.terminate()

  def kill(self):
    self.handle.kill()


class BrowserProcessPosix(BrowserProcessBase):
  """ This variant of BrowserProcess uses process groups to manage browser
  life time. """

  def term(self):
    os.killpg(self.handle.pid, signal.SIGTERM)

  def kill(self):
    os.killpg(self.handle.pid, signal.SIGKILL)


def RunCommandWithSubprocess(cmd, env=None):
  handle = subprocess.Popen(cmd, env=env)
  return BrowserProcess(handle)


def RunCommandInProcessGroup(cmd, env=None):
  def SetPGrp():
    os.setpgrp()
    print('I\'M THE SESSION LEADER!')

  handle = subprocess.Popen(cmd, env=env, preexec_fn=SetPGrp)
  return BrowserProcessPosix(handle)

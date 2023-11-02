#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import sys
import time


class RPCListener(object):

  def __init__(self, shutdown_callback):
    self.shutdown_callback = shutdown_callback
    self.prefix = '|||| '
    self.ever_failed = False
    self.start_time = time.time()

  def Log(self, message):
    # Display the number of milliseconds since startup.
    # This gives us additional data for debugging bot behavior.
    prefix = '[%6s ms] ' % int((time.time()-self.start_time)*1000) + self.prefix
    lines = [line.rstrip() for line in message.split('\n')]
    text = ''.join(['%s%s\n' % (prefix, line) for line in lines])
    sys.stdout.write(text)

  def TestLog(self, message):
    self.Log(message)
    return 'OK'

  # Something went very wrong on the server side, everything is horked?
  # Only called locally.
  def ServerError(self, message):
    self.Log('\n[SERVER_ERROR] %s' % (message,))
    self.ever_failed = True
    self._TestingDone()
    return 'OK'

  # Does nothing.  Called to prevent timeouts.  (The server resets the timeout
  # every time it receives a GET request.)
  def Ping(self):
    return 'OK'

  # This happens automatically, as long as the renderer process has not crashed.
  def JavaScriptIsAlive(self):
    return 'OK'

  def Shutdown(self, message, passed):
    self.Log(message)
    # This check looks slightly backwards, but this is intentional.
    # Everything but passed.lower() == 'true' is considered a failure.  This
    # means that if the test runner sends garbage, it will be a failure.
    # NOTE in interactive mode this function may be called multiple times.
    # ever_failed is designed to be set and never reset - if any of the runs
    # fail, the an error code will be returned to the command line.
    # In summary, the tester is biased towards failure - it should scream "FAIL"
    # if things are not 100% correct.  False positives must be avoided.
    if passed.lower() != 'true':
      self.ever_failed = True
    close_browser = self._TestingDone()
    if close_browser:
      return 'Die, please'
    else:
      return 'OK'

  def _TestingDone(self):
    return self.shutdown_callback()

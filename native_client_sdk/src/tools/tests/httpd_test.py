#!/usr/bin/env vpython3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import Queue
import sys
import subprocess
import threading
import unittest
import urllib2

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(TOOLS_DIR)))

sys.path.append(TOOLS_DIR)

import httpd
from mock import patch, Mock


class HTTPDTest(unittest.TestCase):
  def setUp(self):
    patcher = patch('BaseHTTPServer.BaseHTTPRequestHandler.log_message')
    patcher.start()
    self.addCleanup(patcher.stop)

    self.server = httpd.LocalHTTPServer('.', 0)
    self.addCleanup(self.server.Shutdown)

  def testQuit(self):
    urllib2.urlopen(self.server.GetURL('?quit=1'))
    self.server.process.join(10)  # Wait 10 seconds for the process to finish.
    self.assertFalse(self.server.process.is_alive())


class MainTest(unittest.TestCase):
  @patch('httpd.LocalHTTPServer')
  @patch('sys.stdout', Mock())
  def testArgs(self, mock_server_ctor):
    mock_server = Mock()
    mock_server_ctor.return_value = mock_server
    httpd.main(['-p', '123', '-C', 'dummy'])
    mock_server_ctor.assert_called_once_with('dummy', 123)


class RunTest(unittest.TestCase):
  def setUp(self):
    self.process = None

  def tearDown(self):
    if self.process and self.process.returncode is None:
      self.process.kill()

  @staticmethod
  def _SubprocessThread(process, queue):
    stdout, stderr = process.communicate()
    queue.put((process.returncode, stdout, stderr))

  def _Run(self, args=None, timeout=None):
    args = args or []
    cmd = [sys.executable, os.path.join(TOOLS_DIR, 'run.py'), '--port=5555']
    cmd.extend(args)
    self.process = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)
    queue = Queue.Queue()
    thread = threading.Thread(target=RunTest._SubprocessThread,
                              args=(self.process, queue))
    thread.daemon = True
    thread.start()
    thread.join(timeout)
    self.assertFalse(thread.is_alive(), "Thread still running after timeout")

    returncode, stdout, stderr = queue.get(False)
    return returncode, stdout, stderr


  @staticmethod
  def _GetChromeMockArgs(page, http_request_type, sleep,
                         expect_to_be_killed=True):
    args = []
    if page:
      args.extend(['-P', page])
    args.append('--')
    args.extend([sys.executable, os.path.join(SCRIPT_DIR, 'chrome_mock.py')])
    if http_request_type:
      args.append('--' + http_request_type)
    if sleep:
      args.extend(['--sleep', str(sleep)])
    if expect_to_be_killed:
      args.append('--expect-to-be-killed')
    return args

  def testQuit(self):
    args = self._GetChromeMockArgs('?quit=1', 'get', sleep=10)
    rtn, stdout, _ = self._Run(args, timeout=20)
    self.assertEqual(rtn, 0)
    self.assertIn('Starting', stdout)
    self.assertNotIn('Expected to be killed', stdout)

  def testSubprocessDies(self):
    args = self._GetChromeMockArgs(page=None, http_request_type=None, sleep=0,
                                   expect_to_be_killed=False)
    returncode, stdout, _ = self._Run(args, timeout=10)
    self.assertNotEqual(-1, returncode)
    self.assertIn('Starting', stdout)


if __name__ == '__main__':
  unittest.main()

# Copyright (C) 2014 Google Inc. All rights reserved.
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

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.web_tests.port.test import TestPort
from blinkpy.web_tests.port.browser_test_driver import BrowserTestDriver
from blinkpy.web_tests.port.server_process_mock import MockServerProcess


class BrowserTestDriverTest(unittest.TestCase):
    def test_read_stdin_path(self):
        port = TestPort(MockHost())
        driver = BrowserTestDriver(port, 0)
        driver._server_process = MockServerProcess(
            lines=[b'StdinPath: /foo/bar', b'#EOF'])
        content_block = driver._read_block(0)
        self.assertEqual(content_block.stdin_path, b'/foo/bar')
        driver._stdin_directory = None

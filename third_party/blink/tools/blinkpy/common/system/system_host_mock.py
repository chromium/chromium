# Copyright (c) 2011 Google Inc. All rights reserved.
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

from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.common.system.platform_info_mock import MockPlatformInfo
from blinkpy.common.system.user_mock import MockUser

from six import StringIO


class MockSystemHost(object):
    def __init__(self,
                 log_executive=False,
                 os_name=None,
                 os_version=None,
                 machine=None,
                 executive=None,
                 filesystem=None,
                 processor=None,
                 time_return_val=123):
        self.executable = 'python'
        self.executive = executive or MockExecutive(should_log=log_executive)
        self.filesystem = filesystem or MockFileSystem()
        self.user = MockUser()
        self.platform = MockPlatformInfo(machine=machine, processor=processor)
        if os_name:
            self.platform.os_name = os_name
        if os_version:
            self.platform.os_version = os_version

        self.stdin = StringIO()
        self.stdout = StringIO()
        self.stderr = StringIO()
        self.environ = {'MOCK_ENVIRON_COPY': '1', 'PATH': '/bin:/mock/bin'}
        self.time_return_val = time_return_val

    def time(self):
        return self.time_return_val

    def sleep(self, seconds):
        self.time_return_val += seconds

    def print_(self, *args, **kwargs):
        sep = kwargs.get('sep', ' ')
        end = kwargs.get('end', '\n')
        stream = kwargs.get('stream', self.stdout)
        stream.write(sep.join([str(arg) for arg in args]) + end)

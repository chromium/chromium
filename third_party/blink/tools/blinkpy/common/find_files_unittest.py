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

import sys
import unittest

from blinkpy.common import find_files
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.common.system.filesystem_mock import MockFileSystem


class MockWinFileSystem(object):
    def join(self, *paths):
        return '\\'.join(paths)

    def normpath(self, path):
        return path.replace('/', '\\')


class TestWinNormalize(unittest.TestCase):
    def assert_filesystem_normalizes(self, filesystem):
        # pylint: disable=protected-access
        self.assertEqual(
            find_files._normalize(
                filesystem, 'c:\\foo',
                ['fast/html', 'fast/canvas/*', 'compositing/foo.html']), [
                    'c:\\foo\\fast\\html', 'c:\\foo\\fast\\canvas\\*',
                    'c:\\foo\\compositing\\foo.html'
                ])

    def test_mocked_win(self):
        # This tests test_files.normalize, using portable behavior emulating
        # what we think Windows is supposed to do. This test will run on all
        # platforms.
        self.assert_filesystem_normalizes(MockWinFileSystem())

    def test_win(self):
        # This tests the actual windows platform, to ensure we get the same
        # results that we get in test_mocked_win().
        if sys.platform != 'win32':
            return
        self.assert_filesystem_normalizes(FileSystem())


class TestFind(unittest.TestCase):
    def test_basic(self):
        filesystem = MockFileSystem({
            '/base/a/1': '',
            '/base/a/2': '',
            '/base/b/1': '',
            '/base/c/2': '',
        })
        self.assertEqual(
            list(find_files.find(filesystem, '/base/', ['a'])),
            ['/base/a/1', '/base/a/2'])
        self.assertEqual(
            list(find_files.find(filesystem, '/base/', ['b', 'c'])),
            ['/base/b/1', '/base/c/2'])
        self.assertEqual(
            list(find_files.find(filesystem, '/base/', ['*/1'])),
            ['/base/a/1', '/base/b/1'])

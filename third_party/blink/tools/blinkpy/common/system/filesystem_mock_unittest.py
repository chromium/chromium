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

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system import filesystem_mock
from blinkpy.common.system import filesystem_unittest
from blinkpy.common.system.filesystem_mock import MockFileSystem

from collections import OrderedDict


class MockFileSystemTest(unittest.TestCase,
                         filesystem_unittest.GenericFileSystemTests):
    def setUp(self):
        self.fs = filesystem_mock.MockFileSystem()
        self.setup_generic_test_dir()

    def tearDown(self):
        self.teardown_generic_test_dir()
        self.fs = None

    def check_with_reference_function(self, test_function, good_function,
                                      tests):
        for test in tests:
            if (isinstance(test, tuple)):
                expected = good_function(*test)
                actual = test_function(*test)
            else:
                expected = good_function(test)
                actual = test_function(test)
            self.assertEqual(
                expected, actual,
                'given %s, expected %s, got %s' % (repr(test), repr(expected),
                                                   repr(actual)))

    def test_join(self):
        self.check_with_reference_function(self.fs.join,
                                           self.fs._slow_but_correct_join, [
                                               ('', ),
                                               ('', 'bar'),
                                               ('foo', ),
                                               ('foo/', ),
                                               ('foo', ''),
                                               ('foo/', ''),
                                               ('foo', 'bar'),
                                               ('foo', '/bar'),
                                           ])

    def test_normpath(self):
        self.check_with_reference_function(
            self.fs.normpath, self.fs._slow_but_correct_normpath, [
                '',
                '/',
                '.',
                '/.',
                'foo',
                'foo/',
                'foo/.',
                'foo/bar',
                '/foo',
                'foo/../bar',
                'foo/../bar/baz',
                '../foo',
            ])

    def test_abspath_given_abs_path(self):
        self.assertEqual(self.fs.abspath('/some/path'), '/some/path')

    def test_abspath_given_rel_path(self):
        self.fs.cwd = '/home/user'
        self.assertEqual(self.fs.abspath('docs/foo'), '/home/user/docs/foo')

    def test_abspath_given_rel_path_up_dir(self):
        self.fs.cwd = '/home/user'
        self.assertEqual(self.fs.abspath('../../etc'), '/etc')

    def test_relpath_down_one_dir(self):
        self.assertEqual(self.fs.relpath('/foo/bar/', '/foo/'), 'bar')

    def test_relpath_no_start_arg(self):
        self.fs.cwd = '/home/user'
        self.assertEqual(self.fs.relpath('/home/user/foo/bar'), 'foo/bar')

    def test_filesystem_walk(self):
        mock_dir = 'foo'
        mock_files = {'foo/bar/baz': '', 'foo/a': '', 'foo/b': '', 'foo/c': ''}
        host = MockHost()
        host.filesystem = MockFileSystem(files=mock_files)
        self.assertEquals(list(host.filesystem.walk(mock_dir)), [
            ('foo', ['bar'], ['a', 'b', 'c']),
            ('foo/bar', [], ['baz']),
        ])

    def test_filesystem_walk_deeply_nested(self):
        mock_dir = 'foo'
        mock_files = {
            'foo/bar/baz': '',
            'foo/bar/quux': '',
            'foo/a/x': '',
            'foo/a/y': '',
            'foo/a/z/lyrics': '',
            'foo/b': '',
            'foo/c': ''
        }
        mock_files_ordered = OrderedDict(sorted(mock_files.items()))
        host = MockHost()
        host.filesystem = MockFileSystem(files=mock_files_ordered)
        self.assertEquals(list(host.filesystem.walk(mock_dir)), [
            ('foo', ['a', 'bar'], ['b', 'c']),
            ('foo/a', ['z'], ['x', 'y']),
            ('foo/a/z', [], ['lyrics']),
            ('foo/bar', [], ['baz', 'quux']),
        ])

    def test_relpath_win32(self):
        # This unit test inherits tests from GenericFileSystemTests, but
        # test_relpath_win32 doesn't work with a mock filesystem since sep
        # is always '/' for MockFileSystem.
        # FIXME: Remove this.
        pass

    def test_file_permissions(self):
        mock_files = {'foo': '', 'bar': '', 'a': ''}
        filesystem = MockFileSystem(files=mock_files)
        filesystem.make_executable('foo')
        self.assertEquals(filesystem.executable_files, set(['foo']))

# Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import textwrap

from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.executive_mock import MockExecutive, ScriptError
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.test_copier import TestCopier

MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS

FAKE_SOURCE_REPO_DIR = '/blink/w3c'

FAKE_FILES = {
    MOCK_WEB_TESTS + 'external/OWNERS': b'',
    '/blink/w3c/dir/run.bat': b'',
    '/blink/w3c/dir/has_shebang.txt': b'#!',
    '/blink/w3c/dir/README.txt': b'',
    '/blink/w3c/dir/OWNERS': b'',
    '/blink/w3c/dir/DIR_METADATA': b'',
    '/blink/w3c/dir/reftest.list': b'',
    MOCK_WEB_TESTS + 'external/README.txt': b'',
    MOCK_WEB_TESTS + 'W3CImportExpectations': b'',
}


class TestCopierTest(LoggingTestCase):
    def test_import_dir_with_no_tests(self):
        host = MockHost()
        host.executive = MockExecutive(exception=ScriptError('error'))
        host.filesystem = MockFileSystem(files=FAKE_FILES)
        copier = TestCopier(host, FAKE_SOURCE_REPO_DIR)
        copier.do_import()  # No exception raised.

    def test_does_not_import_owner_files(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files=FAKE_FILES)
        copier = TestCopier(host, FAKE_SOURCE_REPO_DIR)
        copy_by_dir = copier.find_importable_tests()
        self.assertEqual(
            copy_by_dir, {
                '/blink/w3c/dir': [{
                    'dest': 'run.bat',
                    'src': '/blink/w3c/dir/run.bat'
                }, {
                    'dest': 'has_shebang.txt',
                    'src': '/blink/w3c/dir/has_shebang.txt'
                }, {
                    'dest': 'README.txt',
                    'src': '/blink/w3c/dir/README.txt'
                }],
            })

    def test_does_not_import_reftestlist_file(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files=FAKE_FILES)
        copier = TestCopier(host, FAKE_SOURCE_REPO_DIR)
        copy_by_dir = copier.find_importable_tests()
        self.assertEqual(
            copy_by_dir, {
                '/blink/w3c/dir': [{
                    'dest': 'run.bat',
                    'src': '/blink/w3c/dir/run.bat'
                }, {
                    'dest': 'has_shebang.txt',
                    'src': '/blink/w3c/dir/has_shebang.txt'
                }, {
                    'dest': 'README.txt',
                    'src': '/blink/w3c/dir/README.txt'
                }],
            })

    def test_executable_files(self):
        # Files with shebangs or .bat files need to be made executable.
        host = MockHost()
        host.filesystem = MockFileSystem(files=FAKE_FILES)
        copier = TestCopier(host, FAKE_SOURCE_REPO_DIR)
        copier.do_import()
        self.assertEqual(
            host.filesystem.executable_files, {
                MOCK_WEB_TESTS + 'external/w3c/dir/run.bat',
                MOCK_WEB_TESTS + 'external/w3c/dir/has_shebang.txt'
            })

    def test_ref_test_with_ref_is_copied(self):
        host = MockHost()
        host.filesystem = MockFileSystem(
            files={
                '/blink/w3c/dir1/my-ref-test.html':
                b'<html><head><link rel="match" href="ref-file.html" />test</head></html>',
                '/blink/w3c/dir1/ref-file.html':
                b'<html><head>test</head></html>',
                MOCK_WEB_TESTS + 'W3CImportExpectations':
                b'',
            })
        copier = TestCopier(host, FAKE_SOURCE_REPO_DIR)
        copy_by_dir = copier.find_importable_tests()
        self.assertEqual(len(copy_by_dir), 1)
        # The order of copy_list depends on the implementation of
        # filesystem.walk, so don't check the order
        self.assertCountEqual(copy_by_dir['/blink/w3c/dir1'],
                              [{
                                  'src': '/blink/w3c/dir1/ref-file.html',
                                  'dest': 'ref-file.html'
                              }, {
                                  'src': '/blink/w3c/dir1/my-ref-test.html',
                                  'dest': 'my-ref-test.html'
                              }])

    def test_skip_dir_but_copy_child_file(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files=FAKE_FILES)
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations',
            textwrap.dedent("""\
                # results: [ Pass Skip ]
                external/w3c/dir [ Skip ]
                external/w3c/dir/run.bat [ Pass ]
                """))
        copier = TestCopier(host, FAKE_SOURCE_REPO_DIR)
        copy_by_dir = copier.find_importable_tests()
        self.assertEqual(
            copy_by_dir, {
                '/blink/w3c/dir': [{
                    'dest': 'run.bat',
                    'src': '/blink/w3c/dir/run.bat',
                }],
            })

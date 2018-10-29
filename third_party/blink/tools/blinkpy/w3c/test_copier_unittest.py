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

from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.executive_mock import MockExecutive, ScriptError
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.test_copier import TestCopier


MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS

FAKE_SOURCE_REPO_DIR = '/blink'

FAKE_FILES = {
    MOCK_WEB_TESTS + 'external/OWNERS': '',
    '/blink/w3c/dir/has_shebang.txt': '#!',
    '/blink/w3c/dir/README.txt': '',
    '/blink/w3c/dir/OWNERS': '',
    '/blink/w3c/dir/reftest.list': '',
    MOCK_WEB_TESTS + 'external/README.txt': '',
    MOCK_WEB_TESTS + 'W3CImportExpectations': '',
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
        copier.find_importable_tests()
        self.assertEqual(
            copier.import_list,
            [
                {
                    'copy_list': [
                        {'dest': 'has_shebang.txt', 'src': '/blink/w3c/dir/has_shebang.txt'},
                        {'dest': 'README.txt', 'src': '/blink/w3c/dir/README.txt'}
                    ],
                    'dirname': '/blink/w3c/dir',
                }
            ])

    def test_does_not_import_reftestlist_file(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files=FAKE_FILES)
        copier = TestCopier(host, FAKE_SOURCE_REPO_DIR)
        copier.find_importable_tests()
        self.assertEqual(
            copier.import_list,
            [
                {
                    'copy_list': [
                        {'dest': 'has_shebang.txt', 'src': '/blink/w3c/dir/has_shebang.txt'},
                        {'dest': 'README.txt', 'src': '/blink/w3c/dir/README.txt'}
                    ],
                    'dirname': '/blink/w3c/dir',
                }
            ])

    def test_files_with_shebang_are_made_executable(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files=FAKE_FILES)
        copier = TestCopier(host, FAKE_SOURCE_REPO_DIR)
        copier.do_import()
        self.assertEqual(
            host.filesystem.executable_files,
            set([MOCK_WEB_TESTS + 'external/blink/w3c/dir/has_shebang.txt']))

    def test_ref_test_with_ref_is_copied(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files={
            '/blink/w3c/dir1/my-ref-test.html': '<html><head><link rel="match" href="ref-file.html" />test</head></html>',
            '/blink/w3c/dir1/ref-file.html': '<html><head>test</head></html>',
            MOCK_WEB_TESTS + 'W3CImportExpectations': '',
        })
        copier = TestCopier(host, FAKE_SOURCE_REPO_DIR)
        copier.find_importable_tests()
        self.assertEqual(
            copier.import_list,
            [
                {
                    'copy_list': [
                        {'src': '/blink/w3c/dir1/ref-file.html', 'dest': 'ref-file.html'},
                        {'src': '/blink/w3c/dir1/my-ref-test.html', 'dest': 'my-ref-test.html'}
                    ],
                    'dirname': '/blink/w3c/dir1',
                }
            ])

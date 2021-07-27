# Copyright (C) 2012 Balazs Ankes (bank@inf.u-szeged.hu) University of Szeged
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
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
"""Unit test for png.py."""

import unittest

from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.style.checkers.png import PNGChecker


class PNGCheckerTest(unittest.TestCase):
    """Tests PNGChecker class."""

    def test_init(self):
        """Test __init__() method."""

        def mock_handle_style_error(self):
            pass

        checker = PNGChecker("test/config", mock_handle_style_error,
                             MockSystemHost())
        self.assertEqual(checker._file_path, "test/config")
        self.assertEqual(checker._handle_style_error, mock_handle_style_error)

    def test_check(self):
        errors = []

        def mock_handle_style_error(line_number, category, confidence,
                                    message):
            error = (line_number, category, confidence, message)
            errors.append(error)

        fs = MockFileSystem()

        file_path = "foo.png"
        fs.write_binary_file(file_path, b"Dummy binary data")
        errors = []
        checker = PNGChecker(file_path, mock_handle_style_error,
                             MockSystemHost(os_name='linux', filesystem=fs))
        checker.check()
        self.assertEqual(len(errors), 0)

        file_path = "foo-expected.png"
        fs.write_binary_file(file_path, b"Dummy binary data")
        errors = []
        checker = PNGChecker(file_path, mock_handle_style_error,
                             MockSystemHost(os_name='linux', filesystem=fs))
        checker.check()
        self.assertEqual(len(errors), 1)
        self.assertEqual(errors[0], (
            0, 'image/png', 5,
            'Image lacks a checksum. Generate pngs using run_web_tests.py to ensure they have a checksum.'
        ))

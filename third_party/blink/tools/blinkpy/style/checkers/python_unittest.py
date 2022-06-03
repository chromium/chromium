# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""Unit tests for python.py."""

import os
import unittest

from blinkpy.style.checkers.python import PythonChecker


class PythonCheckerTest(unittest.TestCase):
    """Tests the PythonChecker class."""

    def test_init(self):
        """Test __init__() method."""

        def _mock_handle_style_error(self):
            pass

        checker = PythonChecker("foo.txt", _mock_handle_style_error)
        self.assertEqual(checker._file_path, "foo.txt")
        self.assertEqual(checker._handle_style_error, _mock_handle_style_error)

    # TODO(crbug.com/757067): Figure out why this is failing on LUCI mac/win.
    def disable_test_check(self):
        """Test check() method."""
        errors = []

        def _mock_handle_style_error(line_number, category, confidence,
                                     message):
            error = (line_number, category, confidence, message)
            errors.append(error)

        current_dir = os.path.dirname(__file__)
        file_path = os.path.join(current_dir, "python_unittest_input.py")

        checker = PythonChecker(file_path, _mock_handle_style_error)
        checker.check()

        self.assertEqual([
            (2, 'pep8/W291', 5, 'trailing whitespace'),
            (3, 'pep8/E261', 5, 'at least two spaces before inline comment'),
            (3, 'pep8/E262', 5, "inline comment should start with '# '"),
            (2, 'pylint/C0303(trailing-whitespace)', 5,
             '[] Trailing whitespace'),
            (2, 'pylint/E0602(undefined-variable)', 5,
             u"[] Undefined variable 'error'"),
            (3, 'pylint/W0611(unused-import)', 5, '[] Unused import math'),
        ], errors)

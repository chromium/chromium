# Copyright (C) 2010 Google Inc. All rights reserved.
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

from blinkpy.tool import grammar


class GrammarTest(unittest.TestCase):
    def test_join_with_separators_zero(self):
        self.assertEqual('', grammar.join_with_separators([]))

    def test_join_with_separators_one(self):
        self.assertEqual('one', grammar.join_with_separators(['one']))

    def test_join_with_separators_two(self):
        self.assertEqual('one and two',
                         grammar.join_with_separators(['one', 'two']))

    def test_join_with_separators_three(self):
        self.assertEqual('one, two, and three',
                         grammar.join_with_separators(['one', 'two', 'three']))

    def test_pluralize_zero(self):
        self.assertEqual('0 tests', grammar.pluralize('test', 0))

    def test_pluralize_one(self):
        self.assertEqual('1 test', grammar.pluralize('test', 1))

    def test_pluralize_two(self):
        self.assertEqual('2 tests', grammar.pluralize('test', 2))

    def test_pluralize_two_ends_with_sh(self):
        self.assertEqual('2 crashes', grammar.pluralize('crash', 2))

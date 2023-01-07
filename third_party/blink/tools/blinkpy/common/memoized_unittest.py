# Copyright (c) 2010 Google Inc. All rights reserved.
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

import unittest

from blinkpy.common.memoized import memoized


class _TestObject(object):
    def __init__(self):
        self.call_count = 0

    @memoized
    def memoized_add_one(self, argument):
        self.call_count += 1
        return argument + 1

    @memoized
    def memoized_subtract_one(self, argument):
        self.call_count += 1
        return argument - 1


class MemoizedTest(unittest.TestCase):
    def test_multiple_identical_calls(self):
        # When a function is called multiple times with identical arguments,
        # the call count doesn't increase past 1.
        test = _TestObject()
        self.assertEqual(test.memoized_add_one(1), 2)
        self.assertEqual(test.memoized_add_one(1), 2)
        self.assertEqual(test.call_count, 1)

    def test_different_calls(self):
        # Validate that call_count is working as expected.
        test = _TestObject()
        self.assertEqual(test.memoized_add_one(1), 2)
        self.assertEqual(test.memoized_add_one(2), 3)
        self.assertEqual(test.call_count, 2)

    def test_reassign_function(self):
        # The function can be assigned to a different variable.
        test = _TestObject()
        add_one = test.memoized_add_one
        self.assertEqual(add_one(4), 5)
        self.assertEqual(test.call_count, 1)

    def test_cache_clear(self):
        test = _TestObject()
        self.assertEqual(test.memoized_add_one(1), 2)
        self.assertEqual(test.memoized_subtract_one(2), 1)
        self.assertEqual(test.call_count, 2)

        # Now clear the cache of memoized_add_one. This should only clear the
        # cache for that function.
        test.memoized_add_one.cache_clear()

        self.assertEqual(test.memoized_subtract_one(2), 1)
        self.assertEqual(test.call_count, 2)

        self.assertEqual(test.memoized_add_one(1), 2)
        self.assertEqual(test.call_count, 3)

    def test_non_hashable_args(self):
        test = _TestObject()
        try:
            test.memoized_add_one([])
            self.fail('Expected TypeError.')
        except TypeError as error:
            self.assertEqual(
                str(error),
                'Cannot call memoized function memoized_add_one with '
                'unhashable arguments: unhashable type: \'list\'')

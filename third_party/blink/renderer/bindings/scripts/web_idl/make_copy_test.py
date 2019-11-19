# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from .composition_parts import Component
from .composition_parts import Identifier
from .make_copy import make_copy


class MakeCopyTest(unittest.TestCase):
    def test_primitives(self):
        self.assertEqual(None, make_copy(None))
        self.assertEqual(True, make_copy(True))
        self.assertEqual(False, make_copy(False))
        self.assertEqual(42, make_copy(42))
        self.assertEqual(3.14, make_copy(3.14))
        self.assertEqual('abc', make_copy('abc'))

    def test_primitives_subclasses(self):
        # Identifier and Component are subclasses of str.  Copies of them
        # shouldn't change their type.
        original = (Identifier('x'), Component('x'))
        copy = make_copy(original)
        self.assertEqual(original[0], copy[0])
        self.assertEqual(original[1], copy[1])
        self.assertIsInstance(copy[0], Identifier)
        self.assertIsInstance(copy[1], Component)

    def test_object_identity(self):
        # A diamond structure must be preserved when making a copy.
        #      /--> B --\
        #     A          --> D
        #      \--> C --/
        # A1->B1, A1->C1, B1->D1, C1->D1 will be copied as;
        # A2->B2, A2->C2, B2->D2, C2->D2 where X2 is a copy of X1.

        class Obj(object):
            pass

        class Ref(object):
            def __init__(self, value=None):
                self.value = value

        obj = Obj()
        ref1 = Ref(obj)
        ref2 = Ref(obj)
        self.assertNotEqual(ref1, ref2)
        self.assertIs(ref1.value, ref2.value)

        copy = make_copy((ref1, ref2))
        self.assertIsInstance(copy, tuple)
        self.assertIsInstance(copy[0], Ref)
        self.assertIsInstance(copy[1], Ref)
        self.assertIsNot(copy[0], copy[1])
        self.assertIs(copy[0].value, copy[1].value)

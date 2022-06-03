# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import unittest

from mojom.generate import module as mojom


class ModuleTest(unittest.TestCase):
  def testNonInterfaceAsInterfaceRequest(self):
    """Tests that a non-interface cannot be used for interface requests."""
    module = mojom.Module('test_module', 'test_namespace')
    struct = mojom.Struct('TestStruct', module=module)
    with self.assertRaises(Exception) as e:
      mojom.InterfaceRequest(struct)
    self.assertEquals(
        e.exception.__str__(),
        'Interface request requires \'x:TestStruct\' to be an interface.')

  def testNonInterfaceAsAssociatedInterface(self):
    """Tests that a non-interface type cannot be used for associated interfaces.
    """
    module = mojom.Module('test_module', 'test_namespace')
    struct = mojom.Struct('TestStruct', module=module)
    with self.assertRaises(Exception) as e:
      mojom.AssociatedInterface(struct)
    self.assertEquals(
        e.exception.__str__(),
        'Associated interface requires \'x:TestStruct\' to be an interface.')

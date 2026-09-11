# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import unittest

from mojom.generate import module as mojom


class ModuleTest(unittest.TestCase):
  def testNonInterfaceAsPendingReceiver(self):
    module = mojom.Module('test_module', 'test_namespace')
    struct = mojom.Struct('TestStruct', module=module)
    with self.assertRaises(Exception) as e:
      mojom.PendingReceiver(struct)
    self.assertEqual(
      e.exception.__str__(),
      'pending_receiver<T> requires T to be an interface type. '
      'Got \'x:TestStruct\'',
    )

  def testNonInterfaceAsPendingRemote(self):
    module = mojom.Module('test_module', 'test_namespace')
    struct = mojom.Struct('TestStruct', module=module)
    with self.assertRaises(Exception) as e:
      mojom.PendingRemote(struct)
    self.assertEqual(
      e.exception.__str__(),
      'pending_remote<T> requires T to be an interface type. '
      'Got \'x:TestStruct\'',
    )

  def testNonInterfaceAsPendingAssociatedReceiver(self):
    module = mojom.Module('test_module', 'test_namespace')
    struct = mojom.Struct('TestStruct', module=module)
    with self.assertRaises(Exception) as e:
      mojom.PendingAssociatedReceiver(struct)
    self.assertEqual(
      e.exception.__str__(),
      'pending_associated_receiver<T> requires T to be an interface type. '
      'Got \'x:TestStruct\'',
    )

  def testNonInterfaceAsPendingAssociatedRemote(self):
    module = mojom.Module('test_module', 'test_namespace')
    struct = mojom.Struct('TestStruct', module=module)
    with self.assertRaises(Exception) as e:
      mojom.PendingAssociatedRemote(struct)
    self.assertEqual(
      e.exception.__str__(),
      'pending_associated_remote<T> requires T to be an interface type. '
      'Got \'x:TestStruct\'',
    )

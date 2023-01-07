#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
DATA_DIR = os.path.join(SCRIPT_DIR, 'data')

sys.path.append(PARENT_DIR)

import elf


class TestIsDynamicElf(unittest.TestCase):
  def test_arm(self):
    static_nexe = os.path.join(DATA_DIR, 'test_static_arm.nexe')
    self.assertFalse(elf.IsDynamicElf(static_nexe, False))

  def test_x86_32(self):
    dyn_nexe = os.path.join(DATA_DIR, 'test_dynamic_x86_32.nexe')
    static_nexe = os.path.join(DATA_DIR, 'test_static_x86_32.nexe')
    self.assertTrue(elf.IsDynamicElf(dyn_nexe, False))
    self.assertFalse(elf.IsDynamicElf(static_nexe, False))

  def test_x86_64(self):
    dyn_nexe = os.path.join(DATA_DIR, 'test_dynamic_x86_64.nexe')
    static_nexe = os.path.join(DATA_DIR, 'test_static_x86_64.nexe')
    self.assertTrue(elf.IsDynamicElf(dyn_nexe, True))
    self.assertFalse(elf.IsDynamicElf(static_nexe, True))


class TestParseElfHeader(unittest.TestCase):
  def test_invalid_elf(self):
    self.assertRaises(elf.Error, elf.ParseElfHeader, __file__)

  def test_arm_elf_parse(self):
    """Test parsing of ARM elf header."""
    static_nexe = os.path.join(DATA_DIR, 'test_static_arm.nexe')
    arch, dynamic = elf.ParseElfHeader(static_nexe)
    self.assertEqual(arch, 'arm')
    self.assertFalse(dynamic)

  def test_x86_32_elf_parse(self):
    """Test parsing of x86-32 elf header."""
    dyn_nexe = os.path.join(DATA_DIR, 'test_dynamic_x86_32.nexe')
    static_nexe = os.path.join(DATA_DIR, 'test_static_x86_32.nexe')

    arch, dynamic = elf.ParseElfHeader(dyn_nexe)
    self.assertEqual(arch, 'x86-32')
    self.assertTrue(dynamic)

    arch, dynamic = elf.ParseElfHeader(static_nexe)
    self.assertEqual(arch, 'x86-32')
    self.assertFalse(dynamic)

  def test_x86_64_elf_parse(self):
    """Test parsing of x86-64 elf header."""
    dyn_nexe = os.path.join(DATA_DIR, 'test_dynamic_x86_64.nexe')
    static_nexe = os.path.join(DATA_DIR, 'test_static_x86_64.nexe')

    arch, dynamic = elf.ParseElfHeader(dyn_nexe)
    self.assertEqual(arch, 'x86-64')
    self.assertTrue(dynamic)

    arch, dynamic = elf.ParseElfHeader(static_nexe)
    self.assertEqual(arch, 'x86-64')
    self.assertFalse(dynamic)


if __name__ == '__main__':
  unittest.main()

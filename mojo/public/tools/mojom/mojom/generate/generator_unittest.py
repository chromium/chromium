# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import importlib.util
import os.path
import sys
import unittest

def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    assert tail
    if tail == dirname:
      return path


try:
  importlib.util.find_spec("mojom")
except ImportError:
  sys.path.append(os.path.join(_GetDirAbove("pylib"), "pylib"))
from mojom.generate import generator

class StringManipulationTest(unittest.TestCase):
  """generator contains some string utilities, this tests only those."""

  def testSplitCamelCase(self):
    self.assertEqual(["camel", "case"], generator.SplitCamelCase("CamelCase"))
    self.assertEqual(["url", "loader", "factory"],
                     generator.SplitCamelCase('URLLoaderFactory'))
    self.assertEqual(["get99", "entries"],
                     generator.SplitCamelCase('Get99Entries'))
    self.assertEqual(["get99entries"], generator.SplitCamelCase('Get99entries'))

  def testToCamel(self):
    self.assertEqual("CamelCase", generator.ToCamel("camel_case"))
    self.assertEqual("CAMELCASE", generator.ToCamel("CAMEL_CASE"))
    self.assertEqual("camelCase",
                     generator.ToCamel("camel_case", lower_initial=True))
    self.assertEqual("CamelCase", generator.ToCamel("camel case",
                                                    delimiter=' '))
    self.assertEqual("CaMelCaSe", generator.ToCamel("caMel_caSe"))
    self.assertEqual("L2Tp", generator.ToCamel("l2tp", digits_split=True))
    self.assertEqual("l2tp", generator.ToCamel("l2tp", lower_initial=True))

  def testToSnakeCase(self):
    self.assertEqual("snake_case", generator.ToLowerSnakeCase("SnakeCase"))
    self.assertEqual("snake_case", generator.ToLowerSnakeCase("snakeCase"))
    self.assertEqual("snake_case", generator.ToLowerSnakeCase("SnakeCASE"))
    self.assertEqual("snake_d3d11_case",
                     generator.ToLowerSnakeCase("SnakeD3D11Case"))
    self.assertEqual("snake_d3d11_case",
                     generator.ToLowerSnakeCase("SnakeD3d11Case"))
    self.assertEqual("snake_d3d11_case",
                     generator.ToLowerSnakeCase("snakeD3d11Case"))
    self.assertEqual("SNAKE_CASE", generator.ToUpperSnakeCase("SnakeCase"))
    self.assertEqual("SNAKE_CASE", generator.ToUpperSnakeCase("snakeCase"))
    self.assertEqual("SNAKE_CASE", generator.ToUpperSnakeCase("SnakeCASE"))
    self.assertEqual("SNAKE_D3D11_CASE",
                     generator.ToUpperSnakeCase("SnakeD3D11Case"))
    self.assertEqual("SNAKE_D3D11_CASE",
                     generator.ToUpperSnakeCase("SnakeD3d11Case"))
    self.assertEqual("SNAKE_D3D11_CASE",
                     generator.ToUpperSnakeCase("snakeD3d11Case"))

if __name__ == "__main__":
  unittest.main()

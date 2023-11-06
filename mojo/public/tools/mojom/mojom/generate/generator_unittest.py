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
    self.assertEquals(["camel", "case"], generator.SplitCamelCase("CamelCase"))
    self.assertEquals(["url", "loader", "factory"],
                      generator.SplitCamelCase('URLLoaderFactory'))
    self.assertEquals(["get99", "entries"],
                      generator.SplitCamelCase('Get99Entries'))
    self.assertEquals(["get99entries"],
                      generator.SplitCamelCase('Get99entries'))

  def testToCamel(self):
    self.assertEquals("CamelCase", generator.ToCamel("camel_case"))
    self.assertEquals("CAMELCASE", generator.ToCamel("CAMEL_CASE"))
    self.assertEquals("camelCase",
                      generator.ToCamel("camel_case", lower_initial=True))
    self.assertEquals("CamelCase", generator.ToCamel(
        "camel case", delimiter=' '))
    self.assertEquals("CaMelCaSe", generator.ToCamel("caMel_caSe"))
    self.assertEquals("L2Tp", generator.ToCamel("l2tp", digits_split=True))
    self.assertEquals("l2tp", generator.ToCamel("l2tp", lower_initial=True))

  def testToSnakeCase(self):
    self.assertEquals("snake_case", generator.ToLowerSnakeCase("SnakeCase"))
    self.assertEquals("snake_case", generator.ToLowerSnakeCase("snakeCase"))
    self.assertEquals("snake_case", generator.ToLowerSnakeCase("SnakeCASE"))
    self.assertEquals("snake_d3d11_case",
                      generator.ToLowerSnakeCase("SnakeD3D11Case"))
    self.assertEquals("snake_d3d11_case",
                      generator.ToLowerSnakeCase("SnakeD3d11Case"))
    self.assertEquals("snake_d3d11_case",
                      generator.ToLowerSnakeCase("snakeD3d11Case"))
    self.assertEquals("SNAKE_CASE", generator.ToUpperSnakeCase("SnakeCase"))
    self.assertEquals("SNAKE_CASE", generator.ToUpperSnakeCase("snakeCase"))
    self.assertEquals("SNAKE_CASE", generator.ToUpperSnakeCase("SnakeCASE"))
    self.assertEquals("SNAKE_D3D11_CASE",
                      generator.ToUpperSnakeCase("SnakeD3D11Case"))
    self.assertEquals("SNAKE_D3D11_CASE",
                      generator.ToUpperSnakeCase("SnakeD3d11Case"))
    self.assertEquals("SNAKE_D3D11_CASE",
                      generator.ToUpperSnakeCase("snakeD3d11Case"))

if __name__ == "__main__":
  unittest.main()

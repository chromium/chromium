#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""code generator for OpenGL ES 2.0 conformance tests."""

import os
import re
import sys
import typing

def ReadFileAsLines(filename: str) -> typing.List[str]:
  """Reads a file, removing blank lines and lines that start with #"""
  with open(filename, "r") as in_file:
    raw_lines = in_file.readlines()
  lines = []
  for line in raw_lines:
    line = line.strip()
    if len(line) > 0 and not line.startswith("#"):
      lines.append(line)
  return lines


def GenerateTests(out_file: typing.IO) -> None:
  """Generates gles2_conform_test_autogen.cc"""

  tests = ReadFileAsLines(
      "../../third_party/gles2_conform/GTF_ES/glsl/GTF/mustpass_es20.run")

  out_file.write("""
#include "gpu/gles2_conform_support/gles2_conform_test.h"
#include "testing/gtest/include/gtest/gtest.h"
""".encode("utf8"))

  for test in tests:
    out_file.write(("""
TEST(GLES2ConformTest, %(name)s) {
  EXPECT_TRUE(RunGLES2ConformTest("%(path)s"));
}
""" % {
        "name": re.sub(r'[^A-Za-z0-9]', '_', test),
        "path": test,
      }).encode("utf8"))


def main(argv: typing.List[str]) -> int:
  """This is the main function."""

  if len(argv) >= 1:
    out_dir = argv[0]
  else:
    out_dir = '.'

  out_filename = os.path.join(out_dir, 'gles2_conform_test_autogen.cc')
  with open(out_filename, 'wb') as out_file:
    GenerateTests(out_file)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

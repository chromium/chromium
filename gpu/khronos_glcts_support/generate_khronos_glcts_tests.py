#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code generator for khronos_glcts tests."""

import argparse
import os
import re
import sys
import typing

TEST_DEF_TEMPLATE = """
TEST(KhronosGLCTSTest, %(gname)s) {
  EXPECT_TRUE(RunKhronosGLCTSTest("%(cname)s"));
}
"""

RUN_FILE_SUITE_PREFIX = {
  "mustpass_es20.run" : "ES2-CTS.gtf",
}

BUILT_IN_TESTS = {
  "mustpass_es20.run" : [
    "CTS-Configs.es2",
    "ES2-CTS.info.vendor",
    "ES2-CTS.info.renderer",
    "ES2-CTS.info.version",
    "ES2-CTS.info.shading_language_version",
    "ES2-CTS.info.extensions",
    "ES2-CTS.info.render_target",
  ],
}

def ReadFileAsLines(filename: str) -> typing.Generator[str, None, None]:
  """
    Reads a file, yielding each non-blank line
    and lines that don't begin with #
  """
  with open(filename, "r") as in_file:
    lines = in_file.readlines()
  for line in lines:
    line = line.strip()
    if len(line) > 0 and not line.startswith("#"):
      yield line

def ReadRunFile(run_file: str) -> typing.List[str]:
  """
    Find all .test tests in a .run file and return their paths.
    If the .run file contains another .run file, then that is inspected
    too.
  """
  tests = []
  base_dir = os.path.dirname(run_file)
  for line in ReadFileAsLines(run_file):
    _, ext = os.path.splitext(line)
    if ext == ".test":
      tests.append(os.path.join(base_dir, line))
    elif ext == ".run":
      tests += ReadRunFile(os.path.join(base_dir, line))
    else:
      raise ValueError("Unexpected line '%s' in '%s'" % (line, run_file))
  return tests

def GenerateTests(run_files: typing.List[str], output: typing.IO) -> None:
  """
    Generates code for khronos_glcts_test test-cases that are
    listed in the run_files.
  """
  output.write('#include "gpu/khronos_glcts_support/khronos_glcts_test.h"\n')
  output.write('#include "testing/gtest/include/gtest/gtest.h"\n\n')

  for run_file in run_files:
    run_file_name = os.path.basename(run_file)
    run_file_dir = os.path.dirname(run_file)
    suite_prefix = RUN_FILE_SUITE_PREFIX[run_file_name]
    output.write("// " + run_file_name + "\n")
    builtin_tests = BUILT_IN_TESTS[run_file_name]
    for test in builtin_tests:
      output.write(TEST_DEF_TEMPLATE
        % {
          "gname": re.sub(r'[^A-Za-z0-9]', '_', test),
          "cname": test,
        })
    for test in ReadRunFile(run_file):
      rel_path = os.path.relpath(test, run_file_dir)
      root, _ = os.path.splitext(rel_path)
      name = root.replace('.', '_')
      name = "%s.%s" % (suite_prefix, name.replace(os.path.sep, '.'))
      output.write(TEST_DEF_TEMPLATE
        % {
          "gname": re.sub(r'[^A-Za-z0-9]', '_', name),
          "cname": name,
        })
    output.write("\n");

def main() -> int:
  """This is the main function."""
  parser = argparse.ArgumentParser()
  parser.add_argument("--outdir", default = ".")
  parser.add_argument("run_files", nargs = "+")

  args = parser.parse_args()

  output = open(
    os.path.join(args.outdir, "khronos_glcts_test_autogen.cc"), "w")

  try:
    GenerateTests(args.run_files, output)
  finally:
    output.close()

  return 0

if __name__ == '__main__':
  sys.exit(main())

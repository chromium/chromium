#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple utility which concatenates a set of files into a single output file
while also stripping any goog.provide or goog.require lines. This allows us to
provide a very primitive sort of "compilation" without any extra toolchain
support and without having to modify otherwise compilable sources in the tree
which use these directives.

goog.provide lines are replaced with an equivalent invocation of
mojo.internal.exportModule, which accomplishes essentially the same thing in an
uncompiled context. A singular exception is made for the 'mojo.internal' export,
which is instead replaced with an inlined assignment to initialize the
namespace.
"""

from __future__ import print_function

import optparse
import re


_MOJO_INTERNAL_MODULE_NAME = "mojo.internal"
_MOJO_EXPORT_MODULE_SYMBOL = "mojo.internal.exportModule"


def FilterLine(filename, line, output):
  if line.startswith("goog.require"):
    return

  if line.startswith("goog.provide"):
    match = re.match("goog.provide\('([^']+)'\);", line)
    if not match:
      print("Invalid goog.provide line in %s:\n%s" % (filename, line))
      exit(1)

    module_name = match.group(1)
    if module_name == _MOJO_INTERNAL_MODULE_NAME:
      output.write("self.mojo = { internal: {} };")
    else:
      output.write("%s('%s');\n" % (_MOJO_EXPORT_MODULE_SYMBOL, module_name))
    return

  output.write(line)

def ConcatenateAndReplaceExports(filenames):
  if (len(filenames) < 2):
    print("At least two filenames (one input and the output) are required.")
    return False

  try:
    with open(filenames[-1], "wb") as target:
      for filename in filenames[:-1]:
        with open(filename, "rb") as current:
          for line in current.readlines():
            FilterLine(filename, line, target)
    return True
  except IOError as e:
    print("Error generating %s\n: %s" % (filenames[-1], e))
    return False

def main():
  parser = optparse.OptionParser()
  parser.set_usage("""file1 [file2...] outfile
    Concatenate several files into one, stripping Closure provide and
    require directives along the way.""")
  (_, args) = parser.parse_args()
  exit(0 if ConcatenateAndReplaceExports(args) else 1)

if __name__ == "__main__":
  main()

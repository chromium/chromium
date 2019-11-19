#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Test runner for IDL Generator changes """

from __future__ import print_function

import subprocess
import sys

def TestIDL(testname, args):
  print('\nRunning unit tests for %s.' % testname)
  try:
    args = [sys.executable, testname] + args
    subprocess.check_call(args)
    return 0
  except subprocess.CalledProcessError as err:
    print('Failed with %s.' % str(err))
    return 1

def main(args):
  errors = 0
  errors += TestIDL('idl_lexer.py', ['--test'])
  assert errors == 0
  errors += TestIDL('idl_parser.py', ['--test'])
  assert errors == 0
  errors += TestIDL('idl_c_header.py', [])
  assert errors == 0
  errors += TestIDL('idl_c_proto.py', ['--wnone', '--test'])
  assert errors == 0
  errors += TestIDL('idl_gen_pnacl.py', ['--wnone', '--test'])
  assert errors == 0
  errors += TestIDL('idl_namespace.py', [])
  assert errors == 0
  errors += TestIDL('idl_node.py', [])
  assert errors == 0

  if errors:
    print('\nFailed tests.')
  return errors


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

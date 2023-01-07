#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper script OSX native linker to handle lack of linker script support.
"""

import sys
import os

SYMS = ('PSUserMainGet', '__nacl_main', 'PPP_InitializeModule')

debug = False

def main(args):
  assert(args)
  if '-lppapi_simple' in args:
    args[args.index('-lppapi_simple')] = '-lppapi_simple_real'
    for s in SYMS:
      args += ['-Wl,-u', '-Wl,_' + s]

  if '-lppapi_simple_cpp' in args:
    args[args.index('-lppapi_simple_cpp')] = '-lppapi_simple_cpp_real'
    for s in SYMS:
      args += ['-Wl,-u', '-Wl,_' + s]

  if debug:
    print ' '.join(args)
  os.execvp(args[0], args)
  # should never get here
  return 1

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

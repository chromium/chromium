#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import sys
import traceback

# Note: some of these files are imported to register cmdline options.
from idl_generator import Generator
from idl_option import ParseOptions
from idl_outfile import IDLOutFile
from idl_parser import ParseFiles
from idl_c_header import HGen
from idl_thunk import TGen
from idl_gen_pnacl import PnaclGen


def Main(args):
  # If no arguments are provided, assume we are trying to rebuild the
  # C headers with warnings off.
  try:
    if not args:
      args = [
          '--wnone', '--cgen', '--range=start,end',
          '--pnacl', '--pnaclshim',
          '../native_client/src/untrusted/pnacl_irt_shim/pnacl_shim.c',
          '--tgen',
      ]
      current_dir = os.path.abspath(os.getcwd())
      script_dir = os.path.abspath(os.path.dirname(__file__))
      if current_dir != script_dir:
        print('\nIncorrect CWD, default run skipped.')
        print(
            'When running with no arguments set CWD to the scripts directory:')
        print('\t' + script_dir + '\n')
        print('This ensures correct default paths and behavior.\n')
        return 1

    filenames = ParseOptions(args)
    ast = ParseFiles(filenames)
    if ast.errors:
      print('Found %d errors.  Aborting build.\n' % ast.errors)
      return 1
    return Generator.Run(ast)
  except SystemExit as ec:
    print('Exiting with %d' % ec.code)
    sys.exit(ec.code)

  except:
    typeinfo, value, tb = sys.exc_info()
    traceback.print_exception(typeinfo, value, tb)
    print('Called with: ' + ' '.join(sys.argv))


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to repack paks for a list of locales.

Gyp doesn't have any built-in looping capability, so this just provides a way to
loop over a list of locales when repacking pak files, thus avoiding a
proliferation of mostly duplicate, cut-n-paste gyp actions.
"""

import optparse
import os
import sys

# Prepend the grit module from the source tree so it takes precedence over other
# grit versions that might present in the search path.
sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..', '..', '..',
                                'tools', 'grit'))
from grit.format import data_pack

# Some build paths defined by gyp.
GRIT_DIR = None
INT_DIR = None

# The target platform. If it is not defined, sys.platform will be used.
OS = None

# Extra input files.
EXTRA_INPUT_FILES = []

class Usage(Exception):
  def __init__(self, msg):
    self.msg = msg


def calc_output(locale):
  """Determine the file that will be generated for the given locale."""
  #e.g. '<(INTERMEDIATE_DIR)/remoting_locales/da.pak',
  if OS == 'mac' or OS == 'ios':
    # For Cocoa to find the locale at runtime, it needs to use '_' instead
    # of '-' (http://crbug.com/20441).
    return os.path.join(INT_DIR, 'remoting', 'resources',
                        '%s.lproj' % locale.replace('-', '_'), 'locale.pak')
  else:
    return os.path.join(INT_DIR, 'remoting_locales', locale + '.pak')


def calc_inputs(locale):
  """Determine the files that need processing for the given locale."""
  inputs = []

  #e.g. '<(grit_out_dir)/remoting/resources/da.pak'
  inputs.append(os.path.join(GRIT_DIR, 'remoting/resources/%s.pak' % locale))

  # Add any extra input files.
  for extra_file in EXTRA_INPUT_FILES:
    inputs.append('%s_%s.pak' % (extra_file, locale))

  return inputs


def list_outputs(locales):
  """Returns the names of files that will be generated for the given locales.

  This is to provide gyp the list of output files, so build targets can
  properly track what needs to be built.
  """
  outputs = []
  for locale in locales:
    outputs.append(calc_output(locale))
  # Quote each element so filename spaces don't mess up gyp's attempt to parse
  # it into a list.
  return " ".join(['"%s"' % x for x in outputs])


def list_inputs(locales):
  """Returns the names of files that will be processed for the given locales.

  This is to provide gyp the list of input files, so build targets can properly
  track their prerequisites.
  """
  inputs = []
  for locale in locales:
    inputs += calc_inputs(locale)
  # Quote each element so filename spaces don't mess up gyp's attempt to parse
  # it into a list.
  return " ".join(['"%s"' % x for x in inputs])


def repack_locales(locales):
  """ Loop over and repack the given locales."""
  for locale in locales:
    inputs = calc_inputs(locale)
    output = calc_output(locale)
    data_pack.RePack(output, inputs)


def DoMain(argv):
  global GRIT_DIR
  global INT_DIR
  global OS
  global EXTRA_INPUT_FILES

  parser = optparse.OptionParser("usage: %prog [options] locales")
  parser.add_option("-i", action="store_true", dest="inputs", default=False,
                    help="Print the expected input file list, then exit.")
  parser.add_option("-o", action="store_true", dest="outputs", default=False,
                    help="Print the expected output file list, then exit.")
  parser.add_option("-g", action="store", dest="grit_dir",
                    help="GRIT build files output directory.")
  parser.add_option("-x", action="store", dest="int_dir",
                    help="Intermediate build files output directory.")
  parser.add_option("-e", action="append", dest="extra_input", default=[],
                    help="Full path to an extra input pak file without the\
                         locale suffix and \".pak\" extension.")
  parser.add_option("-p", action="store", dest="os",
                    help="The target OS. (e.g. mac, linux, win, etc.)")
  options, locales = parser.parse_args(argv)

  if not locales:
    parser.error('Please specificy at least one locale to process.\n')

  print_inputs = options.inputs
  print_outputs = options.outputs
  GRIT_DIR = options.grit_dir
  INT_DIR = options.int_dir
  EXTRA_INPUT_FILES = options.extra_input
  OS = options.os

  if not OS:
    if sys.platform == 'darwin':
      OS = 'mac'
    elif sys.platform.startswith('linux'):
      OS = 'linux'
    elif sys.platform in ('cygwin', 'win32'):
      OS = 'win'
    else:
      OS = sys.platform

  if print_inputs and print_outputs:
    parser.error('Please specify only one of "-i" or "-o".\n')
  if print_inputs and not GRIT_DIR:
    parser.error('Please specify "-g".\n')
  if print_outputs and not INT_DIR:
    parser.error('Please specify "-x".\n')
  if not (print_inputs or print_outputs  or (GRIT_DIR and INT_DIR)):
    parser.error('Please specify both "-g" and "-x".\n')

  if print_inputs:
    return list_inputs(locales)

  if print_outputs:
    return list_outputs(locales)

  return repack_locales(locales)

if __name__ == '__main__':
  results = DoMain(sys.argv[1:])
  if results:
    print results

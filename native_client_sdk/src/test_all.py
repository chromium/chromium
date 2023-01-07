#!/usr/bin/env vpython3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top level script for running all python unittests in the NaCl SDK.
"""

from __future__ import print_function

import argparse
import os
import subprocess
import sys
import unittest

# add tools folder to sys.path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.join(SCRIPT_DIR, 'tools')
BUILD_TOOLS_DIR = os.path.join(SCRIPT_DIR, 'build_tools')

sys.path.append(TOOLS_DIR)
sys.path.append(os.path.join(TOOLS_DIR, 'tests'))
sys.path.append(os.path.join(TOOLS_DIR, 'lib', 'tests'))
sys.path.append(BUILD_TOOLS_DIR)
sys.path.append(os.path.join(BUILD_TOOLS_DIR, 'tests'))

import build_paths

PKG_VER_DIR = os.path.join(build_paths.NACL_DIR, 'build', 'package_version')
TAR_DIR = os.path.join(build_paths.NACL_DIR, 'toolchain', '.tars')

PKG_VER = os.path.join(PKG_VER_DIR, 'package_version.py')

EXTRACT_PACKAGES = ['nacl_x86_glibc', 'nacl_arm_glibc']
TOOLCHAIN_OUT = os.path.join(build_paths.OUT_DIR, 'sdk_tests', 'toolchain')

# List of modules containing unittests. The goal is to keep the total
# runtime of these tests under 2 seconds. Any slower tests should go
# in TEST_MODULES_BIG.
TEST_MODULES = [
    'build_artifacts_test',
    'build_version_test',
    'create_html_test',
    'create_nmf_test',
    'easy_template_test',
    'elf_test',
    'fix_deps_test',
    'getos_test',
    'get_shared_deps_test',
    'httpd_test',
    'nacl_config_test',
    'oshelpers_test',
    'parse_dsc_test',
    'quote_test',
    'sdktools_config_test',
    'sel_ldr_test',
    'test_projects_test',
    'update_nacl_manifest_test',
    'verify_filelist_test',
    'verify_ppapi_test',
]


# Slower tests. For example the 'sdktools' are mostly slower system tests
# that longer to run.  If --quick is passed then we don't run these.
TEST_MODULES_BIG = [
    'sdktools_commands_test',
    'sdktools_test',
]


def ExtractToolchains():
  cmd = [sys.executable, PKG_VER,
         '--packages', ','.join(EXTRACT_PACKAGES),
         '--tar-dir', TAR_DIR,
         '--dest-dir', TOOLCHAIN_OUT,
         'extract']
  subprocess.check_call(cmd)


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-v', '--verbose', action='store_true')
  parser.add_argument('--quick', action='store_true')
  options = parser.parse_args(args)

  # Some of the unit tests use parts of toolchains. Extract to TOOLCHAIN_OUT.
  print('Extracting toolchains...')
  ExtractToolchains()

  suite = unittest.TestSuite()
  modules = TEST_MODULES
  if not options.quick:
    modules += TEST_MODULES_BIG

  for module_name in modules:
    module = __import__(module_name)
    suite.addTests(unittest.defaultTestLoader.loadTestsFromModule(module))

  if options.verbose:
    verbosity = 2
  else:
    verbosity = 1

  print('Running unittests...')
  result = unittest.TextTestRunner(verbosity=verbosity).run(suite)
  return int(not result.wasSuccessful())


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

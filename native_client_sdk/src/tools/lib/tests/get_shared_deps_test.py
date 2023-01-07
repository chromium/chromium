#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
LIB_DIR = os.path.dirname(SCRIPT_DIR)
TOOLS_DIR = os.path.dirname(LIB_DIR)
SDK_DIR = os.path.dirname(TOOLS_DIR)
DATA_DIR = os.path.join(SCRIPT_DIR, 'data')
BUILD_TOOLS_DIR = os.path.join(SDK_DIR, 'build_tools')

sys.path.append(LIB_DIR)
sys.path.append(TOOLS_DIR)
sys.path.append(BUILD_TOOLS_DIR)

import build_paths
import get_shared_deps
import getos

TOOLCHAIN_OUT = os.path.join(build_paths.OUT_DIR, 'sdk_tests', 'toolchain')
NACL_X86_GLIBC_TOOLCHAIN = os.path.join(TOOLCHAIN_OUT,
                                        '%s_x86' % getos.GetPlatform(),
                                        'nacl_x86_glibc')


def StripDependencies(deps):
  '''Strip the dirnames and version suffixes from
  a list of nexe dependencies.

  e.g:
  /path/to/libpthread.so.1a2d3fsa -> libpthread.so
  '''
  names = []
  for name in deps:
    name = os.path.basename(name)
    if '.so.' in name:
      name = name.rsplit('.', 1)[0]
    names.append(name)
  return names


class TestGetNeeded(unittest.TestCase):
  def setUp(self):
    self.tempdir = None
    self.toolchain = NACL_X86_GLIBC_TOOLCHAIN
    self.objdump = os.path.join(self.toolchain, 'bin', 'i686-nacl-objdump')
    if os.name == 'nt':
      self.objdump += '.exe'
    self.Mktemp()
    self.dyn_nexe = self.createTestNexe('test_dynamic_x86_32.nexe', 'i686')
    self.dyn_deps = set(['libc.so', 'runnable-ld.so',
                         'libgcc_s.so', 'libpthread.so'])

  def tearDown(self):
    if self.tempdir:
      shutil.rmtree(self.tempdir)

  def Mktemp(self):
    self.tempdir = tempfile.mkdtemp()

  def createTestNexe(self, name, arch):
    '''Create an empty test .nexe file for use in create_nmf tests.

    This is used rather than checking in test binaries since the
    checked in binaries depend on .so files that only exist in the
    certain SDK that built them.
    '''
    compiler = os.path.join(self.toolchain, 'bin', '%s-nacl-g++' % arch)
    if os.name == 'nt':
      compiler += '.exe'
      os.environ['CYGWIN'] = 'nodosfilewarning'
    program = 'int main() { return 0; }'
    name = os.path.join(self.tempdir, name)
    cmd = [compiler, '-pthread', '-x' , 'c', '-o', name, '-']
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    p.communicate(input=program)
    self.assertEqual(p.returncode, 0)
    return name

  def testStatic(self):
    nexe = os.path.join(DATA_DIR, 'test_static_x86_32.nexe')
    # GetNeeded should not raise an error if objdump is not set, but the .nexe
    # is statically linked.
    objdump = None
    lib_path = []
    needed = get_shared_deps.GetNeeded([nexe], objdump, lib_path)

    # static nexe should have exactly one needed file
    self.assertEqual(len(needed), 1)
    self.assertEqual(needed.keys()[0], nexe)

    # arch of needed file should be x86-32
    arch = needed.values()[0]
    self.assertEqual(arch, 'x86-32')

  def testDynamic(self):
    libdir = os.path.join(self.toolchain, 'x86_64-nacl', 'lib32')
    needed = get_shared_deps.GetNeeded([self.dyn_nexe],
                                       lib_path=[libdir],
                                       objdump=self.objdump)
    names = needed.keys()

    # this nexe has 5 dependencies
    expected = set(self.dyn_deps)
    expected.add(os.path.basename(self.dyn_nexe))

    basenames = set(StripDependencies(names))
    self.assertEqual(expected, basenames)

  def testMissingArchLibrary(self):
    libdir = os.path.join(self.toolchain, 'x86_64-nacl', 'lib32')
    lib_path = [libdir]
    nexes = ['libgcc_s.so.1']
    # CreateNmfUtils uses the 32-bit library path, but not the 64-bit one
    # so searching for a 32-bit library should succeed while searching for
    # a 64-bit one should fail.
    get_shared_deps.GleanFromObjdump(nexes, 'x86-32', self.objdump, lib_path)
    self.assertRaises(get_shared_deps.Error,
                      get_shared_deps.GleanFromObjdump,
                      nexes, 'x86-64', self.objdump, lib_path)

  def testCorrectArch(self):
    lib_path = [os.path.join(self.toolchain, 'x86_64-nacl', 'lib32'),
                os.path.join(self.toolchain, 'x86_64-nacl', 'lib')]

    needed = get_shared_deps.GetNeeded([self.dyn_nexe],
                                       lib_path=lib_path,
                                       objdump=self.objdump)
    for arch in needed.itervalues():
      self.assertEqual(arch, 'x86-32')


if __name__ == '__main__':
  unittest.main()

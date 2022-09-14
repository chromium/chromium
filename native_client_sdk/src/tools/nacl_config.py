#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A helper script to print paths of NaCl binaries, includes, libs, etc.

It is similar in behavior to pkg-config or sdl-config.
"""

import argparse
import os
import posixpath
import sys

import getos


if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)


VALID_ARCHES = ('arm', 'x86_32', 'x86_64', 'i686')
VALID_PNACL_ARCHES = (None, 'pnacl', 'le32')
ARCH_NAME = {
  'arm': 'arm',
  'x86_32': 'i686',
  'i686': 'i686',
  'x86_64': 'x86_64'
}

ARCH_ALT_NAME = {
  'arm': 'arm',
  'x86_32': 'x86_32',
  'i686': 'x86_32',
  'x86_64': 'x86_64'
}

ARCH_BASE_NAME = {
  'arm': 'arm',
  'x86_32': 'x86',
  'i686': 'x86',
  'x86_64': 'x86'
}

NACL_TOOLCHAINS = ('glibc', 'pnacl', 'bionic', 'clang-newlib')
HOST_TOOLCHAINS = ('linux', 'mac', 'win')
VALID_TOOLCHAINS = list(HOST_TOOLCHAINS) + list(NACL_TOOLCHAINS) + ['host']

# This is not an exhaustive list of tools, just the ones that need to be
# special-cased.

# e.g. For PNaCL cc => pnacl-clang
#      For NaCl  cc => pnacl-gcc
#
# Most tools will be passed through directly.
# e.g. For PNaCl foo => pnacl-foo
#      For NaCl  foo => x86_64-nacl-foo.
CLANG_TOOLS = {
  'cc': 'clang',
  'c++': 'clang++',
  'gcc': 'clang',
  'g++': 'clang++',
  'ld': 'clang++'
}

GCC_TOOLS = {
  'cc': 'gcc',
  'c++': 'g++',
  'gcc': 'gcc',
  'g++': 'g++',
  'ld': 'g++'
}


class Error(Exception):
  pass


def Expect(condition, message):
  if not condition:
    raise Error(message)


def ExpectToolchain(toolchain, expected_toolchains):
  Expect(toolchain in expected_toolchains,
         'Expected toolchain to be one of [%s], not %s.' % (
             ', '.join(expected_toolchains), toolchain))


def ExpectArch(arch, expected_arches):
  Expect(arch in expected_arches,
         'Expected arch to be one of [%s], not %s.' % (
             ', '.join(map(str, expected_arches)), arch))


def CheckValidToolchainArch(toolchain, arch, arch_required=False):
  if toolchain or arch or arch_required:
    ExpectToolchain(toolchain, VALID_TOOLCHAINS)

  if toolchain in HOST_TOOLCHAINS:
    Expect(arch is None,
           'Expected no arch for host toolchain %r. Got %r.' % (
               toolchain, arch))
  elif toolchain == 'pnacl':
    Expect(arch is None or arch in ['pnacl', 'le32'],
           'Expected no arch for toolchain %r. Got %r.' % (toolchain, arch))
  elif arch_required:
    Expect(arch is not None,
           'Expected arch to be one of [%s] for toolchain %r.\n'
           'Use the -a or --arch flags to specify one.\n' % (
               ', '.join(VALID_ARCHES), toolchain))

  if arch:
    if toolchain == 'pnacl':
      ExpectArch(arch, VALID_PNACL_ARCHES)
    else:
      ExpectArch(arch, VALID_ARCHES)


def GetArchName(arch):
  return ARCH_NAME.get(arch)


def GetArchAltName(arch):
  return ARCH_ALT_NAME.get(arch)


def GetArchBaseName(arch):
  return ARCH_BASE_NAME.get(arch)


def CanonicalizeToolchain(toolchain):
  if toolchain == 'host':
    return getos.GetPlatform()
  return toolchain


def GetPosixSDKPath():
  sdk_path = getos.GetSDKPath()
  if getos.GetPlatform() == 'win':
    return sdk_path.replace('\\', '/')
  else:
    return sdk_path


def GetToolchainDir(toolchain, arch=None):
  ExpectToolchain(toolchain, NACL_TOOLCHAINS)
  root = GetPosixSDKPath()
  platform = getos.GetPlatform()
  if toolchain in ('pnacl', 'clang-newlib'):
    subdir = '%s_pnacl' % platform
  else:
    assert arch is not None
    subdir = '%s_%s_%s' % (platform, GetArchBaseName(arch), toolchain)

  return posixpath.join(root, 'toolchain', subdir)


def GetToolchainArchDir(toolchain, arch):
  ExpectToolchain(toolchain, NACL_TOOLCHAINS)
  assert arch is not None
  toolchain_dir = GetToolchainDir(toolchain, arch)
  arch_dir = '%s-nacl' % GetArchName(arch)
  return posixpath.join(toolchain_dir, arch_dir)


def GetToolchainBinDir(toolchain, arch=None):
  ExpectToolchain(toolchain, NACL_TOOLCHAINS)
  return posixpath.join(GetToolchainDir(toolchain, arch), 'bin')


def GetSDKIncludeDirs(toolchain):
  root = GetPosixSDKPath()
  base_include = posixpath.join(root, 'include')
  if toolchain == 'clang-newlib':
    toolchain = 'newlib'
  return [base_include, posixpath.join(base_include, toolchain)]


def GetSDKLibDir():
  return posixpath.join(GetPosixSDKPath(), 'lib')


# Commands

def GetToolPath(toolchain, arch, tool):
  if tool == 'gdb':
    # Always use the same gdb; it supports multiple toolchains/architectures.
    # NOTE: this is always a i686 executable. i686-nacl-gdb is a symlink to
    # x86_64-nacl-gdb.
    return posixpath.join(GetToolchainBinDir('glibc', 'x86_64'),
                          'x86_64-nacl-gdb')

  if toolchain == 'pnacl':
    CheckValidToolchainArch(toolchain, arch)
    tool = CLANG_TOOLS.get(tool, tool)
    full_tool_name = 'pnacl-%s' % tool
  else:
    CheckValidToolchainArch(toolchain, arch, arch_required=True)
    ExpectArch(arch, VALID_ARCHES)
    if toolchain == 'clang-newlib':
      tool = CLANG_TOOLS.get(tool, tool)
    else:
      tool = GCC_TOOLS.get(tool, tool)
    full_tool_name = '%s-nacl-%s' % (GetArchName(arch), tool)
  return posixpath.join(GetToolchainBinDir(toolchain, arch), full_tool_name)


def GetCFlags(toolchain):
  ExpectToolchain(toolchain, VALID_TOOLCHAINS)
  return ' '.join('-I%s' % dirname for dirname in GetSDKIncludeDirs(toolchain))


def GetIncludeDirs(toolchain):
  ExpectToolchain(toolchain, VALID_TOOLCHAINS)
  return ' '.join(GetSDKIncludeDirs(toolchain))


def GetLDFlags():
  return '-L%s' % GetSDKLibDir()


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-t', '--toolchain', help='toolchain name. This can also '
                      'be specified with the NACL_TOOLCHAIN environment '
                      'variable.')
  parser.add_argument('-a', '--arch', help='architecture name. This can also '
                      'be specified with the NACL_ARCH environment variable.')

  group = parser.add_argument_group('Commands')
  group.add_argument('--tool', help='get tool path')
  group.add_argument('--cflags',
                     help='output all preprocessor and compiler flags',
                     action='store_true')
  group.add_argument('--libs', '--ldflags', help='output all linker flags',
                     action='store_true')
  group.add_argument('--include-dirs',
                     help='output include dirs, separated by spaces',
                     action='store_true')

  options = parser.parse_args(args)

  # Get toolchain/arch from environment, if not specified on commandline
  options.toolchain = options.toolchain or os.getenv('NACL_TOOLCHAIN')
  options.arch = options.arch or os.getenv('NACL_ARCH')

  options.toolchain = CanonicalizeToolchain(options.toolchain)
  CheckValidToolchainArch(options.toolchain, options.arch)

  if options.cflags:
    print GetCFlags(options.toolchain)
  elif options.include_dirs:
    print GetIncludeDirs(options.toolchain)
  elif options.libs:
    print GetLDFlags()
  elif options.tool:
    print GetToolPath(options.toolchain, options.arch, options.tool)
  else:
    parser.error('Expected a command. Run with --help for more information.')

  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as e:
    sys.stderr.write(str(e) + '\n')
    sys.exit(1)

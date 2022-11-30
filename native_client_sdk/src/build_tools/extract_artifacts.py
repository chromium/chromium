#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import glob
import os
import sys

if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)

import buildbot_common
import build_paths
from build_paths import NACL_DIR, SDK_SRC_DIR, EXTRACT_ARCHIVE_DIR

sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))

import getos
import oshelpers

# TODO(binji): The artifacts should be downloaded; until then, point at the
# directory where the artifacts are built.
DOWNLOAD_ARCHIVE_DIR = build_paths.BUILD_ARCHIVE_DIR

PLATFORM = getos.GetPlatform()
GLIBC_X86_TC_DIR = os.path.join('toolchain', '%s_x86_glibc' % PLATFORM)
PNACL_TC_DIR = os.path.join('toolchain', '%s_pnacl' % PLATFORM)
PNACL_TRANSLATOR_DIR = os.path.join(PNACL_TC_DIR, 'translator')

CYGTAR = os.path.join(NACL_DIR, 'build', 'cygtar.py')
TAR = oshelpers.FindExeInPath('tar')

options = None


PPAPI_ARCHIVE = os.path.join(DOWNLOAD_ARCHIVE_DIR,
                             '%s_ppapi.tar.bz2' % PLATFORM)

GLIBC_ARCHIVE_MAP = [
  ('glibc', GLIBC_X86_TC_DIR),
  ('glibc_headers', os.path.join(GLIBC_X86_TC_DIR, 'x86_64-nacl', 'include')),
  ('glibc_x86_32_libs', os.path.join(GLIBC_X86_TC_DIR, 'x86_64-nacl', 'lib32')),
  ('glibc_x86_64_libs', os.path.join(GLIBC_X86_TC_DIR, 'x86_64-nacl', 'lib'))]

PNACL_ARCHIVE_MAP = [
  ('pnacl', PNACL_TC_DIR),
  ('newlib_headers', os.path.join(PNACL_TC_DIR, 'le32-nacl', 'include')),
  ('pnacl_libs', os.path.join(PNACL_TC_DIR, 'le32-nacl', 'lib')),
  ('pnacl_translator_arm_libs',
      os.path.join(PNACL_TRANSLATOR_DIR, 'arm', 'lib')),
  ('pnacl_translator_x86_32_libs',
      os.path.join(PNACL_TRANSLATOR_DIR, 'x86-32', 'lib')),
  ('pnacl_translator_x86_64_libs',
      os.path.join(PNACL_TRANSLATOR_DIR, 'x86-64', 'lib'))]

TOOLCHAIN_ARCHIVE_MAPS = {
  'glibc': GLIBC_ARCHIVE_MAP,
  'pnacl': PNACL_ARCHIVE_MAP,
}

TOOLS_ARCHIVE_MAP = [('tools', 'tools')]


def Untar(archive, destdir):
  if os.path.exists(TAR):
    cmd = [TAR]
  else:
    cmd = [sys.executable, CYGTAR]

  if options.verbose:
    cmd.extend(['-xvf', archive])
  else:
    cmd.extend(['-xf', archive])

  if not os.path.exists(destdir):
    buildbot_common.MakeDir(destdir)
  buildbot_common.Run(cmd, cwd=destdir)


def RemoveExt(path):
  while True:
    path, ext = os.path.splitext(path)
    if ext == '':
      return path


def ExtractArchive(archive_path, destdirs):
  Untar(archive_path, EXTRACT_ARCHIVE_DIR)
  basename = RemoveExt(os.path.basename(archive_path))
  srcdir = os.path.join(EXTRACT_ARCHIVE_DIR, basename)
  if not isinstance(destdirs, list):
    destdirs = [destdirs]

  for destdir in destdirs:
    if not os.path.exists(destdir):
      buildbot_common.MakeDir(destdir)
    src_files = glob.glob(os.path.join(srcdir, '*'))
    for src_file in src_files:
      buildbot_common.CopyDir(src_file, destdir)


def ExtractAll(archive_dict, archive_dir, destroot):
  for archive_part, rel_destdirs in archive_dict:
    archive_name = '%s_%s.tar.bz2' % (PLATFORM, archive_part)
    archive_path = os.path.join(archive_dir, archive_name)
    if not isinstance(rel_destdirs, list):
      rel_destdirs = [rel_destdirs]
    destdirs = [os.path.join(destroot, d) for d in rel_destdirs]
    ExtractArchive(archive_path, destdirs)


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-v', '--verbose')
  parser.add_argument('-o', '--outdir')
  parser.add_argument('-t', '--toolchain', action='append', dest='toolchains',
                      default=[])
  parser.add_argument('--clean', action='store_true')
  global options
  options = parser.parse_args(args)

  if options.clean:
    buildbot_common.RemoveDir(options.outdir)
  for toolchain in options.toolchains:
    ExtractAll(TOOLCHAIN_ARCHIVE_MAPS[toolchain], DOWNLOAD_ARCHIVE_DIR,
               options.outdir)
  ExtractAll(TOOLS_ARCHIVE_MAP, DOWNLOAD_ARCHIVE_DIR, options.outdir)
  Untar(PPAPI_ARCHIVE, EXTRACT_ARCHIVE_DIR)
  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except KeyboardInterrupt:
    buildbot_common.ErrorExit('extract_artifacts: interrupted')

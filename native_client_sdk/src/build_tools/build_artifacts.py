#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to build binary components of the SDK.

This script builds binary components of the Native Client SDK, create tarballs
for them, and uploads them to Google Cloud Storage.

This prevents a source dependency on the Chromium/NaCl tree in the Native
Client SDK repo.
"""

import argparse
import datetime
import glob
import hashlib
import json
import os
import sys
import tempfile

if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)

import buildbot_common
import build_version
from build_paths import NACL_DIR, OUT_DIR, SRC_DIR, SDK_SRC_DIR
from build_paths import BUILD_ARCHIVE_DIR

sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))

import getos
import oshelpers

BUILD_DIR = os.path.join(NACL_DIR, 'build')
NACL_TOOLCHAIN_DIR = os.path.join(NACL_DIR, 'toolchain')
NACL_TOOLCHAINTARS_DIR = os.path.join(NACL_TOOLCHAIN_DIR, '.tars')

CYGTAR = os.path.join(BUILD_DIR, 'cygtar.py')
PKGVER = os.path.join(BUILD_DIR, 'package_version', 'package_version.py')
VERSION_JSON = os.path.join(BUILD_ARCHIVE_DIR, 'version.json')


PLATFORM = getos.GetPlatform()
TAR = oshelpers.FindExeInPath('tar')
options = None
all_archives = []


# Mapping from toolchain name to the equivalent package_version.py directory
# name.
TOOLCHAIN_PACKAGE_MAP = {
    'glibc_x86': 'nacl_x86_glibc',
    'glibc_arm': 'nacl_arm_glibc',
    'pnacl': 'pnacl_newlib'}


def Tar(archive_path, root, files):
  if os.path.exists(TAR):
    cmd = [TAR]
  else:
    cmd = [sys.executable, CYGTAR]
  cmd.extend(['-cjf', archive_path])
  cmd.extend(files)
  buildbot_common.Run(cmd, cwd=root)


def ComputeSha(filename):
  with open(filename) as f:
    return hashlib.sha1(f.read()).hexdigest()


class TempDir(object):
  def __init__(self, prefix=None, dont_remove=False):
    self.prefix = prefix
    self.name = None
    self.dont_remove = dont_remove
    self.created = False
    self.destroyed = False

  def Create(self):
    assert not self.created
    self.name = tempfile.mkdtemp(prefix=self.prefix)
    return self

  def Destroy(self):
    assert not self.destroyed
    if not self.dont_remove:
      buildbot_common.RemoveDir(self.name)

  def __enter__(self):
    self.Create()
    return self.name

  def __exit__(self, exc, value, tb):
    return self.Destroy()


class Archive(object):
  def __init__(self, name):
    self.name = '%s_%s' % (PLATFORM, name)
    self.archive_name = self.name + '.tar.bz2'
    self.archive_path = os.path.join(BUILD_ARCHIVE_DIR, self.archive_name)
    self.dirname = os.path.join(BUILD_ARCHIVE_DIR, self.name)
    self._MakeDirname()

  def _MakeDirname(self):
    if os.path.exists(self.dirname):
      buildbot_common.RemoveDir(self.dirname)
    buildbot_common.MakeDir(self.dirname)

  def Copy(self, src_root, file_list):
    if not isinstance(file_list, list):
      file_list = [file_list]

    for file_spec in file_list:
      # The list of files to install can be a simple list of
      # strings or a list of pairs, where each pair corresponds
      # to a mapping from source to destination names.
      if isinstance(file_spec, str):
        src_file = dest_file = file_spec
      else:
        src_file, dest_file = file_spec

      src_file = os.path.join(src_root, src_file)

      # Expand sources files using glob.
      sources = glob.glob(src_file)
      if not sources:
        sources = [src_file]

      if len(sources) > 1:
        if not (dest_file.endswith('/') or dest_file == ''):
          buildbot_common.ErrorExit(
              "Target file %r must end in '/' or be empty when "
              "using globbing to install files %r" % (dest_file, sources))

      for source in sources:
        if dest_file.endswith('/'):
          dest = os.path.join(dest_file, os.path.basename(source))
        else:
          dest = dest_file
        dest = os.path.join(self.dirname, dest)
        if not os.path.isdir(os.path.dirname(dest)):
          buildbot_common.MakeDir(os.path.dirname(dest))
        if os.path.isdir(source):
          buildbot_common.CopyDir(source, dest)
        else:
          buildbot_common.CopyFile(source, dest)

  def CreateArchiveShaFile(self):
    sha1 = ComputeSha(self.archive_path)
    sha1_filename = self.archive_path + '.sha1'
    with open(sha1_filename, 'w') as f:
      f.write(sha1)

  def Tar(self):
    Tar(self.archive_path, BUILD_ARCHIVE_DIR, [
        self.name,
        os.path.basename(VERSION_JSON)])
    self.CreateArchiveShaFile()
    all_archives.append(self.archive_name)


def MakeToolchainArchive(toolchain):
  archive = Archive(toolchain)

  build_platform = '%s_x86' % PLATFORM

  with TempDir('tc_%s_' % toolchain) as tmpdir:
    package_name = os.path.join(build_platform,
                                TOOLCHAIN_PACKAGE_MAP.get(toolchain))

    # Extract all of the packages into the temp directory.
    buildbot_common.Run([sys.executable, PKGVER,
                           '--packages', package_name,
                           '--tar-dir', NACL_TOOLCHAINTARS_DIR,
                           '--dest-dir', tmpdir,
                           'extract'])

    # Copy all the files we extracted to the correct destination.
    archive.Copy(os.path.join(tmpdir, package_name), ('*', ''))

  archive.Tar()


def MakeNinjaRelPath(path):
  return os.path.join(os.path.relpath(OUT_DIR, SRC_DIR), path)


def NinjaBuild(targets, out_dir):
  if not isinstance(targets, list):
    targets = [targets]
  out_config_dir = os.path.join(out_dir, 'Release')
  buildbot_common.Run(['ninja', '-C', out_config_dir] + targets, cwd=SRC_DIR)


def GypNinjaBuild(arch, gyp_py_script, gyp_file, targets, out_dir):
  gyp_env = dict(os.environ)
  gyp_defines = []
  if options.mac_sdk:
    gyp_defines.append('mac_sdk=%s' % options.mac_sdk)
  if arch:
    gyp_defines.append('target_arch=%s' % arch)
    if arch == 'arm':
      gyp_env['GYP_CROSSCOMPILE'] = '1'
      if options.no_arm_trusted:
        gyp_defines.append('disable_cross_trusted=1')

  gyp_env['GYP_DEFINES'] = ' '.join(gyp_defines)
  generator_flags = ['-G', 'output_dir=%s' % out_dir]
  depth = '--depth=.'
  cmd = [sys.executable, gyp_py_script, gyp_file, depth] + generator_flags
  buildbot_common.Run(cmd, cwd=SRC_DIR, env=gyp_env)
  NinjaBuild(targets, out_dir)


def GetToolsFiles():
  files = [
    ('sel_ldr', 'sel_ldr_x86_32'),
    ('ncval_new', 'ncval'),
    ('irt_core_newlib_x32.nexe', 'irt_core_x86_32.nexe'),
    ('irt_core_newlib_x64.nexe', 'irt_core_x86_64.nexe'),
  ]

  if PLATFORM == 'linux':
    files.append(['nacl_helper_bootstrap', 'nacl_helper_bootstrap_x86_32'])

  # Add .exe extensions to all windows tools
  for pair in files:
    if PLATFORM == 'win' and not pair[0].endswith('.nexe'):
      pair[0] += '.exe'
      pair[1] += '.exe'

  return files


def GetTools64Files():
  files = []
  if PLATFORM == 'win':
    files.append('sel_ldr64')
  else:
    files.append(('sel_ldr', 'sel_ldr_x86_64'))

  if PLATFORM == 'linux':
    files.append(('nacl_helper_bootstrap', 'nacl_helper_bootstrap_x86_64'))

  return files


def GetToolsArmFiles():
  assert PLATFORM == 'linux'
  return [('irt_core_newlib_arm.nexe', 'irt_core_arm.nexe'),
          ('sel_ldr', 'sel_ldr_arm'),
          ('nacl_helper_bootstrap', 'nacl_helper_bootstrap_arm')]


def GetNewlibToolchainLibs():
  return ['crti.o',
          'crtn.o',
          'libminidump_generator.a',
          'libnacl.a',
          'libnacl_dyncode.a',
          'libnacl_exception.a',
          'libnacl_list_mappings.a',
          'libnosys.a',
          'libppapi.a',
          'libppapi_stub.a',
          'libpthread.a']


def GetGlibcToolchainLibs():
  return ['libminidump_generator.a',
          'libminidump_generator.so',
          'libnacl.a',
          'libnacl_dyncode.a',
          'libnacl_dyncode.so',
          'libnacl_exception.a',
          'libnacl_exception.so',
          'libnacl_list_mappings.a',
          'libnacl_list_mappings.so',
          'libppapi.a',
          'libppapi.so',
          'libppapi_stub.a']


def GetPNaClToolchainLibs():
  return ['libminidump_generator.a',
          'libnacl.a',
          'libnacl_dyncode.a',
          'libnacl_exception.a',
          'libnacl_list_mappings.a',
          'libnosys.a',
          'libppapi.a',
          'libppapi_stub.a',
          'libpthread.a']


def GetBionicToolchainLibs():
  return ['libminidump_generator.a',
          'libnacl_dyncode.a',
          'libnacl_exception.a',
          'libnacl_list_mappings.a',
          'libppapi.a']


def GetToolchainNaClLib(tcname, tcpath, xarch):
  if tcname == 'pnacl':
    return os.path.join(tcpath, 'le32-nacl', 'lib')
  elif xarch == 'x86_32':
    return os.path.join(tcpath, 'x86_64-nacl', 'lib32')
  elif xarch == 'x86_64':
    return os.path.join(tcpath, 'x86_64-nacl', 'lib')
  elif xarch == 'arm':
    return os.path.join(tcpath, 'arm-nacl', 'lib')


def GetGypBuiltLib(root, tcname, xarch=None):
  if tcname == 'pnacl':
    tcname = 'pnacl_newlib'
  if tcname in ('glibc_arm', 'glibc_x86'):
    tcname = 'glibc'
  if xarch == 'x86_32':
    xarch = '32'
  elif xarch == 'x86_64':
    xarch = '64'
  elif not xarch:
    xarch = ''
  return os.path.join(root, 'Release', 'gen', 'tc_' + tcname, 'lib' + xarch)


def GetGypToolchainLib(root, tcname, xarch):
  tcpath = os.path.join(root, 'Release', 'gen', 'sdk', '%s_x86' % PLATFORM,
                        TOOLCHAIN_PACKAGE_MAP[tcname])
  return GetToolchainNaClLib(tcname, tcpath, xarch)


def MakeGypArchives():
  join = os.path.join
  gyp_chromium = join(SRC_DIR, 'build', 'gyp_chromium')
  # TODO(binji): gyp_nacl doesn't build properly on Windows anymore; it only
  # can use VS2010, not VS2013 which is now required by the Chromium repo. NaCl
  # needs to be updated to perform the same logic as Chromium in detecting VS,
  # which can now exist in the depot_tools directory.
  # See https://code.google.com/p/nativeclient/issues/detail?id=4022
  #
  # For now, let's use gyp_chromium to build these components.
  # gyp_nacl = join(NACL_DIR, 'build', 'gyp_nacl')
  gyp_nacl = gyp_chromium

  nacl_core_sdk_gyp = join(NACL_DIR, 'build', 'nacl_core_sdk.gyp')
  all_gyp = join(NACL_DIR, 'build', 'all.gyp')
  breakpad_gyp = join(SRC_DIR, 'breakpad', 'breakpad.gyp')
  ppapi_gyp = join(SRC_DIR, 'ppapi', 'native_client', 'native_client.gyp')
  breakpad_targets = ['dump_syms', 'minidump_dump', 'minidump_stackwalk']

  # Build
  tmpdir_obj = TempDir('nacl_core_sdk_', dont_remove=True).Create()
  tmpdir = tmpdir_obj.name
  GypNinjaBuild('ia32', gyp_nacl, nacl_core_sdk_gyp, 'nacl_core_sdk', tmpdir)
  GypNinjaBuild('ia32', gyp_nacl, all_gyp, 'ncval_new', tmpdir)
  GypNinjaBuild('ia32', gyp_chromium, breakpad_gyp, breakpad_targets, tmpdir)
  GypNinjaBuild('ia32', gyp_chromium, ppapi_gyp, 'ppapi_lib', tmpdir)
  GypNinjaBuild('x64', gyp_chromium, ppapi_gyp, 'ppapi_lib', tmpdir)

  tmpdir64_obj = TempDir('nacl_core_sdk_64_', dont_remove=True).Create()
  tmpdir_64 = tmpdir64_obj.name
  if PLATFORM == 'win':
    GypNinjaBuild('ia32', gyp_nacl, nacl_core_sdk_gyp, 'sel_ldr64', tmpdir_64)
  else:
    GypNinjaBuild('x64', gyp_nacl, nacl_core_sdk_gyp, 'sel_ldr', tmpdir_64)

  tmpdirarm_obj = TempDir('nacl_core_sdk_arm_', dont_remove=True).Create()
  tmpdir_arm = tmpdirarm_obj.name
  GypNinjaBuild('arm', gyp_nacl, nacl_core_sdk_gyp, 'nacl_core_sdk', tmpdir_arm)
  GypNinjaBuild('arm', gyp_chromium, ppapi_gyp, 'ppapi_lib', tmpdir_arm)

  # Tools archive
  archive = Archive('tools')
  archive.Copy(join(tmpdir, 'Release'), GetToolsFiles())
  archive.Copy(join(tmpdir_64, 'Release'), GetTools64Files())
  if PLATFORM == 'linux':
    archive.Copy(join(tmpdir_arm, 'Release'), GetToolsArmFiles())
  # TODO(binji): dump_syms doesn't currently build on Windows. See
  # http://crbug.com/245456
  if PLATFORM != 'win':
    archive.Copy(join(tmpdir, 'Release'), breakpad_targets)
  archive.Tar()

  # glibc x86 libs archives
  for arch in ('x86_32', 'x86_64'):
    archive = Archive('glibc_%s_libs' % arch)
    archive.Copy(GetGypBuiltLib(tmpdir, 'glibc', arch), GetGlibcToolchainLibs())
    archive.Copy(GetGypToolchainLib(tmpdir, 'glibc', arch), 'crt1.o')
    archive.Tar()

  # pnacl libs archive
  archive = Archive('pnacl_libs')
  archive.Copy(GetGypBuiltLib(tmpdir, 'pnacl'), GetPNaClToolchainLibs())
  archive.Tar()

  # Destroy the temporary directories
  tmpdir_obj.Destroy()
  tmpdirarm_obj.Destroy()
  tmpdir64_obj.Destroy()


def MakePNaClArchives():
  join = os.path.join
  gyp_chromium = join(SRC_DIR, 'build', 'gyp_chromium')
  pnacl_irt_shim_gyp = join(SRC_DIR, 'ppapi', 'native_client', 'src',
                            'untrusted', 'pnacl_irt_shim', 'pnacl_irt_shim.gyp')

  with TempDir('pnacl_irt_shim_ia32_') as tmpdir:
    GypNinjaBuild('ia32', gyp_chromium, pnacl_irt_shim_gyp, 'aot', tmpdir)

    archive = Archive('pnacl_translator_x86_32_libs')
    libdir = join(tmpdir, 'Release', 'gen', 'tc_pnacl_translate', 'lib-x86-32')
    archive.Copy(libdir, 'libpnacl_irt_shim.a')
    archive.Tar()

    archive = Archive('pnacl_translator_x86_64_libs')
    libdir = join(tmpdir, 'Release', 'gen', 'tc_pnacl_translate', 'lib-x86-32')
    archive.Copy(libdir, 'libpnacl_irt_shim.a')
    archive.Tar()

  with TempDir('pnacl_irt_shim_arm_') as tmpdir:
    GypNinjaBuild('arm', gyp_chromium, pnacl_irt_shim_gyp, 'aot', tmpdir)

    archive = Archive('pnacl_translator_arm_libs')
    libdir = join(tmpdir, 'Release', 'gen', 'tc_pnacl_translate', 'lib-arm')
    archive.Copy(libdir, 'libpnacl_irt_shim.a')
    archive.Tar()


def GetNewlibHeaders():
  return [
      ('native_client/src/include/nacl/nacl_exception.h', 'nacl/'),
      ('native_client/src/include/nacl/nacl_minidump.h', 'nacl/'),
      ('native_client/src/untrusted/irt/irt.h', ''),
      ('native_client/src/untrusted/irt/irt_dev.h', ''),
      ('native_client/src/untrusted/irt/irt_extension.h', ''),
      ('native_client/src/untrusted/nacl/nacl_dyncode.h', 'nacl/'),
      ('native_client/src/untrusted/nacl/nacl_startup.h', 'nacl/'),
      ('native_client/src/untrusted/pthread/pthread.h', ''),
      ('native_client/src/untrusted/pthread/semaphore.h', ''),
      ('native_client/src/untrusted/valgrind/dynamic_annotations.h', 'nacl/'),
      ('ppapi/nacl_irt/public/irt_ppapi.h', '')]


def GetGlibcHeaders():
  return [
      ('native_client/src/include/nacl/nacl_exception.h', 'nacl/'),
      ('native_client/src/include/nacl/nacl_minidump.h', 'nacl/'),
      ('native_client/src/untrusted/irt/irt.h', ''),
      ('native_client/src/untrusted/irt/irt_dev.h', ''),
      ('native_client/src/untrusted/nacl/nacl_dyncode.h', 'nacl/'),
      ('native_client/src/untrusted/nacl/nacl_startup.h', 'nacl/'),
      ('native_client/src/untrusted/valgrind/dynamic_annotations.h', 'nacl/'),
      ('ppapi/nacl_irt/public/irt_ppapi.h', '')]


def GetBionicHeaders():
  return [('ppapi/nacl_irt/public/irt_ppapi.h', '')]


def MakeToolchainHeaderArchives():
  archive = Archive('newlib_headers')
  archive.Copy(SRC_DIR, GetNewlibHeaders())
  archive.Tar()

  archive = Archive('glibc_headers')
  archive.Copy(SRC_DIR, GetGlibcHeaders())
  archive.Tar()


def MakePepperArchive():
  archive = Archive('ppapi')
  archive.Copy(os.path.join(SRC_DIR, 'ppapi'), ['c', 'cpp', 'lib', 'utility'])
  archive.Tar()


def UploadArchives():
  major_version = build_version.ChromeMajorVersion()
  chrome_revision = build_version.ChromeRevision()
  commit_position = build_version.ChromeCommitPosition()
  git_sha = build_version.ParseCommitPosition(commit_position)[0]
  short_sha = git_sha[:9]
  archive_version = '%s-%s-%s' % (major_version, chrome_revision, short_sha)
  bucket_path = 'native-client-sdk/archives/%s' % archive_version
  for archive_name in all_archives:
    buildbot_common.Archive(archive_name, bucket_path,
                            cwd=BUILD_ARCHIVE_DIR, step_link=False)
    sha1_filename = archive_name + '.sha1'
    buildbot_common.Archive(sha1_filename, bucket_path,
                            cwd=BUILD_ARCHIVE_DIR, step_link=False)


def MakeVersionJson():
  time_format = '%Y/%m/%d %H:%M:%S'
  data = {
      'chrome_version': build_version.ChromeVersionNoTrunk(),
      'chrome_revision': build_version.ChromeRevision(),
      'chrome_commit_position': build_version.ChromeCommitPosition(),
      'nacl_revision': build_version.NaClRevision(),
      'build_date': datetime.datetime.now().strftime(time_format)}

  dirname = os.path.dirname(VERSION_JSON)
  if not os.path.exists(dirname):
    buildbot_common.MakeDir(dirname)
  with open(VERSION_JSON, 'w') as outf:
    json.dump(data, outf, indent=2, separators=(',', ': '))


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--mac-sdk',
      help='Set the mac-sdk (e.g. 10.6) to use when building with ninja.')
  parser.add_argument('--no-arm-trusted', action='store_true',
      help='Disable building of ARM trusted components (sel_ldr, etc).')
  parser.add_argument('--upload', action='store_true',
                      help='Upload tarballs to GCS.')

  global options
  options = parser.parse_args(args)

  toolchains = ['pnacl', 'glibc_x86', 'glibc_arm']

  MakeVersionJson()
  for tc in toolchains:
    MakeToolchainArchive(tc)
  MakeGypArchives()
  MakePNaClArchives()
  MakeToolchainHeaderArchives()
  MakePepperArchive()
  if options.upload:
    UploadArchives()

  return 0

if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except KeyboardInterrupt:
    buildbot_common.ErrorExit('build_artifacts: interrupted')

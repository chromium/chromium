#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script lays out the PNaCl translator files for a
   normal Chrome installer, for one platform.  Once run num-of-arches times,
   the result can then be packed into a multi-CRX zip file.

   This script depends on and pulls in the translator nexes and libraries
   from the PNaCl translator. It also depends on the pnacl_irt_shim.
"""

import json
import logging
import optparse
import os
import platform
import re
import shutil
import subprocess
import sys

J = os.path.join

######################################################################
# Target arch and build arch junk to convert between all the
# silly conventions between SCons, Chrome and PNaCl.

# The version of the arch used by NaCl manifest files.
# This is based on the machine "building" this extension.
# We also used this to identify the arch-specific different versions of
# this extension.

def CanonicalArch(arch):
  if arch in ('x86_64', 'x86-64', 'x64', 'amd64'):
    return 'x86-64'
  # TODO(jvoung): be more specific about the arm architecture version?
  if arch in ('arm', 'armv7'):
    return 'arm'
  if arch in ('mipsel'):
    return 'mips32'
  if re.match('^i.86$', arch) or arch in ('x86_32', 'x86-32', 'ia32', 'x86'):
    return 'x86-32'
  return None

def GetBuildArch():
  arch = platform.machine()
  return CanonicalArch(arch)

BUILD_ARCH = GetBuildArch()
ARCHES = ['x86-32', 'x86-64', 'arm', 'mips32']

def IsValidArch(arch):
  return arch in ARCHES


######################################################################

# Normalize the platform name to be the way SCons finds chrome binaries.
# This is based on the platform "building" the extension.

def GetBuildPlatform():
  if sys.platform == 'darwin':
    platform = 'mac'
  elif sys.platform.startswith('linux'):
    platform = 'linux'
  elif sys.platform in ('cygwin', 'win32'):
    platform = 'windows'
  else:
    raise Exception('Unknown platform: %s' % sys.platform)
  return platform
BUILD_PLATFORM = GetBuildPlatform()


def DetermineInstallerArches(target_arch):
  arch = CanonicalArch(target_arch)
  if not IsValidArch(arch):
    raise Exception('Unknown target_arch %s' % target_arch)
  # On windows, we need x86-32 and x86-64 (assuming non-windows RT).
  if BUILD_PLATFORM == 'windows':
    if arch.startswith('x86'):
      return ['x86-32', 'x86-64']
    else:
      raise Exception('Unknown target_arch on windows w/ target_arch == %s' %
                      target_arch)
  else:
    return [arch]


######################################################################

class PnaclPackaging(object):

  package_base = os.path.dirname(__file__)

  # File paths that are set from the command line.
  pnacl_template = None
  package_version_path = None
  pnacl_package = 'pnacl_newlib'

  # Agreed-upon name for pnacl-specific info.
  pnacl_json = 'pnacl.json'

  @staticmethod
  def SetPnaclInfoTemplatePath(path):
    PnaclPackaging.pnacl_template = path

  @staticmethod
  def SetPackageVersionPath(path):
    PnaclPackaging.package_version_path = path

  @staticmethod
  def SetPnaclPackageName(name):
    PnaclPackaging.pnacl_package = name

  @staticmethod
  def PnaclToolsRevision():
    pkg_ver_cmd = [sys.executable, PnaclPackaging.package_version_path,
                   'getrevision',
                   '--revision-package', PnaclPackaging.pnacl_package]

    return subprocess.check_output(pkg_ver_cmd).strip()

  @staticmethod
  def GeneratePnaclInfo(target_dir, abi_version, arch):
    # A note on versions: pnacl_version is the version of translator built
    # by the NaCl repo, while abi_version is bumped when the NaCl sandbox
    # actually changes.
    pnacl_version = PnaclPackaging.PnaclToolsRevision()
    with open(PnaclPackaging.pnacl_template, 'rb') as pnacl_template_fd:
      pnacl_template = json.load(pnacl_template_fd)
    out_name = J(target_dir, UseWhitelistedChars(PnaclPackaging.pnacl_json,
                                                 None))
    with open(out_name, 'w') as output_fd:
      pnacl_template['pnacl-arch'] = arch
      pnacl_template['pnacl-version'] = pnacl_version.decode()
      json.dump(pnacl_template, output_fd, sort_keys=True, indent=4)


######################################################################

class PnaclDirs(object):
  translator_dir = None
  output_dir = None

  @staticmethod
  def SetTranslatorRoot(d):
    PnaclDirs.translator_dir = d

  @staticmethod
  def TranslatorRoot():
    return PnaclDirs.translator_dir

  @staticmethod
  def LibDir(target_arch):
    return J(PnaclDirs.TranslatorRoot(), 'translator', '%s' % target_arch)

  @staticmethod
  def SandboxedCompilerDir(target_arch):
    return J(PnaclDirs.TranslatorRoot(), 'translator', target_arch, 'bin')

  @staticmethod
  def SetOutputDir(d):
    PnaclDirs.output_dir = d

  @staticmethod
  def OutputDir():
    return PnaclDirs.output_dir

  @staticmethod
  def OutputAllDir(version_quad):
    return J(PnaclDirs.OutputDir(), version_quad)

  @staticmethod
  def OutputArchBase(arch):
    return '%s' % arch

  @staticmethod
  def OutputArchDir(arch):
    # Nest this in another directory so that the layout will be the same
    # as the "all"/universal version.
    parent_dir = J(PnaclDirs.OutputDir(), PnaclDirs.OutputArchBase(arch))
    return (parent_dir, J(parent_dir, PnaclDirs.OutputArchBase(arch)))


######################################################################

def StepBanner(short_desc, long_desc):
  logging.info("**** %s\t%s", short_desc, long_desc)


def Clean():
  out_dir = PnaclDirs.OutputDir()
  StepBanner('CLEAN', 'Cleaning out old packaging: %s' % out_dir)
  if os.path.isdir(out_dir):
    shutil.rmtree(out_dir)
  else:
    logging.info('Clean skipped -- no previous output directory!')

######################################################################

def UseWhitelistedChars(orig_basename, arch):
  """ Make the filename match the pattern expected by nacl_file_host.

  Currently, this assumes there is prefix "pnacl_public_" and
  that the allowed chars are in the set [a-zA-Z0-9_].
  """
  if arch:
    target_basename = 'pnacl_public_%s_%s' % (arch, orig_basename)
  else:
    target_basename = 'pnacl_public_%s' % orig_basename
  result = re.sub(r'[^a-zA-Z0-9_]', '_', target_basename)
  logging.info('UseWhitelistedChars using: %s' % result)
  return result

def CopyFlattenDirsAndPrefix(src_dir, arch, dest_dir):
  """ Copy files from src_dir to dest_dir.

  When copying, also rename the files such that they match the white-listing
  pattern in chrome/browser/nacl_host/nacl_file_host.cc.
  """
  if not os.path.isdir(src_dir):
    raise Exception('Copy dir failed, directory does not exist: %s' % src_dir)

  for (root, dirs, files) in os.walk(src_dir, followlinks=True):
    for f in files:
      # Assume a flat directory.
      assert (f == os.path.basename(f))
      full_name = J(root, f)
      target_name = UseWhitelistedChars(f, arch)
      shutil.copy(full_name, J(dest_dir, target_name))


def BuildArchForInstaller(version_quad, arch, lib_overrides):
  """ Build an architecture specific version for the chrome installer.
  """
  target_dir = PnaclDirs.OutputDir()

  StepBanner('BUILD INSTALLER',
             'Packaging for arch %s in %s' % (arch, target_dir))

  # Copy llc.nexe and ld.nexe, but with some renaming and directory flattening.
  CopyFlattenDirsAndPrefix(PnaclDirs.SandboxedCompilerDir(arch),
                           arch,
                           target_dir)

  # Copy native libraries, also with renaming and directory flattening.
  CopyFlattenDirsAndPrefix(PnaclDirs.LibDir(arch), arch, target_dir)

  # Also copy files from the list of overrides.
  # This needs the arch tagged onto the name too, like the other files.
  if arch in lib_overrides:
    for (override_lib, desired_name) in lib_overrides[arch]:
      target_name = UseWhitelistedChars(desired_name, arch)
      shutil.copy(override_lib, J(target_dir, target_name))


def BuildInstallerStyle(version_quad, lib_overrides, arches):
  """ Package the pnacl component for use within the chrome installer
  infrastructure.  These files need to be named in a special way
  so that white-listing of files is easy.
  """
  StepBanner("BUILD_ALL", "Packaging installer for version: %s" % version_quad)
  for arch in arches:
    BuildArchForInstaller(version_quad, arch, lib_overrides)
  # Generate pnacl info manifest.
  # Hack around the fact that there may be more than one arch, on Windows.
  if len(arches) == 1:
    arches = arches[0]
  PnaclPackaging.GeneratePnaclInfo(PnaclDirs.OutputDir(), version_quad, arches)


######################################################################


def Main():
  usage = 'usage: %prog [options] version_arg'
  parser = optparse.OptionParser(usage)
  # We may want to accept a target directory to dump it in the usual
  # output directory (e.g., scons-out).
  parser.add_option('-c', '--clean', dest='clean',
                    action='store_true', default=False,
                    help='Clean out destination directory first.')
  parser.add_option('-d', '--dest', dest='dest',
                    help='The destination root for laying out the extension')
  parser.add_option('-L', '--lib_override',
                    dest='lib_overrides', action='append', default=[],
                    help='Specify path to a fresher native library ' +
                    'that overrides the tarball library with ' +
                    '(arch,libfile,librenamed) tuple.')
  parser.add_option('-t', '--target_arch',
                    dest='target_arch', default=None,
                    help='Only generate the chrome installer version for arch')
  parser.add_option('--info_template_path',
                    dest='info_template_path', default=None,
                    help='Path of the info template file')
  parser.add_option('--package_version_path', dest='package_version_path',
                    default=None, help='Path to package_version.py script.')
  parser.add_option('--pnacl_package_name', dest='pnacl_package_name',
                    default=None, help='Name of PNaCl package.')
  parser.add_option('--pnacl_translator_path', dest='pnacl_translator_path',
                    default=None, help='Location of PNaCl translator.')
  parser.add_option('-v', '--verbose', dest='verbose', default=False,
                    action='store_true',
                    help='Print verbose debug messages.')

  (options, args) = parser.parse_args()
  if options.verbose:
    logging.getLogger().setLevel(logging.DEBUG)
  else:
    logging.getLogger().setLevel(logging.ERROR)
  logging.info('pnacl_component_crx_gen w/ options %s and args %s\n'
               % (options, args))

  # Set destination directory before doing any cleaning, etc.
  if options.dest is None:
    raise Exception('Destination path must be set.')
  PnaclDirs.SetOutputDir(options.dest)

  if options.clean:
    Clean()

  if options.pnacl_translator_path is None:
    raise Exception('PNaCl translator path must be set.')
  PnaclDirs.SetTranslatorRoot(options.pnacl_translator_path)

  if options.info_template_path:
    PnaclPackaging.SetPnaclInfoTemplatePath(options.info_template_path)

  if options.package_version_path:
    PnaclPackaging.SetPackageVersionPath(options.package_version_path)
  else:
    raise Exception('Package verison script must be specified.')

  if options.pnacl_package_name:
    PnaclPackaging.SetPnaclPackageName(options.pnacl_package_name)

  lib_overrides = {}
  for o in options.lib_overrides:
    arch, override_lib, desired_name = o.split(',')
    arch = CanonicalArch(arch)
    if not IsValidArch(arch):
      raise Exception('Unknown arch for -L: %s (from %s)' % (arch, o))
    if not os.path.isfile(override_lib):
      raise Exception('Override native lib not a file for -L: %s (from %s)' %
                      (override_lib, o))
    override_list = lib_overrides.get(arch, [])
    override_list.append((override_lib, desired_name))
    lib_overrides[arch] = override_list

  if len(args) != 1:
    parser.print_help()
    parser.error('Incorrect number of arguments')

  abi_version = int(args[0])

  arches = DetermineInstallerArches(options.target_arch)
  BuildInstallerStyle(abi_version, lib_overrides, arches)
  return 0


if __name__ == '__main__':
  sys.exit(Main())

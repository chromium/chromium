#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper script for launching application within the sel_ldr.
"""

import argparse
import os
import subprocess
import sys

import create_nmf
import getos

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_SDK_ROOT = os.path.dirname(SCRIPT_DIR)

if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)


class Error(Exception):
  pass


def Log(msg):
  if Log.verbose:
    sys.stderr.write(str(msg) + '\n')
Log.verbose = False


def FindQemu():
  path = os.environ.get('PATH', '').split(os.pathsep)
  qemu_locations = [os.path.join(SCRIPT_DIR, 'qemu_arm'),
                    os.path.join(SCRIPT_DIR, 'qemu-arm')]
  qemu_locations += [os.path.join(p, 'qemu_arm') for p in path]
  qemu_locations += [os.path.join(p, 'qemu-arm') for p in path]
  # See if qemu is in any of these locations.
  qemu_bin = None
  for loc in qemu_locations:
    if os.path.isfile(loc) and os.access(loc, os.X_OK):
      qemu_bin = loc
      break
  return qemu_bin


def main(argv):
  epilog = 'Example: sel_ldr.py my_nexe.nexe'
  parser = argparse.ArgumentParser(description=__doc__, epilog=epilog)
  parser.add_argument('-v', '--verbose', action='store_true',
                      help='Verbose output')
  parser.add_argument('-d', '--debug', action='store_true',
                      help='Enable debug stub')
  parser.add_argument('-e', '--exceptions', action='store_true',
                      help='Enable exception handling interface')
  parser.add_argument('-p', '--passthrough-environment', action='store_true',
                      help='Pass environment of host through to nexe')
  parser.add_argument('--debug-libs', action='store_true',
                      help='Legacy option, do not use')
  parser.add_argument('--config', default='Release',
                      help='Use a particular library configuration (normally '
                           'Debug or Release)')
  parser.add_argument('executable', help='executable (.nexe) to run')
  parser.add_argument('args', nargs='*', help='argument to pass to exectuable')
  parser.add_argument('--library-path',
                      help='Pass extra library paths')

  # To enable bash completion for this command first install optcomplete
  # and then add this line to your .bashrc:
  #  complete -F _optcomplete sel_ldr.py
  try:
    import optcomplete
    optcomplete.autocomplete(parser)
  except ImportError:
    pass

  options = parser.parse_args(argv)

  if options.verbose:
    Log.verbose = True

  osname = getos.GetPlatform()
  if not os.path.exists(options.executable):
    raise Error('executable not found: %s' % options.executable)
  if not os.path.isfile(options.executable):
    raise Error('not a file: %s' % options.executable)

  elf_arch, dynamic = create_nmf.ParseElfHeader(options.executable)

  if elf_arch == 'arm' and osname != 'linux':
    raise Error('Cannot run ARM executables under sel_ldr on ' + osname)

  arch_suffix = elf_arch.replace('-', '_')

  sel_ldr = os.path.join(SCRIPT_DIR, 'sel_ldr_%s' % arch_suffix)
  irt = os.path.join(SCRIPT_DIR, 'irt_core_%s.nexe' % arch_suffix)
  if osname == 'win':
    sel_ldr += '.exe'
  Log('ROOT    = %s' % NACL_SDK_ROOT)
  Log('SEL_LDR = %s' % sel_ldr)
  Log('IRT     = %s' % irt)
  cmd = [sel_ldr]

  if osname == 'linux':
    # Run sel_ldr under nacl_helper_bootstrap
    helper = os.path.join(SCRIPT_DIR, 'nacl_helper_bootstrap_%s' % arch_suffix)
    Log('HELPER  = %s' % helper)
    cmd.insert(0, helper)
    cmd.append('--r_debug=0xXXXXXXXXXXXXXXXX')
    cmd.append('--reserved_at_zero=0xXXXXXXXXXXXXXXXX')

  # This script is provided mostly as way to run binaries during testing, not
  # to run untrusted code in a production environment.  As such we want it be
  # as invisible as possible.  So we pass -q (quiet) to disable most of output
  # of sel_ldr itself, and -a (disable ACL) to enable local filesystem access.
  cmd += ['-q', '-a', '-B', irt]

  # Set the default NACLVERBOSITY level LOG_ERROR (-3).  This can still be
  # overridden in the environment if debug information is desired.  However
  # in most cases we don't want the application stdout/stderr polluted with
  # sel_ldr logging.
  if 'NACLVERBOSITY' not in os.environ and not options.verbose:
    os.environ['NACLVERBOSITY'] = "-3"

  if options.debug:
    cmd.append('-g')

  if options.exceptions:
    cmd.append('-e')

  if options.passthrough_environment:
    cmd.append('-p')

  if elf_arch == 'arm':
    # Use the QEMU arm emulator if available.
    qemu_bin = FindQemu()
    if not qemu_bin:
      raise Error('Cannot run ARM executables under sel_ldr without an emulator'
          '. Try installing QEMU (http://wiki.qemu.org/).')

    arm_libpath = os.path.join(NACL_SDK_ROOT, 'tools', 'lib', 'arm_trusted')
    if not os.path.isdir(arm_libpath):
      raise Error('Could not find ARM library path: %s' % arm_libpath)
    qemu = [qemu_bin, '-cpu', 'cortex-a8', '-L', arm_libpath]
    # '-Q' disables platform qualification, allowing arm binaries to run.
    cmd = qemu + cmd + ['-Q']

  if dynamic:
    if options.debug_libs:
      sys.stderr.write('warning: --debug-libs is deprecated (use --config).\n')
      options.config = 'Debug'

    sdk_lib_dir = os.path.join(NACL_SDK_ROOT, 'lib',
                               'glibc_%s' % arch_suffix, options.config)

    if elf_arch == 'x86-64':
      lib_subdir = 'lib'
      tcarch = 'x86'
      tcsubarch = 'x86_64'
      usr_arch = 'x86_64'
    elif elf_arch == 'arm':
      lib_subdir = 'lib'
      tcarch = 'arm'
      tcsubarch = 'arm'
      usr_arch = 'arm'
    elif elf_arch == 'x86-32':
      lib_subdir = 'lib32'
      tcarch = 'x86'
      tcsubarch = 'x86_64'
      usr_arch = 'i686'
    else:
      raise Error("Unknown arch: %s" % elf_arch)

    toolchain = '%s_%s_glibc' % (osname, tcarch)
    toolchain_dir = os.path.join(NACL_SDK_ROOT, 'toolchain', toolchain)
    interp_prefix = os.path.join(toolchain_dir, tcsubarch + '-nacl')
    lib_dir = os.path.join(interp_prefix, lib_subdir)
    usr_lib_dir = os.path.join(toolchain_dir, usr_arch + '-nacl', 'usr', 'lib')

    libpath = [usr_lib_dir, sdk_lib_dir, lib_dir]

    if options.config not in ['Debug', 'Release']:
      config_fallback = 'Release'
      if 'Debug' in options.config:
        config_fallback = 'Debug'
      libpath.append(os.path.join(NACL_SDK_ROOT, 'lib',
                     'glibc_%s' % arch_suffix, config_fallback))

    if options.library_path:
      libpath.extend([os.path.abspath(p) for p
                      in options.library_path.split(':')])
    libpath = ':'.join(libpath)
    if elf_arch == 'arm':
      ldso = os.path.join(SCRIPT_DIR, 'elf_loader_arm.nexe')
      cmd.append('-E')
      cmd.append('LD_LIBRARY_PATH=%s' % libpath)
      cmd.append(ldso)
      cmd.append('--interp-prefix')
      cmd.append(interp_prefix)
    else:
      ldso = os.path.join(lib_dir, 'runnable-ld.so')
      cmd.append(ldso)
      cmd.append('--library-path')
      cmd.append(libpath)
    Log('dynamic loader = %s' % ldso)


  # Append arguments for the executable itself.
  cmd.append(options.executable)
  cmd += options.args

  Log(cmd)
  return subprocess.call(cmd)


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as e:
    sys.stderr.write(str(e) + '\n')
    sys.exit(1)

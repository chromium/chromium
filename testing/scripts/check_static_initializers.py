#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import subprocess
import sys

import common

# A list of whitelisted files that are allowed to have static initializers.
# If something adds a static initializer, revert it. We don't accept regressions
# in static initializers.
_LINUX_SI_FILE_WHITELIST = {
    'chrome': [
        'InstrProfilingRuntime.cpp',  # Only in coverage builds, not production.
        'atomicops_internals_x86.cc',  # TODO(crbug.com/973551): Remove.
        'debugallocation_shim.cc',  # TODO(crbug.com/973552): Remove.
        'iostream.cpp',  # TODO(crbug.com/973554): Remove.
        'spinlock.cc',  # TODO(crbug.com/973556): Remove.
    ],
    'nacl_helper_bootstrap': [],
}
_LINUX_SI_FILE_WHITELIST['nacl_helper'] = _LINUX_SI_FILE_WHITELIST['chrome']

# TODO: Convert Mac to use the whitelist, but need to first modify
# //tools/mac/dump-static-initializers.py to have the same stdout format as
# //tools/linux/dump-static-initializers.py.
#
# A static initializer is needed on Mac for libc++ to set up std::cin/cout/cerr
# before main() runs.
EXPECTED_MAC_SI_COUNT = 1


def run_process(command):
  p = subprocess.Popen(command, stdout=subprocess.PIPE)
  stdout = p.communicate()[0]
  if p.returncode != 0:
    raise Exception(
        'ERROR from command "%s": %d' % (' '.join(command), p.returncode))
  return stdout


def main_mac(src_dir):
  base_names = ('Chromium', 'Google Chrome')
  ret = 0
  for base_name in base_names:
    app_bundle = base_name + '.app'
    framework_name = base_name + ' Framework'
    framework_bundle = framework_name + '.framework'
    framework_dsym_bundle = framework_bundle + '.dSYM'
    framework_unstripped_name = framework_name + '.unstripped'
    chromium_executable = os.path.join(app_bundle, 'Contents', 'MacOS',
                                       base_name)
    chromium_framework_executable = os.path.join(framework_bundle,
                                                 framework_name)
    chromium_framework_dsym = os.path.join(framework_dsym_bundle, 'Contents',
                                           'Resources', 'DWARF', framework_name)
    if os.path.exists(chromium_executable):
      # Count the number of files with at least one static initializer.
      si_count = 0
      # Find the __DATA,__mod_init_func section.
      stdout = run_process(['otool', '-l', chromium_framework_executable])
      section_index = stdout.find('sectname __mod_init_func')
      if section_index != -1:
        # If the section exists, the "size" line must follow it.
        initializers_s = re.search('size 0x([0-9a-f]+)',
                                   stdout[section_index:]).group(1)
        word_size = 8  # Assume 64 bit
        si_count = int(initializers_s, 16) / word_size

      # Print the list of static initializers.
      if si_count > EXPECTED_MAC_SI_COUNT:
        print('Expected <= %d static initializers in %s, but found %d' %
              (EXPECTED_MAC_SI_COUNT, chromium_framework_executable, si_count))
        ret = 1

        # First look for a dSYM to get information about the initializers. If
        # one is not present, check if there is an unstripped copy of the build
        # output.
        mac_tools_path = os.path.join(src_dir, 'tools', 'mac')
        if os.path.exists(chromium_framework_dsym):
          dump_static_initializers = os.path.join(
              mac_tools_path, 'dump-static-initializers.py')
          stdout = run_process(
              [dump_static_initializers, chromium_framework_dsym])
          print stdout
        else:
          show_mod_init_func = os.path.join(mac_tools_path,
                                            'show_mod_init_func.py')
          args = [show_mod_init_func]
          if os.path.exists(framework_unstripped_name):
            args.append(framework_unstripped_name)
          else:
            print '# Warning: Falling back to potentially stripped output.'
            args.append(chromium_framework_executable)
          stdout = run_process(args)
          print stdout
  return ret


def main_linux(src_dir):
  ret = 0
  for binary_name in _LINUX_SI_FILE_WHITELIST:
    if not os.path.exists(binary_name):
      continue

    dump_static_initializers = os.path.join(src_dir, 'tools', 'linux',
                                            'dump-static-initializers.py')
    stdout = run_process([dump_static_initializers, '-d', binary_name])
    # The output has the following format:
    # First lines: '# <file_name> <si_name>'
    # Last line: '# Found <num> static initializers in <num> files.'
    #
    # For example:
    # # spinlock.cc GetSystemCPUsCount()
    # # spinlock.cc adaptive_spin_count
    # # Found 2 static initializers in 1 files.

    files_with_si = set()
    for line in stdout.splitlines()[:-1]:
      parts = line.split(' ', 2)
      assert len(parts) == 3 and parts[0] == '#'

      files_with_si.add(parts[1])

    for f in files_with_si:
      if f not in _LINUX_SI_FILE_WHITELIST[binary_name]:
        ret = 1
        print('Error: file "%s" is not expected to have static initializers in'
              ' binary "%s"') % (f, binary_name)

    print '\n# Static initializers in %s:' % binary_name
    print stdout

  return ret


def main_run(args):
  if args.build_config_fs != 'Release':
    raise Exception('Only release builds are supported')

  src_dir = args.paths['checkout']
  build_dir = os.path.join(src_dir, 'out', args.build_config_fs)
  os.chdir(build_dir)

  if sys.platform.startswith('darwin'):
    rc = main_mac(src_dir)
  elif sys.platform == 'linux2':
    rc = main_linux(src_dir)
  else:
    sys.stderr.write('Unsupported platform %s.\n' % repr(sys.platform))
    return 2

  json.dump({
      'valid': rc == 0,
      'failures': [],
  }, args.output)

  return rc


def main_compile_targets(args):
  if sys.platform.startswith('darwin'):
    compile_targets = ['chrome']
  elif sys.platform == 'linux2':
    compile_targets = ['chrome', 'nacl_helper', 'nacl_helper_bootstrap']
  else:
    compile_targets = []

  json.dump(compile_targets, args.output)

  return 0


if __name__ == '__main__':
  funcs = {
      'run': main_run,
      'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))

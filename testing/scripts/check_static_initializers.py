#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import copy
import json
import os
import re
import subprocess
import sys

# Add src/testing/ into sys.path for importing common without pylint errors.
sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), os.path.pardir)))
from scripts import common

# A list of filename regexes that are allowed to have static initializers.
# If something adds a static initializer, revert it. We don't accept regressions
# in static initializers.
_SHARED_LINUX_CROS_SI_ALLOWLIST = {
    'chrome': [
        # Only in coverage builds, not production.
        'InstrProfilingRuntime\\.cpp : ' +
        '_GLOBAL__sub_I_InstrProfilingRuntime\\.cpp',

        # TODO(crbug.com/973554): Remove.
        'iostream\\.cpp : _GLOBAL__I_000100',

        # TODO(crbug.com/1445935): Rust stdlib argv handling.
        # https://github.com/rust-lang/rust/blob/b08148f6a76010ea3d4e91d61245aa7aac59e4b4/library/std/src/sys/unix/args.rs#L107-L127
        # https://github.com/rust-lang/rust/issues/111921
        '.* : std::sys::unix::args::imp::ARGV_INIT_ARRAY::init_wrapper',
    ],
    'nacl_helper_bootstrap': [],
}

# The lists for Linux and ChromeOS are similar, but some Linux-specific entries
# need to be added below.  If something adds a static initializer, revert it. We
# don't accept regressions in static initializers.
_LINUX_SI_ALLOWLIST = copy.deepcopy(_SHARED_LINUX_CROS_SI_ALLOWLIST)
_LINUX_SI_ALLOWLIST['chrome'].extend([
    # Added by libgcc due to USE_EH_FRAME_REGISTRY.
    'crtstuff\\.c : frame_dummy',
])

# The lists for Linux and ChromeOS are similar, but some ChromeOS-specific
# entries need to be added below.  If something adds a static initializer,
# revert it. We don't accept regressions in static initializers.
_CROS_SI_ALLOWLIST = copy.deepcopy(_SHARED_LINUX_CROS_SI_ALLOWLIST)
_CROS_SI_ALLOWLIST['chrome'].extend([
    '.*000100.*',       # libc++ uses init_priority 100 for iostreams.
])

# `nacl_helper` has the same expectations on Linux and CrOS.
_LINUX_SI_ALLOWLIST['nacl_helper'] = _LINUX_SI_ALLOWLIST['chrome']
_CROS_SI_ALLOWLIST['nacl_helper'] = _LINUX_SI_ALLOWLIST['chrome']

# Mac can use this list when a dsym is available, otherwise it will fall back
# to checking the count.
_MAC_SI_FILE_ALLOWLIST = [
    'InstrProfilingRuntime\\.cpp', # Only in coverage builds, not in production.
    'sysinfo\\.cc', # Only in coverage builds, not in production.
    'iostream\\.cpp', # Used to setup std::cin/cout/cerr.
    '000100', # Used to setup std::cin/cout/cerr
]

# Two static initializers are needed on Mac for libc++ to set up
# std::cin/cout/cerr before main() runs. Only iostream.cpp needs to be counted
# here. Plus, PartitionAlloc-Everywhere uses one static initializer
# (InitializeDefaultMallocZoneWithPartitionAlloc) to install a malloc zone.
FALLBACK_EXPECTED_MAC_SI_COUNT = 3

# Similar to mac, iOS needs the iosstream and PartitionAlloc-Everywhere static
# initializer (InitializeDefaultMallocZoneWithPartitionAlloc) to install a
# malloc zone.
FALLBACK_EXPECTED_IOS_SI_COUNT = 2

# For coverage builds, also allow 'IntrProfilingRuntime.cpp'
COVERAGE_BUILD_FALLBACK_EXPECTED_MAC_SI_COUNT = 4

def get_mod_init_count(executable, hermetic_xcode_path):
  # Find the __DATA,__mod_init_func section.
  if os.path.exists(hermetic_xcode_path):
    otool_path = os.path.join(hermetic_xcode_path, 'Contents', 'Developer',
        'Toolchains', 'XcodeDefault.xctoolchain', 'usr', 'bin', 'otool')
  else:
    otool_path = 'otool'

  stdout = run_process([otool_path, '-l', executable])
  section_index = stdout.find('sectname __mod_init_func')
  if section_index == -1:
    return 0

  # If the section exists, the "size" line must follow it.
  initializers_s = re.search('size 0x([0-9a-f]+)',
                             stdout[section_index:]).group(1)
  word_size = 8  # Assume 64 bit
  return int(initializers_s, 16) / word_size

def run_process(command):
  p = subprocess.Popen(command, stdout=subprocess.PIPE, universal_newlines=True)
  stdout = p.communicate()[0]
  if p.returncode != 0:
    raise Exception(
        'ERROR from command "%s": %d' % (' '.join(command), p.returncode))
  return stdout

def main_ios(src_dir, hermetic_xcode_path):
  base_names = ('Chromium', 'Chrome')
  ret = 0
  for base_name in base_names:
    app_bundle = base_name + '.app'
    chromium_executable = os.path.join(app_bundle, base_name)
    if os.path.exists(chromium_executable):
      si_count = get_mod_init_count(chromium_executable,
                                    hermetic_xcode_path)
      if si_count > 0:
        allowed_si_count = FALLBACK_EXPECTED_IOS_SI_COUNT
        if si_count > allowed_si_count:
          print('Expected <= %d static initializers in %s, but found %d' %
              (allowed_si_count, chromium_executable,
              si_count))
          ret = 1
          show_mod_init_func = os.path.join(src_dir, 'tools', 'mac',
                                            'show_mod_init_func.py')
          args = [show_mod_init_func]
          args.append(chromium_executable)

          if os.path.exists(hermetic_xcode_path):
            args.extend(['--xcode-path', hermetic_xcode_path])
          stdout = run_process(args)
          print(stdout)
  return ret


def main_mac(src_dir, hermetic_xcode_path, allow_coverage_initializer = False):
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
      si_count = get_mod_init_count(chromium_framework_executable,
                                    hermetic_xcode_path)

      # Print the list of static initializers.
      if si_count > 0:
        # First look for a dSYM to get information about the initializers. If
        # one is not present, check if there is an unstripped copy of the build
        # output.
        mac_tools_path = os.path.join(src_dir, 'tools', 'mac')
        if os.path.exists(chromium_framework_dsym):
          dump_static_initializers = os.path.join(
              mac_tools_path, 'dump-static-initializers.py')
          stdout = run_process(
              [dump_static_initializers, chromium_framework_dsym])
          for line in stdout:
            if re.match('0x[0-9a-f]+', line) and not any(
                re.match(f, line) for f in _MAC_SI_FILE_ALLOWLIST):
              ret = 1
              print('Found invalid static initializer: {}'.format(line))
          print(stdout)
        else:
          allowed_si_count = FALLBACK_EXPECTED_MAC_SI_COUNT
          if allow_coverage_initializer:
            allowed_si_count = COVERAGE_BUILD_FALLBACK_EXPECTED_MAC_SI_COUNT
          if si_count > allowed_si_count:
            print('Expected <= %d static initializers in %s, but found %d' %
                (allowed_si_count, chromium_framework_executable,
                si_count))
            ret = 1
            show_mod_init_func = os.path.join(mac_tools_path,
                                              'show_mod_init_func.py')
            args = [show_mod_init_func]
            if os.path.exists(framework_unstripped_name):
              args.append(framework_unstripped_name)
            else:
              print('# Warning: Falling back to potentially stripped output.')
              args.append(chromium_framework_executable)

            if os.path.exists(hermetic_xcode_path):
              args.extend(['--xcode-path', hermetic_xcode_path])

            stdout = run_process(args)
            print(stdout)
  return ret


def main_linux(src_dir, is_chromeos):
  ret = 0
  allowlist = _CROS_SI_ALLOWLIST if is_chromeos else \
      _LINUX_SI_ALLOWLIST
  for binary_name in allowlist:
    if not os.path.exists(binary_name):
      continue

    dump_static_initializers = os.path.join(src_dir, 'tools', 'linux',
                                            'dump-static-initializers.py')
    stdout = run_process([dump_static_initializers, '--json', binary_name])
    entries = json.loads(stdout)['entries']

    for e in entries:
      # Get the basename and remove line number suffix.
      basename = os.path.basename(e['filename']).split(':')[0]
      symbol = e['symbol_name']
      descriptor = f"{basename} : {symbol}"
      if not any(re.match(p, descriptor) for p in allowlist[binary_name]):
        ret = 1
        print(('Error: file "%s" is not expected to have static initializers in'
               ' binary "%s", but found "%s"') % (e['filename'], binary_name,
                                                  e['symbol_name']))

    print('\n# Static initializers in %s:' % binary_name)
    for e in entries:
      print('# 0x%x %s %s' % (e['address'], e['filename'], e['symbol_name']))
      print(e['disassembly'])

    print('Found %d files containing static initializers.' % len(entries))
  return ret


def main_run(args):
  if args.build_config_fs != 'Release':
    raise Exception('Only release builds are supported')

  src_dir = args.paths['checkout']
  build_dir = os.path.join(src_dir, 'out', args.build_config_fs)
  os.chdir(build_dir)

  if sys.platform.startswith('darwin'):
    # If the checkout uses the hermetic xcode binaries, then otool must be
    # directly invoked. The indirection via /usr/bin/otool won't work unless
    # there's an actual system install of Xcode.
    hermetic_xcode_path = os.path.join(src_dir, 'build', 'mac_files',
        'xcode_binaries')

    is_ios = 'target_platform' in args.properties and \
      'ios' in args.properties['target_platform']
    if is_ios:
      rc = main_ios(src_dir, hermetic_xcode_path)
    else:
      rc = main_mac(src_dir, hermetic_xcode_path,
        allow_coverage_initializer = '--allow-coverage-initializer' in \
          args.args)
  elif sys.platform.startswith('linux'):
    is_chromeos = 'buildername' in args.properties and \
        'chromeos' in args.properties['buildername']
    rc = main_linux(src_dir, is_chromeos)
  else:
    sys.stderr.write('Unsupported platform %s.\n' % repr(sys.platform))
    return 2

  common.record_local_script_results(
      'check_static_initializers', args.output, [], rc == 0)

  return rc


def main_compile_targets(args):
  if sys.platform.startswith('darwin'):
    compile_targets = ['chrome']
  elif sys.platform.startswith('linux'):
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

#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import subprocess
import sys

import common

CHROMIUM_ROOT = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
BUILD_DIR = os.path.join(CHROMIUM_ROOT, 'build')

if BUILD_DIR not in sys.path:
  sys.path.insert(0, BUILD_DIR)
import gn_helpers


# A list of filename regexes that are allowed to have static initializers.
# If something adds a static initializer, revert it. We don't accept regressions
# in static initializers.
_LINUX_SI_ALLOWLIST = {
    'chrome': [
        # Only in coverage builds, not production.
        'InstrProfilingRuntime\\.cpp : ' +
        '_GLOBAL__sub_I_InstrProfilingRuntime\\.cpp',

        # TODO(crbug.com/41464604): Remove.
        'iostream\\.cpp : _GLOBAL__I_000100',

        # TODO(crbug.com/40268361): Rust stdlib argv handling.
        # https://github.com/rust-lang/rust/blob/b08148f6a76010ea3d4e91d61245aa7aac59e4b4/library/std/src/sys/unix/args.rs#L107-L127
        # https://github.com/rust-lang/rust/issues/111921
        '.* : std::sys::pal::unix::args::imp::ARGV_INIT_ARRAY::init_wrapper',

        # Added by libgcc due to USE_EH_FRAME_REGISTRY.
        'crtstuff\\.c : frame_dummy',
    ],
}

# Mac can use this list when a dsym is available, otherwise it will fall back
# to checking the count.
_MAC_SI_FILE_ALLOWLIST = [
    # Only in coverage builds, not in production.
    'InstrProfilingRuntime\\.cpp',
    'sysinfo\\.cc',  # Only in coverage builds, not in production.
    'iostream\\.cpp',  # Used to setup std::cin/cout/cerr.
    '000100',  # Used to setup std::cin/cout/cerr
]

# The minimum for Mac is:
# _GLOBAL__I_000100
# InitializeDefaultMallocZoneWithPartitionAlloc()
FALLBACK_MIN_MAC_SI_COUNT = 2

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


# Returns true if args contains properties which look like a chromeos-esque
# builder.
def check_if_chromeos(args):
  return 'buildername' in args.properties and \
      'chromeos' in args.properties['buildername']


def get_mod_init_count(src_dir, executable, hermetic_xcode_path):
  show_mod_init_func = os.path.join(src_dir, 'tools', 'mac',
                                    'show_mod_init_func.py')
  args = [show_mod_init_func]
  args.append(executable)
  if os.path.exists(hermetic_xcode_path):
    args.extend(['--xcode-path', hermetic_xcode_path])
  stdout = run_process(args)
  si_count = len(stdout.splitlines()) - 1  # -1 for executable name
  return (stdout, si_count)


def run_process(command):
  p = subprocess.Popen(command, stdout=subprocess.PIPE, universal_newlines=True)
  stdout = p.communicate()[0]
  if p.returncode != 0:
    raise Exception('ERROR from command "%s": %d' %
                    (' '.join(command), p.returncode))
  return stdout


def main_ios(src_dir, hermetic_xcode_path):
  base_names = ('Chromium', 'Chrome')
  ret = 0
  for base_name in base_names:
    app_bundle = base_name + '.app'
    chromium_executable = os.path.join(app_bundle, base_name)
    if os.path.exists(chromium_executable):
      stdout, si_count = get_mod_init_count(src_dir, chromium_executable,
                                            hermetic_xcode_path)
      expected_si_count = FALLBACK_EXPECTED_IOS_SI_COUNT
      if si_count != expected_si_count:
        print('Expected %d static initializers in %s, but found %d' %
              (expected_si_count, chromium_executable, si_count))
        print(stdout)
        ret = 1
  return ret


def main_mac(src_dir, hermetic_xcode_path, allow_coverage_initializer=False):
  base_names = ('Chromium', 'Google Chrome')
  ret = 0
  for base_name in base_names:
    app_bundle = base_name + '.app'
    framework_name = base_name + ' Framework'
    framework_bundle = framework_name + '.framework'
    chromium_executable = os.path.join(app_bundle, 'Contents', 'MacOS',
                                       base_name)
    chromium_framework_executable = os.path.join(framework_bundle,
                                                 framework_name)
    if os.path.exists(chromium_executable):
      # Count the number static initializers.
      stdout, si_count = get_mod_init_count(src_dir,
                                            chromium_framework_executable,
                                            hermetic_xcode_path)
      min_si_count = FALLBACK_MIN_MAC_SI_COUNT
      allowed_si_count = FALLBACK_EXPECTED_MAC_SI_COUNT
      if allow_coverage_initializer:
        allowed_si_count = COVERAGE_BUILD_FALLBACK_EXPECTED_MAC_SI_COUNT
      if si_count > allowed_si_count or si_count < min_si_count:
        print('Expected [%d, %d] static initializers in %s, but found %d' %
              (min_si_count, allowed_si_count, chromium_framework_executable,
               si_count))
        print(stdout)
        ret = 1
  return ret


def main_linux(src_dir):
  ret = 0
  allowlist = _LINUX_SI_ALLOWLIST
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
      descriptor = f'{basename} : {symbol}'
      if not any(re.match(p, descriptor) for p in allowlist[binary_name]):
        ret = 1
        print(('Error: file "%s" is not expected to have static initializers in'
               ' binary "%s", but found "%s"') %
              (e['filename'], binary_name, e['symbol_name']))

    print('\n# Static initializers in %s:' % binary_name)
    for e in entries:
      print('# 0x%x %s %s' % (e['address'], e['filename'], e['symbol_name']))
      print(e['disassembly'])

    print('Found %d files containing static initializers.' % len(entries))
  return ret


def main_run(args):
  with open(os.path.join(args.build_dir, 'args.gn')) as f:
    gn_args = gn_helpers.FromGNArgs(f.read())
  if gn_args.get('is_debug') or gn_args.get('is_official_build'):
    raise Exception('Only release builds are supported')

  src_dir = args.paths['checkout']
  os.chdir(args.build_dir)

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
    # TODO(crbug.com/40285648): Delete this assert if it's not seen to fail
    # anywhere.
    assert not check_if_chromeos(args), (
        'This script is no longer supported for CrOS')
    rc = main_linux(src_dir)
  else:
    sys.stderr.write('Unsupported platform %s.\n' % repr(sys.platform))
    return 2

  common.record_local_script_results('check_static_initializers', args.output,
                                     [], rc == 0)

  return rc


def main_compile_targets(args):
  if sys.platform.startswith('darwin'):
    if 'ios' in args.properties.get('target_platform', []):
      compile_targets = ['ios/chrome/app:chrome']
    else:
      compile_targets = ['chrome']
  elif sys.platform.startswith('linux'):
    compile_targets = ['chrome']
  else:
    compile_targets = []

  with open(args.output.name, 'w') as fd:
    json.dump(compile_targets, fd)

  return 0


if __name__ == '__main__':
  funcs = {
      'run': main_run,
      'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))

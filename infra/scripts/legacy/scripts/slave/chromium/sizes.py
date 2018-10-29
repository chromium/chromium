#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to extract size information for chrome, executed by buildbot.

When this is run, the current directory (cwd) should be the outer build
directory (e.g., chrome-release/build/).

For a list of command-line options, call this script with '--help'.
"""

import errno
import glob
import json
import platform
import optparse
import os
import re
import stat
import subprocess
import sys
import tempfile

from slave import build_directory


SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..', '..'))

# If something adds a static initializer, revert it, don't increase these
# numbers. We don't accept regressions in static initializers.
#
# Note: Counts for chrome and nacl_helper are one higher in branded builds
# compared to release builds.  This is due to a static initializer in
# WelsThreadPool.cpp (https://crbug.com/893594).
EXPECTED_LINUX_SI_COUNTS = {
  'chrome': 5,
  'nacl_helper': 5,
  'nacl_helper_bootstrap': 0,
}

# If something adds a static initializer, revert it, don't increase these
# numbers. We don't accept regressions in static initializers.
EXPECTED_MAC_SI_COUNT = 1  # https://crbug.com/893594


class ResultsCollector(object):
  def __init__(self):
    self.results = {}
    self.failures = []

  def add_result(self, name, identifier, value, units):
    assert name not in self.results
    self.results[name] = {
      'identifier': identifier,
      'value': int(value),
      'units': units
    }

    # Legacy printing, previously used for parsing the text logs.
    print 'RESULT %s: %s= %s %s' % (name, identifier, value, units)

  def add_failure(self, failure):
    self.failures.append(failure)


def get_size(filename):
  return os.stat(filename)[stat.ST_SIZE]


def get_linux_stripped_size(filename):
  EU_STRIP_NAME = 'eu-strip'
  # Assumes |filename| is in out/Release
  # build/linux/bin/eu-strip'
  src_dir = os.path.dirname(os.path.dirname(os.path.dirname(filename)))
  eu_strip_path = os.path.join(src_dir, 'build', 'linux', 'bin', EU_STRIP_NAME)
  if (platform.architecture()[0] == '64bit' or
      not os.path.exists(eu_strip_path)):
    eu_strip_path = EU_STRIP_NAME

  with tempfile.NamedTemporaryFile() as stripped_file:
    strip_cmd = [eu_strip_path, '-o', stripped_file.name, filename]
    result = 0
    result, _ = run_process(result, strip_cmd)
    if result != 0:
      return (result, 0)
    return (result, get_size(stripped_file.name))


def run_process(result, command):
  p = subprocess.Popen(command, stdout=subprocess.PIPE)
  stdout = p.communicate()[0]
  if p.returncode != 0:
    print 'ERROR from command "%s": %d' % (' '.join(command), p.returncode)
    if result == 0:
      result = p.returncode
  return result, stdout


def print_si_fail_hint(path_to_tool):
  """Print a hint regarding how to handle a static initializer failure."""
  print '# HINT: To get this list, run %s' % path_to_tool
  print '# HINT: diff against the log from the last run to see what changed'


def main_mac(options, args, results_collector):
  """Print appropriate size information about built Mac targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  build_dir = build_directory.GetBuildOutputDirectory(SRC_DIR)
  target_dir = os.path.join(build_dir, options.target)

  result = 0
  # Work with either build type.
  base_names = ('Chromium', 'Google Chrome')
  for base_name in base_names:
    app_bundle = base_name + '.app'
    framework_name = base_name + ' Framework'
    framework_bundle = framework_name + '.framework'
    framework_dsym_bundle = framework_bundle + '.dSYM'
    framework_unstripped_name = framework_name + '.unstripped'

    chromium_app_dir = os.path.join(target_dir, app_bundle)
    chromium_executable = os.path.join(chromium_app_dir,
                                       'Contents', 'MacOS', base_name)

    chromium_framework_dir = os.path.join(target_dir, framework_bundle)
    chromium_framework_executable = os.path.join(chromium_framework_dir,
                                                 framework_name)

    chromium_framework_dsym_dir = os.path.join(target_dir,
                                               framework_dsym_bundle)
    chromium_framework_dsym = os.path.join(chromium_framework_dsym_dir,
                                           'Contents', 'Resources', 'DWARF',
                                           framework_name)
    chromium_framework_unstripped = os.path.join(target_dir,
                                                 framework_unstripped_name)
    if os.path.exists(chromium_executable):
      print_dict = {
        # Remove spaces in the names so any downstream processing is less
        # likely to choke.
        'app_name'         : re.sub(r'\s', '', base_name),
        'app_bundle'       : re.sub(r'\s', '', app_bundle),
        'framework_name'   : re.sub(r'\s', '', framework_name),
        'framework_bundle' : re.sub(r'\s', '', framework_bundle),
        'app_size'         : get_size(chromium_executable),
        'framework_size'   : get_size(chromium_framework_executable)
      }

      # Collect the segment info out of the App
      result, stdout = run_process(result, ['size', chromium_executable])
      print_dict['app_text'], print_dict['app_data'], print_dict['app_objc'] = \
          re.search(r'(\d+)\s+(\d+)\s+(\d+)', stdout).groups()

      # Collect the segment info out of the Framework
      result, stdout = run_process(result, ['size',
                                            chromium_framework_executable])
      print_dict['framework_text'], print_dict['framework_data'], \
        print_dict['framework_objc'] = \
          re.search(r'(\d+)\s+(\d+)\s+(\d+)', stdout).groups()

      # Collect the whole size of the App bundle on disk (include the framework)
      result, stdout = run_process(result, ['du', '-s', '-k', chromium_app_dir])
      du_s = re.search(r'(\d+)', stdout).group(1)
      print_dict['app_bundle_size'] = (int(du_s) * 1024)

      # Count the number of files with at least one static initializer.
      si_count = 0
      # Find the __DATA,__mod_init_func section.
      result, stdout = run_process(result,
          ['otool', '-l', chromium_framework_executable])
      section_index = stdout.find('sectname __mod_init_func')
      if section_index != -1:
        # If the section exists, the "size" line must follow it.
        initializers_s = re.search('size 0x([0-9a-f]+)',
                                   stdout[section_index:]).group(1)
        word_size = 8  # Assume 64 bit
        si_count = int(initializers_s, 16) / word_size
      print_dict['initializers'] = si_count

      # For Release builds only, use dump-static-initializers.py to print the
      # list of static initializers.
      if si_count > EXPECTED_MAC_SI_COUNT and options.target == 'Release':
        result = 125
        results_collector.add_failure(
            'Expected 0 static initializers in %s, but found %d' %
            (chromium_framework_executable, si_count))
        print '\n# Static initializers in %s:' % chromium_framework_executable

        # First look for a dSYM to get information about the initializers. If
        # one is not present, check if there is an unstripped copy of the build
        # output.
        mac_tools_path = os.path.join(os.path.dirname(build_dir),
                                      'tools', 'mac')
        if os.path.exists(chromium_framework_dsym):
          dump_static_initializers = os.path.join(
              mac_tools_path, 'dump-static-initializers.py')
          result, stdout = run_process(result, [dump_static_initializers,
                                                chromium_framework_dsym])
          print_si_fail_hint('tools/mac/dump-static-initializers.py')
          print stdout
        else:
          show_mod_init_func = os.path.join(
              mac_tools_path, 'show_mod_init_func.py')
          args = [show_mod_init_func]
          if os.path.exists(chromium_framework_unstripped):
            args.append(chromium_framework_unstripped)
          else:
            print '# Warning: Falling back to potentially stripped output.'
            args.append(chromium_framework_executable)
          result, stdout = run_process(result, args)
          print_si_fail_hint('tools/mac/show_mod_init_func.py')
          print stdout


      results_collector.add_result(
          print_dict['app_name'], print_dict['app_name'],
          print_dict['app_size'], 'bytes')
      results_collector.add_result(
          '%s-__TEXT' % print_dict['app_name'], '__TEXT',
          print_dict['app_text'], 'bytes')
      results_collector.add_result(
          '%s-__DATA' % print_dict['app_name'], '__DATA',
          print_dict['app_data'], 'bytes')
      results_collector.add_result(
          '%s-__OBJC' % print_dict['app_name'], '__OBJC',
          print_dict['app_objc'], 'bytes')
      results_collector.add_result(
          print_dict['framework_name'], print_dict['framework_name'],
          print_dict['framework_size'], 'bytes')
      results_collector.add_result(
          '%s-__TEXT' % print_dict['framework_name'], '__TEXT',
          print_dict['framework_text'], 'bytes')
      results_collector.add_result(
          '%s-__DATA' % print_dict['framework_name'], '__DATA',
          print_dict['framework_data'], 'bytes')
      results_collector.add_result(
          '%s-__OBJC' % print_dict['framework_name'], '__OBJC',
          print_dict['framework_objc'], 'bytes')
      results_collector.add_result(
          print_dict['app_bundle'], print_dict['app_bundle'],
          print_dict['app_bundle_size'], 'bytes')
      results_collector.add_result(
          'chrome-si', 'initializers',
          print_dict['initializers'], 'files')

      # Found a match, don't check the other base_names.
      return result
  # If no base_names matched, fail script.
  return 66


def check_linux_binary(target_dir, binary_name, options, results_collector):
  """Collect appropriate size information about the built Linux binary given.

  Returns a tuple (result, sizes).  result is the first non-zero exit
  status of any command it executes, or zero on success.  sizes is a list
  of tuples (name, identifier, totals_identifier, value, units).
  The printed line looks like:
    name: identifier= value units
  When this same data is used for totals across all the binaries, then
  totals_identifier is the identifier to use, or '' to just use identifier.
  """
  binary_file = os.path.join(target_dir, binary_name)

  if not os.path.exists(binary_file):
    # Don't print anything for missing files.
    return 0, []

  result = 0
  sizes = []

  def get_elf_section_size(readelf_stdout, section_name):
    # Matches: .ctors PROGBITS 000000000516add0 5169dd0 000010 00 WA 0 0 8
    match = re.search(r'\.%s.*$' % re.escape(section_name),
                      readelf_stdout, re.MULTILINE)
    if not match:
      return (False, -1)
    size_str = re.split(r'\W+', match.group(0))[5]
    return (True, int(size_str, 16))

  sizes.append((binary_name, binary_name, 'size',
                get_size(binary_file), 'bytes'))


  result, stripped_size = get_linux_stripped_size(binary_file)
  sizes.append((binary_name + '-stripped', 'stripped', 'stripped',
                stripped_size, 'bytes'))

  result, stdout = run_process(result, ['size', binary_file])
  text, data, bss = re.search(r'(\d+)\s+(\d+)\s+(\d+)', stdout).groups()
  sizes += [
      (binary_name + '-text', 'text', '', text, 'bytes'),
      (binary_name + '-data', 'data', '', data, 'bytes'),
      (binary_name + '-bss', 'bss', '', bss, 'bytes'),
      ]

  # Find the number of files with at least one static initializer.
  # First determine if we're 32 or 64 bit
  result, stdout = run_process(result, ['readelf', '-h', binary_file])
  elf_class_line = re.search('Class:.*$', stdout, re.MULTILINE).group(0)
  elf_class = re.split(r'\W+', elf_class_line)[1]
  if elf_class == 'ELF32':
    word_size = 4
  else:
    word_size = 8

  # Then find the number of files with global static initializers.
  # NOTE: this is very implementation-specific and makes assumptions
  # about how compiler and linker implement global static initializers.
  si_count = 0
  result, stdout = run_process(result, ['readelf', '-SW', binary_file])
  has_init_array, init_array_size = get_elf_section_size(stdout, 'init_array')
  if has_init_array:
    si_count = init_array_size / word_size
    # In newer versions of gcc crtbegin.o inserts frame_dummy into .init_array
    # but we don't want to count this entry, since its alwasys present and not
    # related to our code.
    si_count -= 1
  si_count = max(si_count, 0)
  sizes.append((binary_name + '-si', 'initializers', '', si_count, 'files'))

  # For Release builds only, use dump-static-initializers.py to print the list
  # of static initializers.
  if options.target == 'Release':
    if (binary_name in EXPECTED_LINUX_SI_COUNTS and
        si_count > EXPECTED_LINUX_SI_COUNTS[binary_name]):
      result = 125
      results_collector.add_failure(
          'Expected <= %d static initializers in %s, but found %d' %
          (EXPECTED_LINUX_SI_COUNTS[binary_name], binary_name, si_count))
    if si_count > 0:
      build_dir = os.path.dirname(target_dir)
      dump_static_initializers = os.path.join(os.path.dirname(build_dir),
                                              'tools', 'linux',
                                              'dump-static-initializers.py')
      result, stdout = run_process(result, [dump_static_initializers,
                                            '-d', binary_file])
      print '\n# Static initializers in %s:' % binary_file
      print_si_fail_hint('tools/linux/dump-static-initializers.py')
      print stdout

  # Determine if the binary has the DT_TEXTREL marker.
  result, stdout = run_process(result, ['readelf', '-Wd', binary_file])
  if re.search(r'\bTEXTREL\b', stdout) is None:
    # Nope, so the count is zero.
    count = 0
  else:
    # There are some, so count them.
    result, stdout = run_process(result, ['eu-findtextrel', binary_file])
    count = stdout.count('\n')
  sizes.append((binary_name + '-textrel', 'textrel', '', count, 'relocs'))

  return result, sizes


def main_linux(options, args, results_collector):
  """Print appropriate size information about built Linux targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  build_dir = build_directory.GetBuildOutputDirectory(SRC_DIR)
  target_dir = os.path.join(build_dir, options.target)

  binaries = [
      'chrome',
      'nacl_helper',
      'nacl_helper_bootstrap',
      'libffmpegsumo.so',
      'libgcflashplayer.so',
      'libppGoogleNaClPluginChrome.so',
  ]

  result = 0

  totals = {}

  for binary in binaries:
    this_result, this_sizes = check_linux_binary(target_dir, binary, options,
                                                 results_collector)
    if result == 0:
      result = this_result
    for name, identifier, totals_id, value, units in this_sizes:
      results_collector.add_result(name, identifier, value, units)
      totals_id = totals_id or identifier, units
      totals[totals_id] = totals.get(totals_id, 0) + int(value)

  files = [
    'nacl_irt_x86_64.nexe',
    'resources.pak',
  ]

  for filename in files:
    path = os.path.join(target_dir, filename)
    try:
      size = get_size(path)
    except OSError, e:
      if e.errno == errno.ENOENT:
        continue  # Don't print anything for missing files.
      raise
    results_collector.add_result(filename, filename, size, 'bytes')
    totals['size', 'bytes'] += size

  # TODO(mcgrathr): This should all be refactored so the mac and win flavors
  # also deliver data structures rather than printing, and the logic for
  # the printing and the summing totals is shared across all three flavors.
  for (identifier, units), value in sorted(totals.iteritems()):
    results_collector.add_result(
        'totals-%s' % identifier, identifier, value, units)

  return result


def check_android_binaries(binaries, target_dir, options, results_collector,
                           binaries_to_print=None):
  """Common method for printing size information for Android targets.

  Prints size information for each element of binaries in target_dir.
  If binaries_to_print is specified, the name of each binary from
  binaries is replaced with corresponding element of binaries_to_print
  in output. Returns the first non-zero exit status of any command it
  executes, or zero on success.
  """
  result = 0
  if not binaries_to_print:
    binaries_to_print = binaries

  for (binary, binary_to_print) in zip(binaries, binaries_to_print):
    this_result, this_sizes = check_linux_binary(target_dir, binary, options,
                                                 results_collector)
    if result == 0:
      result = this_result
    for name, identifier, _, value, units in this_sizes:
      name = name.replace('/', '_').replace(binary, binary_to_print)
      identifier = identifier.replace(binary, binary_to_print)
      results_collector.add_result(name, identifier, value, units)

  return result


def main_android(options, args, results_collector):
  """Print appropriate size information about built Android targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  target_dir = os.path.join(build_directory.GetBuildOutputDirectory(SRC_DIR),
                            options.target)

  binaries = [
      'chrome_public_apk/libs/armeabi-v7a/libchrome.so',
      'lib/libchrome.so',
      'libchrome.so',
  ]

  return check_android_binaries(binaries, target_dir, options,
                                results_collector)


def main_android_webview(options, args, results_collector):
  """Print appropriate size information about Android WebViewChromium targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  target_dir = os.path.join(build_directory.GetBuildOutputDirectory(SRC_DIR),
                            options.target)

  binaries = ['lib/libwebviewchromium.so',
              'libwebviewchromium.so']

  return check_android_binaries(binaries, target_dir, options,
                                results_collector)


def main_android_cronet(options, args, results_collector):
  """Print appropriate size information about Android Cronet targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  target_dir = os.path.join(build_directory.GetBuildOutputDirectory(SRC_DIR),
                            options.target)
  # Use version in binary file name, but not in printed output.
  binaries_with_paths = glob.glob(os.path.join(target_dir,'libcronet.*.so'))
  num_binaries = len(binaries_with_paths)
  assert num_binaries == 1, "Got %d binaries" % (num_binaries,)
  binaries = [os.path.basename(binaries_with_paths[0])]
  binaries_to_print = ['libcronet.so']

  return check_android_binaries(binaries, target_dir, options,
                                results_collector, binaries_to_print)


def main_win(options, args, results_collector):
  """Print appropriate size information about built Windows targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  files = [
    'chrome.dll',
    'chrome.dll.pdb',
    'chrome.exe',
    'chrome_child.dll',
    'chrome_child.dll.pdb',
    'chrome_elf.dll',
    'chrome_watcher.dll',
    'libEGL.dll',
    'libGLESv2.dll',
    'mini_installer.exe',
    'resources.pak',
    'setup.exe',
    'swiftshader\\libEGL.dll',
    'swiftshader\\libGLESv2.dll',
    'WidevineCdm\\_platform_specific\\win_x64\\widevinecdm.dll',
    'WidevineCdm\\_platform_specific\\win_x64\\widevinecdmadapter.dll',
    'WidevineCdm\\_platform_specific\\win_x86\\widevinecdm.dll',
    'WidevineCdm\\_platform_specific\\win_x86\\widevinecdmadapter.dll',
  ]

  build_dir = build_directory.GetBuildOutputDirectory(SRC_DIR)
  target_dir = os.path.join(build_dir, options.target)

  for f in files:
    p = os.path.join(target_dir, f)
    if os.path.isfile(p):
      results_collector.add_result(f, f, get_size(p), 'bytes')

  return 0


def main():
  if sys.platform in ('win32', 'cygwin'):
    default_platform = 'win'
  elif sys.platform.startswith('darwin'):
    default_platform = 'mac'
  elif sys.platform == 'linux2':
    default_platform = 'linux'
  else:
    default_platform = None

  main_map = {
    'android' : main_android,
    'android-webview' : main_android_webview,
    'android-cronet' : main_android_cronet,
    'linux' : main_linux,
    'mac' : main_mac,
    'win' : main_win,
  }
  platforms = sorted(main_map.keys())

  option_parser = optparse.OptionParser()
  option_parser.add_option('--target',
                           default='Release',
                           help='build target (Debug, Release) '
                                '[default: %default]')
  option_parser.add_option('--target-dir', help='ignored')
  option_parser.add_option('--build-dir', help='ignored')
  option_parser.add_option('--platform',
                           default=default_platform,
                           help='specify platform (%s) [default: %%default]'
                                % ', '.join(platforms))
  option_parser.add_option('--json', help='Path to JSON output file')
  option_parser.add_option('--failures',
                           help='Path to JSON output file for failures')

  options, args = option_parser.parse_args()

  real_main = main_map.get(options.platform)
  if not real_main:
    if options.platform is None:
      sys.stderr.write('Unsupported sys.platform %s.\n' % repr(sys.platform))
    else:
      sys.stderr.write('Unknown platform %s.\n' % repr(options.platform))
    msg = 'Use the --platform= option to specify a supported platform:\n'
    sys.stderr.write(msg + '    ' + ' '.join(platforms) + '\n')
    return 2

  results_collector = ResultsCollector()
  rc = real_main(options, args, results_collector)

  if options.json:
    with open(options.json, 'w') as f:
      json.dump(results_collector.results, f)

  if options.failures:
    with open(options.failures, 'w') as f:
      json.dump(results_collector.failures, f)

  return rc


if '__main__' == __name__:
  sys.exit(main())

#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
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

import build_directory


SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
print SRC_DIR

# Add Catapult to the path so we can import the chartjson-histogramset
# conversion.
sys.path.append(os.path.join(SRC_DIR, 'third_party', 'catapult', 'tracing'))
from tracing.value import convert_chart_json

class ResultsCollector(object):
  def __init__(self):
    self.results = {}

  def add_result(self, name, identifier, value, units):
    assert name not in self.results
    self.results[name] = {
      'identifier': identifier,
      'value': int(value),
      'units': units
    }

    # Legacy printing, previously used for parsing the text logs.
    print 'RESULT %s: %s= %s %s' % (name, identifier, value, units)


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


def main_mac(options, args, results_collector):
  """Print appropriate size information about built Mac targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  build_dir = build_directory.GetBuildOutputDirectory(SRC_DIR)
  target_dir = os.path.join(build_dir, options.target)

  size_path = 'size'

  # If there's a hermetic download of Xcode, directly invoke 'size' from it. The
  # hermetic xcode binaries aren't a full Xcode install, so we can't modify
  # DEVELOPER_DIR.
  hermetic_size_path = os.path.join(
      SRC_DIR, 'build', 'mac_files', 'xcode_binaries', 'Contents',
      'Developer', 'Toolchains', 'XcodeDefault.xctoolchain', 'usr', 'bin',
      'size')
  if os.path.exists(hermetic_size_path):
    size_path = hermetic_size_path

  result = 0
  # Work with either build type.
  base_names = ('Chromium', 'Google Chrome')
  for base_name in base_names:
    app_bundle = base_name + '.app'
    framework_name = base_name + ' Framework'
    framework_bundle = framework_name + '.framework'
    framework_dsym_bundle = framework_name + '.dSYM'
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
        'framework_size'   : get_size(chromium_framework_executable),
        'framework_dsym_name' : re.sub(r'\s', '', framework_name) + 'Dsym',
        'framework_dsym_size' : get_size(chromium_framework_dsym),
      }

      # Collect the segment info out of the App
      result, stdout = run_process(result, [size_path, chromium_executable])
      print_dict['app_text'], print_dict['app_data'], print_dict['app_objc'] = \
          re.search(r'(\d+)\s+(\d+)\s+(\d+)', stdout).groups()

      # Collect the segment info out of the Framework
      result, stdout = run_process(result, [size_path,
                                            chromium_framework_executable])
      print_dict['framework_text'], print_dict['framework_data'], \
        print_dict['framework_objc'] = \
          re.search(r'(\d+)\s+(\d+)\s+(\d+)', stdout).groups()

      # Collect the whole size of the App bundle on disk (include the framework)
      result, stdout = run_process(result, ['du', '-s', '-k', chromium_app_dir])
      du_s = re.search(r'(\d+)', stdout).group(1)
      print_dict['app_bundle_size'] = (int(du_s) * 1024)

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
          print_dict['framework_dsym_name'], print_dict['framework_dsym_name'],
          print_dict['framework_dsym_size'], 'bytes')

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
    'WidevineCdm\\_platform_specific\\win_arm64\\widevinecdm.dll',
    'WidevineCdm\\_platform_specific\\win_x64\\widevinecdm.dll',
    'WidevineCdm\\_platform_specific\\win_x86\\widevinecdm.dll',
  ]

  build_dir = build_directory.GetBuildOutputDirectory(SRC_DIR)
  target_dir = os.path.join(build_dir, options.target)

  for f in files:
    p = os.path.join(target_dir, f)
    if os.path.isfile(p):
      results_collector.add_result(f, f, get_size(p), 'bytes')

  return 0


def format_for_histograms_conversion(data):
  # We need to do two things to the provided data to make it compatible with the
  # conversion script:
  # 1. Add a top-level "benchmark_name" key.
  # 2. Pull out the "identifier" value to be the story name.
  formatted_data = {}
  for metric, metric_data in data.iteritems():
    story = metric_data['identifier']
    formatted_data[metric] = {
      story: metric_data.copy()
    }
    del formatted_data[metric][story]['identifier']
  return {
    'benchmark_name': 'sizes',
    'charts': formatted_data
  }


def main():
  if sys.platform in ('win32', 'cygwin'):
    default_platform = 'win'
  elif sys.platform.startswith('darwin'):
    default_platform = 'mac'
  elif sys.platform.startswith('linux'):
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
  # This needs to be --output-dir (and not something like --output-directory) in
  # order to work properly with the build-side runtest.py script that's
  # currently used for dashboard uploading results from this script.
  option_parser.add_option('--output-dir',
                           help='Directory to dump data in the HistogramSet '
                                'format')

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

  if options.output_dir:
    histogram_path = os.path.join(options.output_dir, 'perf_results.json')
    # We need to add a bit more data to the results and rearrange some things,
    # otherwise the conversion fails due to the provided data being malformed.
    updated_results = format_for_histograms_conversion(
        results_collector.results)
    with open(histogram_path, 'w') as f:
      json.dump(updated_results, f)
    histogram_result = convert_chart_json.ConvertChartJson(histogram_path)
    if histogram_result.returncode != 0:
      sys.stderr.write(
          'chartjson conversion failed: %s\n' % histogram_result.stdout)
      return histogram_result.returncode
    with open(histogram_path, 'w') as f:
      f.write(histogram_result.stdout)

  return rc


if '__main__' == __name__:
  sys.exit(main())

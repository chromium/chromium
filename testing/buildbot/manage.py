#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Toolbox to manage all the json files in this directory.

It can reformat them in their canonical format or ensures they are well
formatted.
"""

import argparse
import ast
import collections
import glob
import json
import os
import six
import subprocess
import sys


THIS_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(THIS_DIR))
BLINK_DIR = os.path.join(SRC_DIR, 'third_party', 'WebKit')
sys.path.insert(0, os.path.join(SRC_DIR, 'third_party', 'colorama', 'src'))

import colorama


SKIP = {
  # These are not 'builders'.
  'compile_targets', 'gtest_tests', 'filter_compile_builders',
  'non_filter_builders', 'non_filter_tests_builders',

  # These are not supported on Swarming yet.

  # Android Cloud is still experimental and involves spinning up an Android
  # instance on GCE.  Swarming doesn't work in that environment yet.
  'Android Cloud Tests',

  # Android bots need custom dimension_sets entries for swarming, and capacity
  # is not there yet -- so don't let manage.py add swarming automatically there.
  'Android User Builder Tests',
  'Android GN',

  # http://crbug.com/441429
  'Linux Trusty (32)', 'Linux Trusty (dbg)(32)',

  # Swarming may not work on Mac10.10,11,12; need to
  # re-investigate and confirm.
  'WebKit Mac10.10',
  'WebKit Mac10.11',
  'WebKit Mac10.12',
  'WebKit Mac10.11 (dbg)',
  'Chromium Mac10.10 Tests',
  'Chromium Mac10.11 Tests',

  # One off builders. Note that Swarming does support ARM.
  'Linux ARM Cross-Compile',
  'Site Isolation Android',
  'Site Isolation Linux',
  'Site Isolation Win',
}


SKIP_GN_ISOLATE_MAP_TARGETS = {
    # This target is magic and not present in gn_isolate_map.pyl.
    'all',
    'remoting/client:client',
    'remoting/host:host',

    # These targets are listed only in build-side recipes.
    'All_syzygy',
    'blink_tests',
    'captured_sites_interactive_tests',
    'cast_shell',
    'cast_shell_apk',
    'chrome_official_builder',
    'chrome_official_builder_no_unittests',
    'chrome_sandbox',
    'chromium_builder_asan',
    'chromium_builder_perf',
    'chromiumos_preflight',
    'linux_symbols',
    'mini_installer',
    'previous_version_mini_installer',
    'symupload',

    # iOS tests are listed in //ios/build/bots.
    'cronet_test',
    'cronet_unittests_ios',
    'ios_chrome_bookmarks_eg2tests_module',
    'ios_chrome_bookmarks_egtests',
    'ios_chrome_integration_eg2tests_module',
    'ios_chrome_integration_egtests',
    'ios_chrome_reading_list_egtests',
    'ios_chrome_settings_eg2tests_module',
    'ios_chrome_settings_egtests',
    'ios_chrome_signin_eg2tests_module',
    'ios_chrome_signin_egtests',
    'ios_chrome_smoke_eg2tests_module',
    'ios_chrome_smoke_egtests',
    'ios_chrome_translate_egtests',
    'ios_chrome_ui_eg2tests_module',
    'ios_chrome_ui_egtests',
    'ios_chrome_unittests',
    'ios_chrome_web_eg2tests_module',
    'ios_chrome_web_egtests',
    'ios_components_unittests',
    'ios_crash_xcuitests_module',
    'ios_net_unittests',
    'ios_remoting_unittests',
    'ios_showcase_eg2tests_module',
    'ios_showcase_egtests',
    'ios_testing_unittests',
    'ios_web_inttests',
    'ios_web_shell_eg2tests_module',
    'ios_web_shell_egtests',
    'ios_web_unittests',
    'ios_web_view_inttests',
    'ios_web_view_unittests',

    # These are listed in Builders that are skipped for other reasons.
    'chrome_junit_tests',
    'components_background_task_scheduler_junit_tests',
    'components_embedder_support_junit_tests',
    'components_gcm_driver_junit_tests',
    'components_permissions_junit_tests',
    'components_policy_junit_tests',
    'components_variations_junit_tests',
    'content_junit_tests',
    'content_junit_tests',
    'device_junit_tests',
    'junit_unit_tests',
    'keyboard_accessory_junit_tests',
    'media_router_e2e_tests',
    'media_router_perf_tests',
    'net_junit_tests',
    'net_junit_tests',
    'password_check_junit_tests',
    'password_manager_junit_tests',
    'services_junit_tests',
    'system_webview_apk',
    'touch_to_fill_junit_tests',
    'traffic_annotation_auditor_dependencies',
    'ui_junit_tests',
    'vr_common_perftests',
    'vr_perf_tests',
    'vrcore_fps_test',
    'webapk_client_junit_tests',
    'webapk_shell_apk_h2o_junit_tests',
    'webapk_shell_apk_junit_tests',

    # These tests are only run on WebRTC CI.
    'AppRTCMobileTest',
    'android_examples_junit_tests',
    'android_sdk_junit_tests',
    'audio_decoder_unittests',
    'common_audio_unittests',
    'common_video_unittests',
    'dcsctp_unittests',
    'libjingle_peerconnection_android_unittest',
    'modules_tests',
    'modules_unittests',
    'peerconnection_unittests',
    'rtc_media_unittests',
    'rtc_pc_unittests',
    'rtc_stats_unittests',
    'rtc_unittests',
    'system_wrappers_unittests',
    'test_support_unittests',
    'tools_unittests',
    'video_engine_tests',
    'voice_engine_unittests',
    'voip_unittests',
    'webrtc_nonparallel_tests',
    'xmllite_xmpp_unittests',

    # These are only run on V8 CI.
    'pdfium_test',
    'postmortem-metadata',

    # These are only for developer convenience and not on any bots.
    'telemetry_gpu_integration_test_scripts_only',

    # These are defined by an android internal gn_isolate_map.pyl file.
    'resource_sizes_chrome_modern_minimal_apks',
    'resource_sizes_monochrome_minimal_apks',
    'resource_sizes_trichrome_google',
    'resource_sizes_system_webview_google_bundle',

    # These are only used by perf bots.
    'chrome_apk',
    'system_webview_google_apk',

    # These are used by https://www.chromium.org/developers/cluster-telemetry.
    'ct_telemetry_perf_tests_without_chrome',
}


class Error(Exception):
  """Processing error."""


def get_isolates():
  """Returns the list of all isolate files."""

  def git_ls_files(cwd):
    return subprocess.check_output(['git', 'ls-files'], cwd=cwd).splitlines()

  files = git_ls_files(SRC_DIR) + git_ls_files(BLINK_DIR)
  return [os.path.basename(f) for f in files if f.endswith('.isolate')]


def process_builder_convert(data, test_name):
  """Converts 'test_name' to run on Swarming in 'data'.

  Returns True if 'test_name' was found.
  """
  result = False
  for test in data['gtest_tests']:
    if test['test'] != test_name:
      continue
    test.setdefault('swarming', {})
    if not test['swarming'].get('can_use_on_swarming_builders'):
      test['swarming']['can_use_on_swarming_builders'] = True
    result = True
  return result


def process_builder_remaining(data, filename, builder, tests_location):
  """Calculates tests_location when mode is --remaining."""
  for test in data['gtest_tests']:
    name = test['test']
    if test.get('swarming', {}).get('can_use_on_swarming_builders'):
      tests_location[name]['count_run_on_swarming'] += 1
    else:
      tests_location[name]['count_run_local'] += 1
      tests_location[name]['local_configs'].setdefault(
          filename, []).append(builder)


def process_file(mode, test_name, tests_location, filepath, ninja_targets,
                 ninja_targets_seen):
  """Processes a json file describing what tests should be run for each recipe.

  The action depends on mode. Updates tests_location.

  Return False if the process exit code should be 1.
  """
  filename = os.path.basename(filepath)
  with open(filepath) as f:
    content = f.read()
  try:
    config = json.loads(content)
  except ValueError as e:
    six.raise_from(
        Error('Exception raised while checking %s: %s' % (filepath, e)), e)

  for builder, data in sorted(config.items()):
    if builder in SKIP:
      # Oddities.
      continue
    if not isinstance(data, dict):
      raise Error('%s: %s is broken: %s' % (filename, builder, data))
    if ('gtest_tests' not in data and
        'isolated_scripts' not in data and
        'additional_compile_targets' not in data and
        'instrumentation_tests' not in data):
      continue

    for d in data.get('junit_tests', []):
      test = d['test']
      if (test not in ninja_targets and
          test not in SKIP_GN_ISOLATE_MAP_TARGETS):
        raise Error('%s: %s / %s is not listed in gn_isolate_map.pyl' %
                    (filename, builder, test))
      if test in ninja_targets:
        ninja_targets_seen.add(test)

    for target in data.get('additional_compile_targets', []):
      if (target not in ninja_targets and
          target not in SKIP_GN_ISOLATE_MAP_TARGETS):
        raise Error('%s: %s / %s is not listed in gn_isolate_map.pyl' %
                    (filename, builder, target))
      if target in ninja_targets:
        ninja_targets_seen.add(target)

    gtest_tests = data.get('gtest_tests', [])
    if not isinstance(gtest_tests, list):
      raise Error(
          '%s: %s is broken: %s' % (filename, builder, gtest_tests))
    if not all(isinstance(g, dict) for g in gtest_tests):
      raise Error(
          '%s: %s is broken: %s' % (filename, builder, gtest_tests))

    seen = set()
    for d in gtest_tests:
      test = d['test']
      if (test not in ninja_targets and
          test not in SKIP_GN_ISOLATE_MAP_TARGETS):
        raise Error('%s: %s / %s is not listed in gn_isolate_map.pyl.' %
                    (filename, builder, test))
      if test in ninja_targets:
        ninja_targets_seen.add(test)

      name = d.get('name', d['test'])
      if name in seen:
        raise Error('%s: %s / %s is listed multiple times.' %
                    (filename, builder, name))
      seen.add(name)
      d.setdefault('swarming', {}).setdefault(
          'can_use_on_swarming_builders', False)

    if gtest_tests:
      config[builder]['gtest_tests'] = sorted(
          gtest_tests, key=lambda x: x['test'])

    for d in data.get('isolated_scripts', []):
      name = d['isolate_name']
      if (name not in ninja_targets and
          name not in SKIP_GN_ISOLATE_MAP_TARGETS):
        raise Error('%s: %s / %s is not listed in gn_isolate_map.pyl.' %
                    (filename, builder, name))
      if name in ninja_targets:
        ninja_targets_seen.add(name)

    for d in data.get('instrumentation_tests', []):
      name = d['test']
      if (name not in ninja_targets and
          name not in SKIP_GN_ISOLATE_MAP_TARGETS):
        raise Error('%s: %s / %s is not listed in gn_isolate_map.pyl.' %
                    (filename, builder, name))
      if name in ninja_targets:
        ninja_targets_seen.add(name)

    # The trick here is that process_builder_remaining() is called before
    # process_builder_convert() so tests_location can be used to know how many
    # tests were converted.
    if mode in ('convert', 'remaining'):
      process_builder_remaining(data, filename, builder, tests_location)
    if mode == 'convert':
      process_builder_convert(data, test_name)

  expected = json.dumps(
      config, sort_keys=True, indent=2, separators=(',', ': ')) + '\n'
  if content != expected:
    if mode in ('convert', 'write'):
      with open(filepath, 'wb') as f:
        f.write(expected)
      if mode == 'write':
        print('Updated %s' % filename)
    else:
      print('%s is not in canonical format' % filename)
      print('run `testing/buildbot/manage.py -w` to fix')
    return mode != 'check'
  return True


def print_convert(test_name, tests_location):
  """Prints statistics for a test being converted for use in a CL description.
  """
  data = tests_location[test_name]
  print('Convert %s to run exclusively on Swarming' % test_name)
  print('')
  print('%d configs already ran on Swarming' % data['count_run_on_swarming'])
  print('%d used to run locally and were converted:' % data['count_run_local'])
  for builder_group, builders in sorted(data['local_configs'].items()):
    for builder in builders:
      print('- %s: %s' % (builder_group, builder))
  print('')
  print('Ran:')
  print('  ./manage.py --convert %s' % test_name)
  print('')
  print('R=')
  print('BUG=98637')


def print_remaining(test_name, tests_location):
  """Prints a visual summary of what tests are yet to be converted to run on
  Swarming.
  """
  if test_name:
    if test_name not in tests_location:
      raise Error('Unknown test %s' % test_name)
    for config, builders in sorted(
        tests_location[test_name]['local_configs'].items()):
      print('%s:' % config)
      for builder in sorted(builders):
        print('  %s' % builder)
    return

  isolates = get_isolates()
  l = max(map(len, tests_location))
  print('%-*s%sLocal       %sSwarming  %sMissing isolate' %
      (l, 'Test', colorama.Fore.RED, colorama.Fore.GREEN,
        colorama.Fore.MAGENTA))
  total_local = 0
  total_swarming = 0
  for name, location in sorted(tests_location.items()):
    if not location['count_run_on_swarming']:
      c = colorama.Fore.RED
    elif location['count_run_local']:
      c = colorama.Fore.YELLOW
    else:
      c = colorama.Fore.GREEN
    total_local += location['count_run_local']
    total_swarming += location['count_run_on_swarming']
    missing_isolate = ''
    if name + '.isolate' not in isolates:
      missing_isolate = colorama.Fore.MAGENTA + '*'
    print('%s%-*s %4d           %4d    %s' %
        (c, l, name, location['count_run_local'],
          location['count_run_on_swarming'], missing_isolate))

  total = total_local + total_swarming
  p_local = 100. * total_local / total
  p_swarming = 100. * total_swarming / total
  # pylint: disable=bad-string-format-type
  print('%s%-*s %4d (%4.1f%%)   %4d (%4.1f%%)' %
      (colorama.Fore.WHITE, l, 'Total:', total_local, p_local,
        total_swarming, p_swarming))
  print('%-*s                %4d' % (l, 'Total executions:', total))
  #pylint: enable=bad-string-format-type


def main():
  colorama.init()
  parser = argparse.ArgumentParser(description=sys.modules[__name__].__doc__)
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument(
      '-c', '--check', dest='mode', action='store_const', const='check',
      default='check', help='Only check the files')
  group.add_argument(
      '--convert', dest='mode', action='store_const', const='convert',
      help='Convert a test to run on Swarming everywhere')
  group.add_argument(
      '--remaining', dest='mode', action='store_const', const='remaining',
      help='Count the number of tests not yet running on Swarming')
  group.add_argument(
      '-w', '--write', dest='mode', action='store_const', const='write',
      help='Rewrite the files')
  parser.add_argument(
      'test_name', nargs='?',
      help='The test name to print which configs to update; only to be used '
           'with --remaining')
  args = parser.parse_args()

  if args.mode == 'convert':
    if not args.test_name:
      parser.error('A test name is required with --convert')
    if args.test_name + '.isolate' not in get_isolates():
      parser.error('Create %s.isolate first' % args.test_name)

  # Stats when running in --remaining mode;
  tests_location = collections.defaultdict(
      lambda: {
        'count_run_local': 0, 'count_run_on_swarming': 0, 'local_configs': {}
      })

  with open(os.path.join(THIS_DIR, "gn_isolate_map.pyl")) as fp:
    gn_isolate_map = ast.literal_eval(fp.read())
    ninja_targets = {k: v['label'] for k, v in gn_isolate_map.items()}

  try:
    result = 0
    ninja_targets_seen = set()
    for filepath in glob.glob(os.path.join(THIS_DIR, '*.json')):
      if not process_file(args.mode, args.test_name, tests_location, filepath,
                          ninja_targets, ninja_targets_seen):
        result = 1

    skip_targets = [k for k, v in gn_isolate_map.items() if
                    ('skip_usage_check' in v and v['skip_usage_check'])]
    extra_targets = (set(ninja_targets) - set(skip_targets) -
                     ninja_targets_seen - SKIP_GN_ISOLATE_MAP_TARGETS)
    if extra_targets:
      if len(extra_targets) > 1:
        extra_targets_str = ', '.join(extra_targets) + ' are'
      else:
        extra_targets_str = list(extra_targets)[0] + ' is'
      raise Error('%s listed in gn_isolate_map.pyl but not in any .json '
                  'files' % extra_targets_str)

    if args.mode == 'convert':
      print_convert(args.test_name, tests_location)
    elif args.mode == 'remaining':
      print_remaining(args.test_name, tests_location)
    return result
  except Error as e:
    sys.stderr.write('%s\n' % e)
    return 1


if __name__ == "__main__":
  sys.exit(main())

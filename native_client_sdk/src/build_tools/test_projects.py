#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys
import time

import build_projects
import build_version
import buildbot_common
import parse_dsc

from build_paths import OUT_DIR, SRC_DIR, SDK_SRC_DIR, SCRIPT_DIR

sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
import getos
platform = getos.GetPlatform()

import find_chrome
browser_path = find_chrome.FindChrome(SRC_DIR, ['Debug', 'Release'])

# Fall back to using CHROME_PATH (same as in common.mk)
if not browser_path:
  browser_path = os.environ.get('CHROME_PATH')


pepper_ver = str(int(build_version.ChromeMajorVersion()))
pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)

browser_tester_py = os.path.join(SRC_DIR, 'ppapi', 'native_client', 'tools',
    'browser_tester', 'browser_tester.py')


ALL_CONFIGS = ['Debug', 'Release']
ALL_TOOLCHAINS = [
  'newlib',
  'glibc',
  'pnacl',
  'win',
  'linux',
  'mac',
  'clang-newlib',
]

# Values you can filter by:
#   name: The name of the test. (e.g. "pi_generator")
#   config: See ALL_CONFIGS above.
#   toolchain: See ALL_TOOLCHAINS above.
#   platform: mac/win/linux.
#
# All keys must be matched, but any value that matches in a sequence is
# considered a match for that key. For example:
#
#   {'name': ('pi_generator', 'input_event'), 'toolchain': ('newlib', 'pnacl')}
#
# Will match 8 tests:
#   pi_generator.newlib_debug_test
#   pi_generator.newlib_release_test
#   input_event.newlib_debug_test
#   input_event.newlib_release_test
#   pi_generator.glibc_debug_test
#   pi_generator.glibc_release_test
#   input_event.glibc_debug_test
#   input_event.glibc_release_test
DISABLED_TESTS = [
    # TODO(binji): Disable 3D examples on linux/win/mac. See
    # http://crbug.com/262379.
    {'name': 'graphics_3d', 'platform': ('win', 'linux', 'mac')},
    {'name': 'video_decode', 'platform': ('win', 'linux', 'mac')},
    {'name': 'video_encode', 'platform': ('win', 'linux', 'mac')},
    # TODO(sbc): Disable pi_generator on linux/win/mac. See
    # http://crbug.com/475255.
    {'name': 'pi_generator', 'platform': ('win', 'linux', 'mac')},
    # media_stream_audio uses audio input devices which are not supported.
    {'name': 'media_stream_audio', 'platform': ('win', 'linux', 'mac')},
    # media_stream_video uses 3D and webcam which are not supported.
    {'name': 'media_stream_video', 'platform': ('win', 'linux', 'mac')},
    # TODO(binji): These tests timeout on the trybots because the NEXEs take
    # more than 40 seconds to load (!). See http://crbug.com/280753
    {'name': 'nacl_io_test', 'platform': 'win', 'toolchain': 'glibc'},
    # We don't test "getting_started/part1" because it would complicate the
    # example.
    # TODO(binji): figure out a way to inject the testing code without
    # modifying the example; maybe an extension?
    {'name': 'part1'},
]

def ValidateToolchains(toolchains):
  invalid_toolchains = set(toolchains) - set(ALL_TOOLCHAINS)
  if invalid_toolchains:
    buildbot_common.ErrorExit('Invalid toolchain(s): %s' % (
        ', '.join(invalid_toolchains)))


def GetServingDirForProject(desc):
  dest = desc['DEST']
  path = os.path.join(pepperdir, *dest.split('/'))
  return os.path.join(path, desc['NAME'])


def GetRepoServingDirForProject(desc):
  # This differs from GetServingDirForProject, because it returns the location
  # within the Chrome repository of the project, not the "pepperdir".
  return os.path.dirname(desc['FILEPATH'])


def GetExecutableDirForProject(desc, toolchain, config):
  return os.path.join(GetServingDirForProject(desc), toolchain, config)


def GetBrowserTesterCommand(desc, toolchain, config):
  if browser_path is None:
    buildbot_common.ErrorExit('Failed to find chrome browser using FindChrome.')

  args = [
    sys.executable,
    browser_tester_py,
    '--browser_path', browser_path,
    '--timeout', '30.0',  # seconds
    # Prevent the infobar that shows up when requesting filesystem quota.
    '--browser_flag', '--unlimited-storage',
    '--enable_sockets',
    # Prevent installing a new copy of PNaCl.
    '--browser_flag', '--disable-component-update',
  ]

  args.extend(['--serving_dir', GetServingDirForProject(desc)])
  # Fall back on the example directory in the Chromium repo, to find test.js.
  args.extend(['--serving_dir', GetRepoServingDirForProject(desc)])
  # If it is not found there, fall back on the dummy one (in this directory.)
  args.extend(['--serving_dir', SCRIPT_DIR])

  if toolchain == platform:
    exe_dir = GetExecutableDirForProject(desc, toolchain, config)
    ppapi_plugin = os.path.join(exe_dir, desc['NAME'])
    if platform == 'win':
      ppapi_plugin += '.dll'
    else:
      ppapi_plugin += '.so'
    args.extend(['--ppapi_plugin', ppapi_plugin])

    ppapi_plugin_mimetype = 'application/x-ppapi-%s' % config.lower()
    args.extend(['--ppapi_plugin_mimetype', ppapi_plugin_mimetype])

  if toolchain == 'pnacl':
    args.extend(['--browser_flag', '--enable-pnacl'])

  url = 'index.html'
  url += '?tc=%s&config=%s&test=true' % (toolchain, config)
  args.extend(['--url', url])
  return args


def GetBrowserTesterEnv():
  # browser_tester imports tools/valgrind/memcheck_analyze, which imports
  # tools/valgrind/common. Well, it tries to, anyway, but instead imports
  # common from PYTHONPATH first (which on the buildbots, is a
  # common/__init__.py file...).
  #
  # Clear the PYTHONPATH so it imports the correct file.
  env = dict(os.environ)
  env['PYTHONPATH'] = ''
  return env


def RunTestOnce(desc, toolchain, config):
  args = GetBrowserTesterCommand(desc, toolchain, config)
  env = GetBrowserTesterEnv()
  start_time = time.time()
  try:
    subprocess.check_call(args, env=env)
    result = True
  except subprocess.CalledProcessError:
    result = False
  elapsed = (time.time() - start_time) * 1000
  return result, elapsed


def RunTestNTimes(desc, toolchain, config, times):
  total_elapsed = 0
  for _ in xrange(times):
    result, elapsed = RunTestOnce(desc, toolchain, config)
    total_elapsed += elapsed
    if result:
      # Success, stop retrying.
      break
  return result, total_elapsed


def RunTestWithGtestOutput(desc, toolchain, config, retry_on_failure_times):
  test_name = GetTestName(desc, toolchain, config)
  WriteGtestHeader(test_name)
  result, elapsed = RunTestNTimes(desc, toolchain, config,
                                  retry_on_failure_times)
  WriteGtestFooter(result, test_name, elapsed)
  return result


def WriteGtestHeader(test_name):
  print '\n[ RUN      ] %s' % test_name
  sys.stdout.flush()
  sys.stderr.flush()


def WriteGtestFooter(success, test_name, elapsed):
  sys.stdout.flush()
  sys.stderr.flush()
  if success:
    message = '[       OK ]'
  else:
    message = '[  FAILED  ]'
  print '%s %s (%d ms)' % (message, test_name, elapsed)


def GetTestName(desc, toolchain, config):
  return '%s.%s_%s_test' % (desc['NAME'], toolchain, config.lower())


def IsTestDisabled(desc, toolchain, config):
  def AsList(value):
    if not isinstance(value, (list, tuple)):
      return [value]
    return value

  def TestMatchesDisabled(test_values, disabled_test):
    for key in test_values:
      if key in disabled_test:
        if test_values[key] not in AsList(disabled_test[key]):
          return False
    return True

  test_values = {
      'name': desc['NAME'],
      'toolchain': toolchain,
      'config': config,
      'platform': platform
  }

  for disabled_test in DISABLED_TESTS:
    if TestMatchesDisabled(test_values, disabled_test):
      return True
  return False


def WriteHorizontalBar():
  print '-' * 80


def WriteBanner(message):
  WriteHorizontalBar()
  print message
  WriteHorizontalBar()


def RunAllTestsInTree(tree, toolchains, configs, retry_on_failure_times):
  tests_run = 0
  total_tests = 0
  failed = []
  disabled = []

  for _, desc in parse_dsc.GenerateProjects(tree):
    desc_configs = desc.get('CONFIGS', ALL_CONFIGS)
    valid_toolchains = set(toolchains) & set(desc['TOOLS'])
    valid_configs = set(configs) & set(desc_configs)
    for toolchain in sorted(valid_toolchains):
      for config in sorted(valid_configs):
        test_name = GetTestName(desc, toolchain, config)
        total_tests += 1
        if IsTestDisabled(desc, toolchain, config):
          disabled.append(test_name)
          continue

        tests_run += 1
        success = RunTestWithGtestOutput(desc, toolchain, config,
                                         retry_on_failure_times)
        if not success:
          failed.append(test_name)

  if failed:
    WriteBanner('FAILED TESTS')
    for test in failed:
      print '  %s failed.' % test

  if disabled:
    WriteBanner('DISABLED TESTS')
    for test in disabled:
      print '  %s disabled.' % test

  WriteHorizontalBar()
  print 'Tests run: %d/%d (%d disabled).' % (
      tests_run, total_tests, len(disabled))
  print 'Tests succeeded: %d/%d.' % (tests_run - len(failed), tests_run)

  success = len(failed) != 0
  return success


def BuildAllTestsInTree(tree, toolchains, configs):
  for branch, desc in parse_dsc.GenerateProjects(tree):
    desc_configs = desc.get('CONFIGS', ALL_CONFIGS)
    valid_toolchains = set(toolchains) & set(desc['TOOLS'])
    valid_configs = set(configs) & set(desc_configs)
    for toolchain in sorted(valid_toolchains):
      for config in sorted(valid_configs):
        name = '%s/%s' % (branch, desc['NAME'])
        build_projects.BuildProjectsBranch(pepperdir, name, deps=False,
                                           clean=False, config=config,
                                           args=['TOOLCHAIN=%s' % toolchain])


def GetProjectTree(include):
  # Everything in src is a library, and cannot be run.
  exclude = {'DEST': 'src'}
  try:
    return parse_dsc.LoadProjectTree(SDK_SRC_DIR, include=include,
                                     exclude=exclude)
  except parse_dsc.ValidationError as e:
    buildbot_common.ErrorExit(str(e))


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-c', '--config',
      help='Choose configuration to run (Debug or Release).  Runs both '
           'by default', action='append')
  parser.add_argument('-x', '--experimental',
      help='Run experimental projects', action='store_true')
  parser.add_argument('-t', '--toolchain',
      help='Run using toolchain. Can be passed more than once.',
      action='append', default=[])
  parser.add_argument('-d', '--dest',
      help='Select which destinations (project types) are valid.',
      action='append')
  parser.add_argument('-b', '--build',
      help='Build each project before testing.', action='store_true')
  parser.add_argument('--retry-times',
      help='Number of types to retry on failure', type=int, default=1)
  parser.add_argument('projects', nargs='*')

  options = parser.parse_args(args)

  if not options.toolchain:
    options.toolchain = ['newlib', 'glibc', 'pnacl', 'host']

  if 'host' in options.toolchain:
    options.toolchain.remove('host')
    options.toolchain.append(platform)
    print 'Adding platform: ' + platform

  ValidateToolchains(options.toolchain)

  include = {}
  if options.toolchain:
    include['TOOLS'] = options.toolchain
    print 'Filter by toolchain: ' + str(options.toolchain)
  if not options.experimental:
    include['EXPERIMENTAL'] = False
  if options.dest:
    include['DEST'] = options.dest
    print 'Filter by type: ' + str(options.dest)
  if options.projects:
    include['NAME'] = options.projects
    print 'Filter by name: ' + str(options.projects)
  if not options.config:
    options.config = ALL_CONFIGS

  project_tree = GetProjectTree(include)
  if options.build:
    BuildAllTestsInTree(project_tree, options.toolchain, options.config)

  return RunAllTestsInTree(project_tree, options.toolchain, options.config,
                           options.retry_times)


if __name__ == '__main__':
  script_name = os.path.basename(sys.argv[0])
  try:
    sys.exit(main(sys.argv[1:]))
  except parse_dsc.ValidationError as e:
    buildbot_common.ErrorExit('%s: %s' % (script_name, e))
  except KeyboardInterrupt:
    buildbot_common.ErrorExit('%s: interrupted' % script_name)

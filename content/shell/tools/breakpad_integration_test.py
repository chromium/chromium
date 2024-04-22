#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Integration test for breakpad in content shell.

This test checks that content shell and breakpad are correctly hooked up, as
well as that the tools can symbolize a stack trace."""


from __future__ import print_function
from __future__ import absolute_import
import glob
import json
import optparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
from six.moves import range

TOP_SRC_DIR = os.path.join(os.path.dirname(__file__), '..', '..', '..')

try:
  sys.path.append(os.path.join(TOP_SRC_DIR, 'build', 'android'))
  import devil_chromium
  devil_chromium.Initialize()

  from pylib.constants import host_paths
  if host_paths.DEVIL_PATH not in sys.path:
    sys.path.append(host_paths.DEVIL_PATH)

  from devil.android import apk_helper
  from devil.android import device_utils
  from devil.android import flag_changer
  from devil.android.sdk import intent
except:
  pass


CONCURRENT_TASKS=4
BREAKPAD_TOOLS_DIR = os.path.join(
  TOP_SRC_DIR, 'components', 'crash', 'content', 'tools')
ANDROID_CRASH_DIR = '/data/data/org.chromium.content_shell_apk/cache'


def GetDevice():
  if hasattr(GetDevice, 'device'):
    return GetDevice.device

  devices = device_utils.DeviceUtils.HealthyDevices()
  assert len(devices) == 1
  GetDevice.device = devices[0]
  return GetDevice.device


def clear_android_dumps(options, device):
  try:
    print('# Deleting stale crash dumps')
    pending = os.path.join(ANDROID_CRASH_DIR, 'pending')
    files = device.RunShellCommand(['ls', pending], as_root=True)
    for f in files:
      if f.endswith('.dmp'):
        dump = os.path.join(pending, f)
        try:
          if options.verbose:
            print(' deleting %s' % dump)
          device.RunShellCommand(['rm', dump], check_return=True, as_root=True)
        except:
          print('Failed to delete %s' % dump)

  except:
    print('Failed to list dumps in android crash dir %s' % pending)


def get_android_dump(options, crash_dir, symbols_dir):
  global failure

  pending = os.path.join(ANDROID_CRASH_DIR, 'pending')
  device = GetDevice()

  for attempts in range(5):
    files = device.RunShellCommand(['ls', pending], as_root=True)

    dumps = [f for f in files if f.endswith('.dmp')]
    if len(dumps) > 0:
      break
    # Crashpad may still be writing the dump. Sleep and try again.
    time.sleep(5)

  if len(dumps) != 1:
    # TODO(crbug.com/41400491): Temporary code to debug unexpected crash dumps.
    minidump_stackwalk = os.path.join(options.build_dir, 'minidump_stackwalk')
    failure = 'Failed to run minidump_stackwalk.'
    for dump in dumps:
      device.PullFile(os.path.join(pending, dump), crash_dir, as_root=True)
      minidump = os.path.join(crash_dir, os.path.basename(dump))
      cmd = [minidump_stackwalk, minidump, symbols_dir]
      proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)
      stack = proc.communicate()[0].decode('utf-8')
      print(stack)

      device.RunShellCommand(['rm', os.path.join(pending, dump)],
                             check_return=True, as_root=True)

    failure = 'Expected 1 crash dump, found %d.' % len(dumps)
    print(dumps)
    raise Exception(failure)

  device.PullFile(os.path.join(pending, dumps[0]), crash_dir, as_root=True)
  device.RunShellCommand(['rm', os.path.join(pending, dumps[0])],
                         check_return=True, as_root=True)

  return os.path.join(crash_dir, os.path.basename(dumps[0]))

def run_test(options, crash_dir, symbols_dir, platform,
             additional_arguments = []):
  global failure

  print('# Run content_shell and make it crash.')
  if platform == 'android':
    device = GetDevice()

    failure = None
    clear_android_dumps(options, device)

    apk_path = os.path.join(options.build_dir, 'apks', 'ContentShell.apk')
    apk = apk_helper.ApkHelper(apk_path)
    view_activity = apk.GetViewActivityName()
    package_name = apk.GetPackageName()

    device.RunShellCommand(['am', 'set-debug-app', '--persistent',
                            package_name])

    changer = flag_changer.FlagChanger(device, 'content-shell-command-line')
    changer.ReplaceFlags(['--enable-crash-reporter',
                          '--crash-dumps-dir=%s' % ANDROID_CRASH_DIR])

    launch_intent = intent.Intent(action='android.intent.action.VIEW',
                                  activity=view_activity, data='chrome://crash',
                                  package=package_name)
    device.StartActivity(launch_intent)
  else:
    cmd = [options.binary,
           '--run-web-tests',
           'chrome://crash',
           '--enable-crash-reporter',
           '--crash-dumps-dir=%s' % crash_dir]
    cmd += additional_arguments

    if options.verbose:
      print(' '.join(cmd))
    failure = 'Failed to run content_shell.'
    if options.verbose:
      subprocess.check_call(cmd)
    else:
      # On Windows, using os.devnull can cause check_call to never return,
      # so use a temporary file for the output.
      with tempfile.TemporaryFile() as tmpfile:
        subprocess.check_call(cmd, stdout=tmpfile, stderr=tmpfile)

  print('# Retrieve crash dump.')
  if platform == 'android':
    minidump = get_android_dump(options, crash_dir, symbols_dir)
  else:
    dmp_dir = crash_dir
    # TODO(crbug.com/41354248): This test should not reach directly into the
    # Crashpad database, but instead should use crashpad_database_util.
    if platform == 'darwin' or platform.startswith('linux'):
      dmp_dir = os.path.join(dmp_dir, 'pending')
    elif platform == 'win32':
      dmp_dir = os.path.join(dmp_dir, 'reports')

    dmp_files = glob.glob(os.path.join(dmp_dir, '*.dmp'))
    failure = 'Expected 1 crash dump, found %d.' % len(dmp_files)
    if len(dmp_files) != 1:
      raise Exception(failure)
    minidump = dmp_files[0]

  print('# Symbolize crash dump.')
  if platform == 'win32':
    cdb_exe = os.path.join(options.build_dir, 'cdb', 'cdb.exe')
    cmd = [cdb_exe, '-y', options.build_dir, '-c', '.lines;.excr;k30;q',
           '-z', minidump]
    if options.verbose:
      print(' '.join(cmd))
    failure = 'Failed to run cdb.exe.'
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    stack = proc.communicate()[0].decode('utf-8')
  else:
    minidump_stackwalk = os.path.join(options.build_dir, 'minidump_stackwalk')
    cmd = [minidump_stackwalk, minidump, symbols_dir]
    if options.verbose:
      print(' '.join(cmd))
    failure = 'Failed to run minidump_stackwalk.'
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    stack = proc.communicate()[0].decode('utf-8')

  # Check whether the stack contains a CrashIntentionally symbol.
  found_symbol = 'CrashIntentionally' in stack

  os.remove(minidump)

  if options.no_symbols:
    if found_symbol:
      if options.verbose:
        print(stack)
      failure = 'Found unexpected reference to CrashIntentionally in stack'
      raise Exception(failure)
  else:
    if not found_symbol:
      if options.verbose:
        print(stack)
      failure = 'Could not find reference to CrashIntentionally in stack.'
      raise Exception(failure)

def main():
  global failure

  parser = optparse.OptionParser()
  parser.add_option('', '--build-dir', default='',
                    help='The build output directory.')
  parser.add_option('', '--binary', default='',
                    help='The path of the binary to generate symbols for and '
                         'then run for the test.')
  parser.add_option('', '--additional-binary', default='',
                    help='An additional binary for which to generate symbols. '
                         'On Mac this is used for specifying the '
                         '"Content Shell Framework" library, which is not '
                         'linked into --binary.')
  parser.add_option('', '--no-symbols', default=False, action='store_true',
                    help='Symbols are not expected to work.')
  parser.add_option('-j', '--jobs', default=CONCURRENT_TASKS, action='store',
                    type='int', help='Number of parallel tasks to run.')
  parser.add_option('-v', '--verbose', action='store_true',
                    help='Print verbose status output.')
  parser.add_option('', '--json', default='',
                    help='Path to JSON output.')
  parser.add_option('', '--platform', default=sys.platform,
                    help='Platform to run the test on.')

  (options, _) = parser.parse_args()

  if not options.build_dir:
    print('Required option --build-dir missing.')
    return 1

  if not options.binary:
    print('Required option --binary missing.')
    return 1

  if not os.access(options.binary, os.X_OK):
    print('Cannot find %s.' % options.binary)
    return 1

  failure = ''

  # Create a temporary directory to store the crash dumps and symbols in.
  crash_dir = tempfile.mkdtemp()
  symbols_dir = os.path.join(crash_dir, 'symbols')

  crash_service = None

  try:
    if options.platform == 'android':
      device = GetDevice()

      print('# Install ContentShell.apk')
      apk_path = os.path.join(options.build_dir, 'apks', 'ContentShell.apk')
      device.Install(apk_path, reinstall=False, allow_downgrade=True)

    if options.platform != 'win32':
      print('# Generate symbols.')
      bins = [options.binary]
      if options.additional_binary:
        bins.append(options.additional_binary)
      generate_symbols = os.path.join(
          BREAKPAD_TOOLS_DIR, 'generate_breakpad_symbols.py')
      for binary in bins:
        cmd = [generate_symbols,
               '--build-dir=%s' % options.build_dir,
               '--binary=%s' % binary,
               '--symbols-dir=%s' % symbols_dir,
               '--jobs=%d' % options.jobs,
               '--platform=%s' % options.platform]
        if options.verbose:
          cmd.append('--verbose')
          print(' '.join(cmd))
        failure = 'Failed to run generate_breakpad_symbols.py.'
        subprocess.check_call(cmd)

    run_test(options, crash_dir, symbols_dir, options.platform)

  except:
    if failure == '':
        failure = '%s: %s' % sys.exc_info()[:2]
    print('FAIL: %s' % failure)
    if options.json:
      with open(options.json, 'w') as json_file:
        json.dump([failure], json_file)

    return 1

  else:
    print('PASS: Breakpad integration test ran successfully.')
    if options.json:
      with open(options.json, 'w') as json_file:
        json.dump([], json_file)
    return 0

  finally:
    if crash_service:
      crash_service.terminate()
      crash_service.wait()
    try:
      shutil.rmtree(crash_dir)
    except:
      print('Failed to delete temp directory "%s".' % crash_dir)
    if options.platform == 'android':
      clear_android_dumps(options, GetDevice())


if '__main__' == __name__:
  sys.exit(main())

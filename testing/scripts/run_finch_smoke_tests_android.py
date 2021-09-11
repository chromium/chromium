#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import contextlib
import json
import logging
import os
import posixpath
import re
import shutil
import sys
import time

# get_compile_steps.py is still using python 2,
# so this is necessary
if sys.version_info.major == 3:
  from functools import lru_cache
else:
  def lru_cache(func):
    def decorator(*args):
      return func(args)
    return decorator

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
BUILD_ANDROID = os.path.join(SRC_DIR, 'build', 'android')
CATAPULT_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult')
PYUTILS = os.path.join(CATAPULT_DIR, 'common', 'py_utils')
TEST_SEED_PATH = os.path.join(SRC_DIR, 'testing', 'scripts',
                              'variations_smoke_test_data',
                              'webview_test_seed')

if PYUTILS not in sys.path:
  sys.path.append(PYUTILS)

if BUILD_ANDROID not in sys.path:
  sys.path.append(BUILD_ANDROID)

import common
import devil_chromium

from devil import devil_env
from devil.android import apk_helper
from devil.android import flag_changer
from devil.android import logcat_monitor
from devil.android.tools import script_common
from devil.android.tools import system_app
from devil.android.tools import webview_app
from devil.utils import logging_common

from pylib.local.emulator import avd
from py_utils.tempfile_ext import NamedTemporaryDirectory

from wpt_android_lib import add_emulator_args, get_device

_LOGCAT_FILTERS = [
  'chromium:v',
  'cr_*:v',
  'DEBUG:I',
  'StrictMode:D',
  'WebView*:v'
]

logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

@lru_cache
def get_package_name(apk_path):
  """Get package name from apk

  Args:
    apk_path: Path to apk

  Returns:
    Package name of apk
  """
  return apk_helper.GetPackageName(apk_path)


@contextlib.contextmanager
def install_apks(device, options):
  """Install apks for testing

  Args:
    device: Interface for device
    options: Command line options

  Returns:
    None
  """
  device.Uninstall(get_package_name(options.webview_shell_apk))
  device.Install(options.webview_shell_apk, reinstall=True)
  with webview_app.UseWebViewProvider(device,
                                      options.webview_provider_apk):
    yield


def install_seed(device, options):
  """Install finch seed for testing

  Args:
    device: Interface for device
    options: Command line options

  Returns:
    None
  """
  shell_pkg_name = get_package_name(options.webview_shell_apk)
  app_data_dir = posixpath.join(
      device.GetApplicationDataDirectory(shell_pkg_name), 'app_webview')
  device.RunShellCommand(['mkdir', '-p', app_data_dir], run_as=shell_pkg_name)

  seed_path = posixpath.join(app_data_dir, 'variations_seed')
  seed_new_path = posixpath.join(app_data_dir, 'variations_seed_new')
  seed_stamp = posixpath.join(app_data_dir, 'variations_stamp')

  device.adb.Push(options.finch_seed_path, seed_path)
  device.adb.Push(options.finch_seed_path, seed_new_path)
  device.RunShellCommand(
      ['touch', seed_stamp], check_return=True, run_as=shell_pkg_name)

  # We need to make the WebView shell package an owner of the seeds,
  # see crbug.com/1191169#c19
  user_id = device.GetUidForPackage(shell_pkg_name)
  logger.info('Setting owner of seed files to %r', user_id)
  device.RunShellCommand(['chown', user_id, seed_path], as_root=True)
  device.RunShellCommand(['chown', user_id, seed_new_path], as_root=True)


def run_tests(device, options, test_suffix, webview_flags):
  """Run browser test on test device

  Args:
    device: Interface for device
    options: Command line options
    test_suffix: Suffix for log output
    webview_flags: Flags for webview browser

  Returns:
    True if browser did not crash or False if the browser crashed
  """
  webview_flags.ReplaceFlags(['--webview-verbose-logging'])
  shell_pkg_name = get_package_name(options.webview_shell_apk)
  activity_name = (
      '%s/org.chromium.webview_shell.WebViewBrowserActivity' % shell_pkg_name)
  logger.info('Starting activity %s' % activity_name)

  device.RunShellCommand([
        'am',
        'start',
        '-n',
        activity_name,
        '-a',
        'VIEW',
        '-d',
        'www.google.com'])
  logger.info('Waiting 10 s')
  time.sleep(10)

  # Check browser process
  browser_runs = check_browser(device, options)
  if browser_runs:
    logger.info('Browser is running ' + test_suffix)
  else:
    logger.error('Browser is not running ' + test_suffix)

  device.ForceStop(shell_pkg_name)
  device.ForceStop(get_package_name(options.webview_provider_apk))
  return browser_runs


def check_browser(device, options):
  """Check processes for browser process

  Args:
    device: Interface for device
    options: command line options

  Returns:
    True if browser is running or False if it is not
  """
  zygotes = device.ListProcesses('zygote')
  zygote_pids = set(p.pid for p in zygotes)
  assert zygote_pids, 'No Android zygote found'
  processes = device.ListProcesses(get_package_name(options.webview_shell_apk))
  return [p for p in processes if p.ppid in zygote_pids]


def get_json_results(w_seed_res, wo_seed_res):
  """Get json results for test suite

  Args:
    w_seed_res: Test result with seed installed
    wo_seed_res: Test result with no seed installed

  Returns:
    JSON results dictionary
  """
  json_results = {'version': 3, 'interrupted': False}
  json_results['tests'] = {'webview_finch_smoke_tests': {}}
  json_results['tests']['webview_finch_smoke_tests']['test_wo_seed'] = (
      {'expected': 'PASS', 'actual': wo_seed_res})
  json_results['tests']['webview_finch_smoke_tests']['test_w_seed'] = (
      {'expected': 'PASS', 'actual': w_seed_res})

  json_results['num_failures_by_type'] = {}
  json_results['num_failures_by_type'].setdefault(w_seed_res, 0)
  json_results['num_failures_by_type'].setdefault(wo_seed_res, 0)
  json_results['num_failures_by_type'][w_seed_res] += 1
  json_results['num_failures_by_type'][wo_seed_res] += 1

  json_results['seconds_since_epoch'] = int(time.time())
  return json_results


def main(args):
  parser = argparse.ArgumentParser(
      prog='run_finch_smoke_tests_android.py')
  parser.add_argument('--finch-seed-path', default=TEST_SEED_PATH,
                      type=os.path.realpath,
                      help='Path to the finch seed')
  parser.add_argument('--webview-shell-apk',
                      help='Path to the WebView shell apk',
                      type=os.path.realpath,
                      required=True)
  parser.add_argument('--webview-provider-apk',
                      required=True,
                      type=os.path.realpath,
                      help='Path to the WebView provider apk')
  parser.add_argument('--write-full-results-to',
                      '--isolated-script-test-output',
                      action='store',
                      type=os.path.realpath,
                      default=os.path.join(os.getcwd(), 'output.json'),
                      help='Path to output directory')
  add_emulator_args(parser)
  script_common.AddDeviceArguments(parser)
  script_common.AddEnvironmentArguments(parser)
  logging_common.AddLoggingArguments(parser)

  options, _ = parser.parse_known_args(args)
  devil_chromium.Initialize(adb_path=options.adb_path)

  logging_common.InitializeLogging(options)

  with get_device(options) as device, install_apks(device, options):
    device.EnableRoot()
    log_mon = logcat_monitor.LogcatMonitor(
          device.adb,
          output_file=os.path.join(
              os.path.dirname(options.write_full_results_to),
              'webview_finch_logcat.txt'),
          filter_specs=_LOGCAT_FILTERS)
    log_mon.Start()

    webview_flags = flag_changer.FlagChanger(device, 'webview-command-line')
    device.RunShellCommand(
        ['pm', 'clear', get_package_name(options.webview_shell_apk)],
        check_return=True)

    tests_pass = False
    w_seed_res = 'FAIL'
    wo_seed_res = 'FAIL'
    if run_tests(device, options, 'without finch seed', webview_flags) != 0:
      install_seed(device, options)
      tests_pass = run_tests(device, options, 'with finch seed', webview_flags)
      wo_seed_res = 'PASS'
      if tests_pass:
        w_seed_res = 'PASS'

    log_mon.Stop()
    json_results = get_json_results(w_seed_res, wo_seed_res)
    with open(options.write_full_results_to, 'w') as json_out:
      json_out.write(json.dumps(json_results, indent=4))

  # Return zero exit code if tests pass
  return not tests_pass


def main_compile_targets(args):
  json.dump([], args.output)


if __name__ == '__main__':
  if 'compile_targets' in sys.argv:
    funcs = {
      'run': None,
      'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main(sys.argv[1:]))

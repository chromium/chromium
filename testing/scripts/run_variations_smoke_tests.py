#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A smoke test to verify Chrome doesn't crash and basic rendering is functional
when parsing a newly given variations seed.
"""

import argparse
import http
import json
import logging
import os
import shutil
import sys
import tempfile
from functools import partial
from http.server import SimpleHTTPRequestHandler
from threading import Thread

# vpython-provided modules.
import packaging.version  # pylint: disable=import-error

# //third_party/webdriver/pylib imports.
from selenium import webdriver
from selenium.webdriver import ChromeOptions
from selenium.webdriver.chrome.service import Service
from selenium.webdriver.common.by import By
from selenium.common.exceptions import NoSuchElementException
from selenium.common.exceptions import WebDriverException

# //testing/scripts imports.
import common
import variations_seed_access_helper as seed_helper
from skia_gold_infra import finch_skia_gold_utils

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_CHROMIUM_SRC_DIR = os.path.realpath(os.path.join(_THIS_DIR, '..', '..'))

sys.path.append(os.path.join(_CHROMIUM_SRC_DIR, 'build'))
# //build imports.
from skia_gold_common.skia_gold_properties import SkiaGoldProperties

_VARIATIONS_TEST_DATA = 'variations_smoke_test_data'
_VERSION_STRING = 'PRODUCT_VERSION'
_FLAG_RELEASE_VERSION = packaging.version.parse('105.0.5176.3')

# Constants for the waiting for seed from finch server
_MAX_ATTEMPTS = 2
_WAIT_TIMEOUT_IN_SEC = 0.5

# Test cases to verify web elements can be rendered correctly.
_TEST_CASES = [
    {
        # data:text/html,<h1 id="success">Success</h1>
        'url': 'data:text/html,%3Ch1%20id%3D%22success%22%3ESuccess%3C%2Fh1%3E',
        'expected_id': 'success',
        'expected_text': 'Success',
    },
    {
        'url': 'http://localhost:8000',
        'expected_id': 'sites-chrome-userheader-title',
        'expected_text': 'The Chromium Projects',
        'skia_gold_image': 'finch_smoke_render_chromium_org_html',
    },
]


def _get_httpd():
  """Returns a HTTPServer instance."""
  hostname = 'localhost'
  port = 8000
  directory = os.path.join(_THIS_DIR, _VARIATIONS_TEST_DATA, 'http_server')
  handler = partial(SimpleHTTPRequestHandler, directory=directory)
  httpd = http.server.HTTPServer((hostname, port), handler)
  httpd.timeout = 0.5
  httpd.allow_reuse_address = True
  return httpd


def _get_platform():
  """Returns the host platform.

  Returns:
    One of 'linux', 'win' and 'mac'.
  """
  if sys.platform == 'win32' or sys.platform == 'cygwin':
    return 'win'
  if sys.platform.startswith('linux'):
    return 'linux'
  if sys.platform == 'darwin':
    return 'mac'

  raise RuntimeError(
      'Unsupported platform: %s. Only Linux (linux*) and Mac (darwin) and '
      'Windows (win32 or cygwin) are supported' % sys.platform)


def _find_chrome_binary():  #pylint: disable=inconsistent-return-statements
  """Finds and returns the relative path to the Chrome binary.

  This function assumes that the CWD is the build directory.

  Returns:
    A relative path to the Chrome binary.
  """
  platform = _get_platform()
  if platform == 'linux':
    return os.path.join('.', 'chrome')
  if platform == 'mac':
    chrome_name = 'Google Chrome'
    return os.path.join('.', chrome_name + '.app', 'Contents', 'MacOS',
                        chrome_name)
  if platform == 'win':
    return os.path.join('.', 'chrome.exe')


def _run_tests(work_dir, skia_util, *args):
  """Runs the smoke tests.

  Args:
    work_dir: A working directory to store screenshots and other artifacts.
    skia_util: A FinchSkiaGoldUtil used to do pixel test.
    args: Arguments to be passed to the chrome binary.

  Returns:
    0 if tests passed, otherwise 1.
  """
  skia_gold_session = skia_util.SkiaGoldSession
  path_chrome = _find_chrome_binary()
  path_chromedriver = os.path.join('.', 'chromedriver')
  hardcoded_seed_path = os.path.join(
      _THIS_DIR, _VARIATIONS_TEST_DATA,
      'variations_seed_beta_%s.json' % _get_platform())
  path_seed = seed_helper.get_test_seed_file_path(hardcoded_seed_path)

  user_data_dir = tempfile.mkdtemp()
  crash_dump_dir = tempfile.mkdtemp()
  _, log_file = tempfile.mkstemp()

  # Crashpad is a separate process and its dump locations is set via env
  # variable.
  os.environ['BREAKPAD_DUMP_LOCATION'] = crash_dump_dir

  chrome_options = ChromeOptions()
  chrome_options.binary_location = path_chrome
  chrome_options.add_argument('user-data-dir=' + user_data_dir)
  chrome_options.add_argument('log-file=' + log_file)
  chrome_options.add_argument('variations-test-seed-path=' + path_seed)
  #TODO(crbug.com/40230862): Remove this line.
  chrome_options.add_argument('disable-field-trial-config')

  for arg in args:
    chrome_options.add_argument(arg)

  # By default, ChromeDriver passes in --disable-backgroud-networking, however,
  # fetching variations seeds requires network connection, so override it.
  chrome_options.add_experimental_option('excludeSwitches',
                                         ['disable-background-networking'])

  driver = None
  try:
    # Starts Chrome with the test seed injected.
    chromedriver_service = Service(executable_path=path_chromedriver)
    driver = webdriver.Chrome(service=chromedriver_service,
                              options=chrome_options)

    # Run test cases: visit urls and verify certain web elements are rendered
    # correctly.
    for t in _TEST_CASES:
      driver.get(t['url'])
      driver.set_window_size(1280, 1024)
      element = driver.find_element(By.ID, t['expected_id'])
      if not element.is_displayed() or t['expected_text'] != element.text:
        logging.error(
            'Test failed because element: "%s" is not visibly found after '
            'visiting url: "%s"', t['expected_text'], t['url'])
        return 1
      if 'skia_gold_image' in t:
        image_name = t['skia_gold_image']
        sc_file = os.path.join(work_dir, image_name + '.png')
        driver.find_element(By.ID, 'body').screenshot(sc_file)
        force_dryrun = False
        if skia_util.IsTryjobRun and skia_util.IsRetryWithoutPatch:
          force_dryrun = True
        status, error = skia_gold_session.RunComparison(
            name=image_name, png_file=sc_file, force_dryrun=force_dryrun)
        if status:
          finch_skia_gold_utils.log_skia_gold_status_code(
              skia_gold_session, image_name, status, error)
          return status

    driver.quit()

  except NoSuchElementException as e:
    logging.error('Failed to find the expected web element.\n%s', e)
    return 1
  except WebDriverException as e:
    if os.listdir(crash_dump_dir):
      logging.error('Chrome crashed and exited abnormally.\n%s', e)
    else:
      logging.error('Uncaught WebDriver exception thrown.\n%s', e)
    return 1
  finally:
    shutil.rmtree(user_data_dir, ignore_errors=True)
    shutil.rmtree(crash_dump_dir, ignore_errors=True)

    # Print logs for debugging purpose.
    with open(log_file) as f:
      logging.info('Chrome logs for debugging:\n%s', f.read())

    shutil.rmtree(log_file, ignore_errors=True)
    if driver:
      driver.quit()

  return 0


def _start_local_http_server():
  """Starts a local http server.

  Returns:
    A local http.server.HTTPServer.
  """
  httpd = _get_httpd()
  address = 'http://{}:{}'.format(httpd.server_name, httpd.server_port)
  logging.info('%s is used as local http server.', address)
  thread = Thread(target=httpd.serve_forever)
  thread.setDaemon(True)
  thread.start()
  return httpd


def main_run(args):
  """Runs the variations smoke tests."""
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output', type=str)
  parser.add_argument('--isolated-script-test-filter', type=str)
  SkiaGoldProperties.AddCommandLineArguments(parser)
  args, rest = parser.parse_known_args()

  temp_dir = tempfile.mkdtemp()
  httpd = _start_local_http_server()
  skia_util = finch_skia_gold_utils.FinchSkiaGoldUtil(temp_dir, args)
  try:
    rc = _run_tests(temp_dir, skia_util, *rest)
    if args.isolated_script_test_output:
      with open(args.isolated_script_test_output, 'w') as f:
        common.record_local_script_results('run_variations_smoke_tests', f, [],
                                           rc == 0)
  finally:
    httpd.shutdown()
    shutil.rmtree(temp_dir, ignore_errors=True)

  return rc


def main_compile_targets(args):
  """Returns the list of targets to compile in order to run this test."""
  json.dump(['chrome', 'chromedriver'], args.output)
  return 0


if __name__ == '__main__':
  if 'compile_targets' in sys.argv:
    funcs = {
        'run': None,
        'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main_run(sys.argv[1:]))

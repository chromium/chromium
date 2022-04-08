#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
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
import time
from functools import partial
from http.server import SimpleHTTPRequestHandler
from threading import Thread
from skia_gold_infra.finch_skia_gold_properties import FinchSkiaGoldProperties
from skia_gold_infra import finch_skia_gold_session_manager
from skia_gold_infra import finch_skia_gold_utils

import common
import variations_seed_access_helper as seed_helper

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_VARIATIONS_TEST_DATA = 'variations_smoke_test_data'

from selenium import webdriver
from selenium.webdriver import ChromeOptions
from selenium.common.exceptions import NoSuchElementException
from selenium.common.exceptions import WebDriverException

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
# This is the corpus used by skia gold to identify the data set.
# We are not using the same corpus as the rest of the skia gold chromium tests.
# This corpus is a dedicated one for finch smoke tests.
CORPUS = 'finch-smoke-tests'


def _get_httpd():
  """Returns a HTTPServer instance."""
  hostname = "localhost"
  port = 8000
  directory = os.path.join(_THIS_DIR, _VARIATIONS_TEST_DATA, "http_server")
  httpd = None
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


def _find_chrome_binary():
  """Finds and returns the relative path to the Chrome binary.

  This function assumes that the CWD is the build directory.

  Returns:
    A relative path to the Chrome binary.
  """
  platform = _get_platform()
  if platform == 'linux':
    return os.path.join('.', 'chrome')
  elif platform == 'mac':
    chrome_name = 'Google Chrome'
    return os.path.join('.', chrome_name + '.app', 'Contents', 'MacOS',
                            chrome_name)
  elif platform == 'win':
    return os.path.join('.', 'chrome.exe')


def _confirm_new_seed_downloaded(user_data_dir,
                                 path_chromedriver,
                                 chrome_options,
                                 old_seed=None,
                                 old_signature=None):
  """Confirms the new seed to be downloaded from finch server.

  Note that Local State does not dump until Chrome has exited.

  Args:
    user_data_dir: the use directory used to store fetched seed.
    path_chromedriver: the path of chromedriver binary.
    chrome_options: the chrome option used to launch Chrome.
    old_seed: the old seed serves as a baseline. New seed should be different.
    old_signature: the old signature serves as a baseline. New signature should
        be different.

  Returns:
    True if the new seed is downloaded, otherwise False.
  """
  driver = None
  attempt = 0
  wait_timeout_in_sec = _WAIT_TIMEOUT_IN_SEC
  while attempt < _MAX_ATTEMPTS:
    # Starts Chrome to allow it to download a seed or a seed delta.
    driver = webdriver.Chrome(path_chromedriver, chrome_options=chrome_options)
    time.sleep(5)
    # Exits Chrome so that Local State could be serialized to disk.
    driver.quit()
    # Checks the seed and signature.
    current_seed, current_signature = seed_helper.get_current_seed(
        user_data_dir)
    if current_seed != old_seed and current_signature != old_signature:
      return True
    attempt += 1
    time.sleep(wait_timeout_in_sec)
    wait_timeout_in_sec *= 2
  return False


def _get_skia_gold_session(session_manager):
  """Returns a SkiaGoldSession from the given session_manager.

  Args:
    session_manager: A SkiaGoldSessionManager object.

  Returns:
    a SkiaGoldSession object.
  """
  key_input = {}
  key_input['platform'] = _get_platform()
  return session_manager.GetSkiaGoldSession(
      key_input, CORPUS)

def _run_tests(work_dir, session_manager, *args):
  """Runs the smoke tests.

  Args:
    work_dir: A working directory to store screenshots and other artifacts.
    session_manager: A SkiaGoldSessionManager used to do
      pixel test.
    args: Arguments to be passed to the chrome binary.

  Returns:
    0 if tests passed, otherwise 1.
  """
  skia_gold_session = _get_skia_gold_session(session_manager)
  path_chrome = _find_chrome_binary()
  path_chromedriver = os.path.join('.', 'chromedriver')

  user_data_dir = tempfile.mkdtemp()
  _, log_file = tempfile.mkstemp()

  chrome_options = ChromeOptions()
  chrome_options.binary_location = path_chrome
  chrome_options.add_argument('user-data-dir=' + user_data_dir)
  chrome_options.add_argument('log-file=' + log_file)
  for arg in args:
    chrome_options.add_argument(arg)

  # By default, ChromeDriver passes in --disable-backgroud-networking, however,
  # fetching variations seeds requires network connection, so override it.
  chrome_options.add_experimental_option('excludeSwitches',
                                         ['disable-background-networking'])

  driver = None
  try:
    # Verify a production version of variations seed was fetched successfully.
    if not _confirm_new_seed_downloaded(user_data_dir, path_chromedriver,
                                        chrome_options):
      logging.error('Failed to fetch variations seed on initial run')
      # For MacOS, there is sometime the test fail to download seed on initial
      # run (crbug/1312393)
      if _get_platform() != 'mac':
        return 1

    # Inject the test seed.
    # This is a path as fallback when |seed_helper.load_test_seed_from_file()|
    # can't find one under src root.
    hardcoded_seed_path = os.path.join(_THIS_DIR, _VARIATIONS_TEST_DATA,
                             'variations_seed_beta_%s.json' % _get_platform())
    seed, signature = seed_helper.load_test_seed_from_file(hardcoded_seed_path)
    if not seed or not signature:
      logging.error(
          'Ill-formed test seed json file: "%s" and "%s" are required',
          seed_helper.LOCAL_STATE_SEED_NAME,
          seed_helper.LOCAL_STATE_SEED_SIGNATURE_NAME)
      return 1

    if not seed_helper.inject_test_seed(seed, signature, user_data_dir):
      logging.error('Failed to inject the test seed')
      return 1

    # Starts Chrome again with the test seed injected.
    driver = webdriver.Chrome(path_chromedriver, chrome_options=chrome_options)

    # Run test cases: visit urls and verify certain web elements are rendered
    # correctly.
    # TODO(crbug.com/1234404): Investigate pixel/layout based testing instead of
    # DOM based testing to verify that rendering is working properly.
    for t in _TEST_CASES:
      driver.get(t['url'])
      element = driver.find_element_by_id(t['expected_id'])
      if not element.is_displayed() or t['expected_text'] != element.text:
        logging.error(
            'Test failed because element: "%s" is not visibly found after '
            'visiting url: "%s"', t['expected_text'], t['url'])
        return 1
      if 'skia_gold_image' in t:
        image_name = t['skia_gold_image']
        sc_file = os.path.join(work_dir, image_name + '.png')
        driver.save_screenshot(sc_file)
        status, error = skia_gold_session.RunComparison(
            image_name, sc_file)
        if status:
          finch_skia_gold_utils.log_skia_gold_status_code(
              skia_gold_session, image_name, status, error)
          return status

    driver.quit()
    # Verify seed has been updated successfully and it's different from the
    # injected test seed.
    #
    # TODO(crbug.com/1234171): This test expectation may not work correctly when
    # a field trial config under test does not affect a platform, so it requires
    # more investigations to figure out the correct behavior.
    if not _confirm_new_seed_downloaded(user_data_dir, path_chromedriver,
                                        chrome_options, seed, signature):
      logging.error('Failed to update seed with a delta')
      return 1

  except WebDriverException as e:
    logging.error('Chrome exited abnormally, likely due to a crash.\n%s', e)
    return 1
  except NoSuchElementException as e:
    logging.error('Failed to find the expected web element.\n%s', e)
    return 1
  finally:
    shutil.rmtree(user_data_dir, ignore_errors=True)

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
  thread = None
  address = "http://{}:{}".format(httpd.server_name, httpd.server_port)
  logging.info("%s is used as local http server.", address)
  thread = Thread(target=httpd.serve_forever)
  thread.setDaemon(True)
  thread.start()
  return httpd


def main_run(args):
  """Runs the variations smoke tests."""
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output', type=str)
  FinchSkiaGoldProperties.AddCommandLineArguments(parser)
  args, rest = parser.parse_known_args()

  temp_dir = tempfile.mkdtemp()
  httpd = _start_local_http_server()
  manager = finch_skia_gold_session_manager.FinchSkiaGoldSessionManager(
      temp_dir, FinchSkiaGoldProperties(args))
  try:
    rc = _run_tests(temp_dir, manager, *rest)
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

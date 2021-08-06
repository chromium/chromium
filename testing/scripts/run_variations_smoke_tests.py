#!/usr/bin/env vpython
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A smoke test to verify Chrome doesn't crash and basic rendering is functional
when parsing a newly given variations seed.
"""

import argparse
import json
import logging
import os
import shutil
import sys
import tempfile
import urllib2

import common

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_SRC_DIR = os.path.join(_THIS_DIR, os.path.pardir, os.path.pardir)
_WEBDRIVER_PATH = os.path.join(_SRC_DIR, 'third_party', 'webdriver', 'pylib')

sys.path.insert(0, _WEBDRIVER_PATH)
from selenium import webdriver
from selenium.webdriver import ChromeOptions
from selenium.common.exceptions import NoSuchElementException
from selenium.common.exceptions import WebDriverException

# Constants around the Local State file and variation keys.
_LOCAL_STATE_FILE_NAME = 'Local State'
_LOCAL_STATE_SEED_NAME = 'variations_compressed_seed'
_LOCAL_STATE_SEED_SIGNATURE_NAME = 'variations_seed_signature'

# Test cases to verify web elements can be rendered correctly.
_TEST_CASES = [
    {
        # data:text/html,<h1 id="success">Success</h1>
        'url': 'data:text/html,%3Ch1%20id%3D%22success%22%3ESuccess%3C%2Fh1%3E',
        'expected_id': 'success',
        'expected_text': 'Success',
    },
    {
        # TODO(crbug.com/1234165): Make tests hermetic by using a test http
        # server or WPR.
        'url': 'https://chromium.org/',
        'expected_id': 'sites-chrome-userheader-title',
        'expected_text': 'The Chromium Projects',
    },
]


def _parse_test_seed():
  """Reads and parses the test variations seed.

  For prototypeing propose, a test seed is hard-coded in the
  variations_smoke_test_data/ directory, and this function should be updated to
  use the seed provided by the official variations test recipe once it's ready
  for integration.

  Returns:
    A tuple of two strings: the compressed seed and the seed signature.
  """
  with open(
      os.path.join(_THIS_DIR, 'variations_smoke_test_data',
                   'variations_seed_beta_linux.json')) as f:
    seed_json = json.load(f)

  return (seed_json.get(_LOCAL_STATE_SEED_NAME, None),
          seed_json.get(_LOCAL_STATE_SEED_NAME, None))


def _get_current_seed(user_data_dir):
  """Gets the current seed.

  Args:
    user_data_dir (str): Path to the user data directory used to laucn Chrome.

  Returns:
    A tuple of two strings: the compressed seed and the seed signature.
  """
  with open(os.path.join(user_data_dir, _LOCAL_STATE_FILE_NAME)) as f:
    local_state = json.load(f)

  return local_state.get(_LOCAL_STATE_SEED_NAME, None), local_state.get(
      _LOCAL_STATE_SEED_SIGNATURE_NAME, None)


def _inject_test_seed(seed, signature, user_data_dir):
  """Injects the given test seed.

  Args:
    seed (str): A variations seed.
    signature (str): A seed signature.
    user_data_dir (str): Path to the user data directory used to laucn Chrome.
  """
  with open(os.path.join(user_data_dir, _LOCAL_STATE_FILE_NAME)) as f:
    local_state = json.load(f)

  local_state[_LOCAL_STATE_SEED_NAME] = seed
  local_state[_LOCAL_STATE_SEED_SIGNATURE_NAME] = signature

  with open(os.path.join(user_data_dir, _LOCAL_STATE_FILE_NAME), 'w') as f:
    json.dump(local_state, f)


def _run_tests():
  """Runs the smoke tests.

  Returns:
    0 if tests passed, otherwise 1.
  """
  path_chrome = os.path.join('.', 'chrome')
  path_chromedriver = os.path.join('.', 'chromedriver')

  user_data_dir = tempfile.mkdtemp()
  _, log_file = tempfile.mkstemp()

  chrome_options = ChromeOptions()
  chrome_options.binary_location = path_chrome
  chrome_options.add_argument('user-data-dir=' + user_data_dir)
  chrome_options.add_argument('log-file=' + log_file)

  # By default, ChromeDriver passes in --disable-backgroud-networking, however,
  # fetching variations seeds requires network connection, so override it.
  chrome_options.add_experimental_option('excludeSwitches',
                                         ['disable-background-networking'])

  driver = None
  try:
    # Starts Chrome without seed.
    driver = webdriver.Chrome(path_chromedriver, chrome_options=chrome_options)
    driver.quit()

    # Verify a production version of variations seed was fetched successfully.
    current_seed, current_signature = _get_current_seed(user_data_dir)
    if not current_seed or not current_signature:
      logging.error('Failed to fetch variations seed on initial run')
      return 1

    # Inject the test seed.
    seed, signature = _parse_test_seed()
    if not seed or not signature:
      logging.error(
          'Ill-formed test seed json file: "%s" and "%s" are required',
          _LOCAL_STATE_SEED_NAME, _LOCAL_STATE_SEED_SIGNATURE_NAME)
      return 1

    _inject_test_seed(seed, signature, user_data_dir)

    # Verify the seed has been injected successfully.
    current_seed, current_signature = _get_current_seed(user_data_dir)
    if current_seed != seed or current_signature != signature:
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

    driver.quit()

    # Starts Chrome again to allow it to download a seed delta and update the
    # seed with it.
    driver = webdriver.Chrome(path_chromedriver, chrome_options=chrome_options)
    driver.quit()

    # Verify seed has been updated successfully and it's different from the
    # injected test seed.
    #
    # TODO(crbug.com/1234171): This test expectation may not work correctly when
    # a field trial config under test does not affect a platform, so it requires
    # more investigations to figure out the correct behavior.
    current_seed, current_signature = _get_current_seed(user_data_dir)
    if current_seed == seed or current_signature == signature:
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
      try:
        driver.quit()
      except urllib2.URLError:
        # Ignore the error as ChromeDriver may have already exited.
        pass

  return 0


def main_run(args):
  """Runs the variations smoke tests."""
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output', type=str, required=True)
  args, _ = parser.parse_known_args()
  rc = _run_tests()
  with open(args.isolated_script_test_output, 'w') as f:
    common.record_local_script_results('run_variations_smoke_tests', f, [],
                                       rc == 0)

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

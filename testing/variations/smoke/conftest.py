# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pytest

import logging
import os
import test_utils
import shutil

from contextlib import contextmanager
from selenium import webdriver
from selenium.common.exceptions import WebDriverException
from selenium.webdriver import ChromeOptions

def pytest_addoption(parser: pytest.Parser):
  # By default, running on the hosted platform.
  parser.addoption('--target-platform',
                   default=test_utils.get_hosted_platform(),
                   dest='target_platform',
                   choices=['linux', 'win', 'mac', 'android', 'cros',
                            'lacros'],
                   help='If present, run for the target platform, '
                   'defaults to the host platform.')

  parser.addoption('--channel',
                   default='dev',
                   choices=['dev', 'canary', 'beta', 'stable', 'extended'],
                   help='The channel of Chrome to download.')

  parser.addoption('--chromedriver',
                   help='The path to the existing chromedriver. '
                   'This will ignore --channel and skip downloading.')

# pylint: disable=redefined-outer-name
@pytest.fixture(scope="session")
def chromedriver_path(pytestconfig: pytest.Config) -> str:
  """Returns a path to the chromedriver."""
  if cd_path := pytestconfig.getoption('chromedriver'):
    cd_path = os.path.abspath(cd_path)
    if not os.path.isfile(cd_path):
      raise RuntimeError(f'Given chromedriver doesn\'t exist. ({cd_path})')
    return cd_path

  platform = pytestconfig.getoption('target_platform')
  channel = pytestconfig.getoption('channel')

  # https://developer.chrome.com/docs/versionhistory/reference/#platform-identifiers
  downloaded_dir = None
  if platform == "linux":
    ver = test_utils.find_version('linux', channel)
    downloaded_dir = test_utils.download_chrome_linux(version=str(ver))
  elif platform == "mac":
    ver = test_utils.find_version('mac_arm64', channel)
    downloaded_dir = test_utils.download_chrome_mac(version=str(ver))
  elif platform == "win":
    ver = test_utils.find_version('win64', channel)
    downloaded_dir = test_utils.download_chrome_win(version=str(ver))
  else:
    raise RuntimeError(f'Given platform ({platform}) is not supported.')

  return os.path.join(downloaded_dir, 'chromedriver')

@pytest.fixture
def driver_factory(chromedriver_path: str,
                   tmp_path_factory: pytest.TempPathFactory):
  """Returns a factory that creates a webdriver."""
  @contextmanager
  def factory(chrome_options = None):
    # Crashpad is a separate process and its dump locations is set via env
    # variable.
    crash_dump_dir = tmp_path_factory.mktemp('crash', True)
    os.environ['BREAKPAD_DUMP_LOCATION'] = str(crash_dump_dir)

    chrome_options = chrome_options or ChromeOptions()
    chrome_options.add_argument("--headless")
    chrome_options.add_experimental_option('excludeSwitches',
                                          ['disable-background-networking'])
    driver = None
    try:
      driver = webdriver.Chrome(chromedriver_path, options=chrome_options)
      yield driver
    except WebDriverException as e:
      # Report this to be part of test result.
      if os.listdir(crash_dump_dir):
        logging.error('Chrome crashed and exited abnormally.\n%s', e)
      else:
        logging.error('Uncaught WebDriver exception thrown.\n%s', e)
      raise
    finally:
      if driver:
        driver.quit()
      shutil.rmtree(crash_dump_dir, ignore_errors=True)

  return factory

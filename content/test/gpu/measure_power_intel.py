# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script runs power measurements for browsers using Intel Power Gadget.

This script only works on Windows/Mac with Intel CPU. Intel Power Gadget needs
to be installed on the machine before this script works. The software can be
downloaded from:
  https://software.intel.com/en-us/articles/intel-power-gadget

Newer IPG versions might also require Visual C++ 2010 runtime to be installed
on Windows:
  https://www.microsoft.com/en-us/download/details.aspx?id=14632

Install selenium via pip: `pip install selenium`
Selenium 4 is required for Edge. Selenium 4.00-alpha5 or later is recommended:
  `pip install selenium==4.0.0a5`

And finally install the web drivers for Chrome (and Edge if needed):
  http://chromedriver.chromium.org/downloads
  https://developer.microsoft.com/en-us/microsoft-edge/tools/webdriver/

Sample runs:

python measure_power_intel.py --browser=canary --duration=10 --delay=5
  --verbose --url="https://www.youtube.com/watch?v=0XdS37Re1XQ"
  --extra-browser-args="--no-sandbox"

Supported browsers (--browser=xxx): 'stable', 'beta', 'dev', 'canary',
  'chromium', 'edge', and path_to_exe_file.
For Edge from insider channels (beta, dev, can), use path_to_exe_file.

It is recommended to test with optimized builds of Chromium e.g. these GN args:

  is_debug = false
  is_component_build = false
  is_official_build = true # optimization similar to official builds
  use_remoteexec = true
  enable_nacl = false
  proprietary_codecs = true
  ffmpeg_branding = "Chrome"

It might also help to disable unnecessary background services and to unplug the
power source some time before measuring.  See "Computer setup" section here:
  https://microsoftedge.github.io/videotest/2017-04/WebdriverMethodology.html
"""

import argparse
import csv
import datetime
import logging
import os
import shutil
import sys
import tempfile

try:
  from selenium import webdriver
  from selenium.common import exceptions
except ImportError as error:
  logging.error(
      'This script needs selenium and appropriate web drivers to be installed.')
  raise

import gpu_tests.ipg_utils as ipg_utils

CHROME_STABLE_PATH_WIN = (
    r'C:\Program Files (x86)\Google\Chrome\Application\chrome.exe')
CHROME_BETA_PATH_WIN = (
    r'C:\Program Files (x86)\Google\Chrome Beta\Application\chrome.exe')
CHROME_DEV_PATH_WIN = (
    r'C:\Program Files (x86)\Google\Chrome Dev\Application\chrome.exe')
# The following two paths are relative to the LOCALAPPDATA
CHROME_CANARY_PATH_WIN = r'Google\Chrome SxS\Application\chrome.exe'
CHROMIUM_PATH_WIN = r'Chromium\Application\chrome.exe'

CHROME_STABLE_PATH_MAC = (
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')
CHROME_BETA_PATH_MAC = CHROME_STABLE_PATH_MAC
CHROME_DEV_PATH_MAC = CHROME_STABLE_PATH_MAC
CHROME_CANARY_PATH_MAC = (
    '/Applications/Google Chrome Canary.app/Contents/MacOS/Google Chrome Canary'
)

SUPPORTED_BROWSERS = ['stable', 'beta', 'dev', 'canary', 'chromium', 'edge']


def LocateBrowserWin(options_browser):
  if options_browser == 'edge':
    return 'edge'
  browser = None
  if not options_browser or options_browser == 'stable':
    browser = CHROME_STABLE_PATH_WIN
  elif options_browser == 'beta':
    browser = CHROME_BETA_PATH_WIN
  elif options_browser == 'dev':
    browser = CHROME_DEV_PATH_WIN
  elif options_browser == 'canary':
    browser = os.path.join(os.getenv('LOCALAPPDATA'), CHROME_CANARY_PATH_WIN)
  elif options_browser == 'chromium':
    browser = os.path.join(os.getenv('LOCALAPPDATA'), CHROMIUM_PATH_WIN)
  elif options_browser.endswith('.exe'):
    browser = options_browser
  else:
    logging.warning('Invalid value for --browser')
    logging.warning(
        'Supported values: %s, or a full path to a browser executable.',
        ', '.join(SUPPORTED_BROWSERS))
    return None
  if not os.path.exists(browser):
    logging.warning("Can't locate browser at %s", browser)
    logging.warning('Please pass full path to the executable in --browser')
    return None
  return browser


def LocateBrowserMac(options_browser):
  browser = None
  if not options_browser or options_browser == 'stable':
    browser = CHROME_STABLE_PATH_MAC
  elif options_browser == 'beta':
    browser = CHROME_BETA_PATH_MAC
  elif options_browser == 'dev':
    browser = CHROME_DEV_PATH_MAC
  elif options_browser == 'canary':
    browser = CHROME_CANARY_PATH_MAC
  elif options_browser.endswith('Chromium'):
    browser = options_browser
  else:
    logging.warning('Invalid value for --browser')
    logging.warning(
        'Supported values: %s, or a full path to a browser executable.',
        ', '.join(SUPPORTED_BROWSERS))
    return None
  if not os.path.exists(browser):
    logging.warning("Can't locate browser at %s", browser)
    logging.warning('Please pass full path to the executable in --browser')
    return None
  return browser


def LocateBrowser(options_browser):
  if sys.platform == 'win32':
    return LocateBrowserWin(options_browser)
  if sys.platform == 'darwin':
    return LocateBrowserMac(options_browser)
  logging.warning('This script only runs on Windows/Mac.')
  return None


def CreateWebDriver(browser, user_data_dir, url, fullscreen,
                    extra_browser_args):
  if browser == 'edge' or browser.endswith('msedge.exe'):
    options = webdriver.EdgeOptions()
    # Set use_chromium to true or an error will be triggered that the latest
    # MSEdgeDriver doesn't support an older version (non-chrome based) of
    # MSEdge.
    options.use_chromium = True
    options.binary_location = browser
    for arg in extra_browser_args:
      options.add_argument(arg)
    logging.debug(' '.join(options.arguments))
    driver = webdriver.Edge(options=options)
  else:
    options = webdriver.ChromeOptions()
    options.binary_location = browser
    options.add_argument('--user-data-dir=%s' % user_data_dir)
    options.add_argument('--no-first-run')
    options.add_argument('--no-default-browser-check')
    options.add_argument('--autoplay-policy=no-user-gesture-required')
    options.add_argument('--start-maximized')
    for arg in extra_browser_args:
      options.add_argument(arg)
    logging.debug(' '.join(options.arguments))
    driver = webdriver.Chrome(options=options)
  driver.implicitly_wait(30)
  if url is not None:
    driver.get(url)
  if fullscreen:
    try:
      video_el = driver.find_element_by_tag_name('video')
      actions = webdriver.ActionChains(driver)
      actions.move_to_element(video_el)
      actions.double_click(video_el)
      actions.perform()
    except exceptions.InvalidSelectorException:
      logging.warning('Could not locate video element to make fullscreen')
  return driver


# pylint: disable=too-many-arguments
def MeasurePowerOnce(browser, logfile, duration, delay, resolution, url,
                     fullscreen, extra_browser_args):
  logging.debug('Logging into %s', logfile)
  user_data_dir = tempfile.mkdtemp()

  driver = CreateWebDriver(browser, user_data_dir, url, fullscreen,
                           extra_browser_args)
  ipg_utils.RunIPG(duration + delay, resolution, logfile)
  driver.quit()

  try:
    shutil.rmtree(user_data_dir)
  except Exception as err:  # pylint: disable=broad-except
    logging.warning('Failed to remove temporary folder: %s', user_data_dir)
    logging.warning('Please kill browser and remove it manually to avoid leak')
    logging.debug(err)
  results = ipg_utils.AnalyzeIPGLogFile(logfile, delay)
  return results
# pylint: enable=too-many-arguments


def ParseArgs():
  parser = argparse.ArgumentParser()
  parser.add_argument('--browser',
                      help=('select which browser to run. Options include: ' +
                            ', '.join(SUPPORTED_BROWSERS) +
                            ', or a full path to a browser executable. ' +
                            'By default, stable is selected.'))
  parser.add_argument('--duration',
                      default=60,
                      type=int,
                      help='specify how many seconds Intel Power Gadget '
                      'measures. By default, 60 seconds is selected.')
  parser.add_argument('--delay',
                      default=10,
                      type=int,
                      help='specify how many seconds we skip in the data '
                      'Intel Power Gadget collects. This time is for starting '
                      'video play, switching to fullscreen mode, etc. '
                      'By default, 10 seconds is selected.')
  parser.add_argument('--resolution',
                      default=100,
                      type=int,
                      help='specify how often Intel Power Gadget samples '
                      'data in milliseconds. By default, 100 ms is selected.')
  parser.add_argument('--logdir',
                      help='specify where Intel Power Gadget stores its log.'
                      'By default, it is the current path.')
  parser.add_argument('--logname',
                      help='specify the prefix for Intel Power Gadget log '
                      'filename. By default, it is PowerLog.')
  parser.add_argument('-v',
                      '--verbose',
                      action='store_true',
                      default=False,
                      help='print out debug information.')
  parser.add_argument('--repeat',
                      default=1,
                      type=int,
                      help='specify how many times to run the measurements.')
  parser.add_argument('--url',
                      help='specify the webpage URL the browser launches with.')
  parser.add_argument(
      '--extra-browser-args',
      dest='extra_browser_args',
      help='specify extra command line switches for the browser '
      'that are separated by spaces (quoted).')
  parser.add_argument(
      '--extra-browser-args-filename',
      dest='extra_browser_args_filename',
      metavar='FILE',
      help='specify extra command line switches for the browser '
      'in a text file that are separated by whitespace.')
  parser.add_argument('--fullscreen',
                      action='store_true',
                      default=False,
                      help='specify whether video should be made fullscreen.')

  return parser.parse_args()


def main():
  options = ParseArgs()
  if options.verbose:
    logging.basicConfig(level=logging.DEBUG)

  browser = LocateBrowser(options.browser)
  if not browser:
    return

  # TODO(zmo): Add code to disable a bunch of Windows services that might
  # affect power consumption.

  log_prefix = options.logname or 'PowerLog'

  all_results = []

  extra_browser_args = []
  if options.extra_browser_args:
    extra_browser_args = options.extra_browser_args.split()
  if options.extra_browser_args_filename:
    if not os.path.isfile(options.extra_browser_args_filename):
      logging.error("Can't locate file at %s",
                    options.extra_browser_args_filename)
    else:
      with open(options.extra_browser_args_filename, 'r') as f:
        extra_browser_args.extend(f.read().split())
        f.close()

  for run in range(1, options.repeat + 1):
    logfile = ipg_utils.GenerateIPGLogFilename(log_prefix, options.logdir, run,
                                               options.repeat, True)
    print('Iteration #%d out of %d' % (run, options.repeat))
    results = MeasurePowerOnce(browser, logfile, options.duration,
                               options.delay, options.resolution, options.url,
                               options.fullscreen, extra_browser_args)
    print(results)
    all_results.append(results)

  now = datetime.datetime.now()
  results_filename = '%s_%s_results.csv' % (log_prefix,
                                            now.strftime('%Y%m%d%H%M%S'))
  try:
    with open(results_filename, 'wb') as results_csv:
      labels = sorted(all_results[0].keys())
      w = csv.DictWriter(results_csv, fieldnames=labels)
      w.writeheader()
      w.writerows(all_results)
  except Exception as err:  # pylint: disable=broad-except
    logging.warning('Failed to write results file %s', results_filename)
    logging.debug(err)


if __name__ == '__main__':
  sys.exit(main())

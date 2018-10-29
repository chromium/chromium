# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script runs power measurements for browsers using Intel Power Gadget.

This script only works on Windows with Intel CPU. Intel Power Gadget needs to
be installed on the machine before this script works. The software can be
downloaded from:
  https://software.intel.com/en-us/articles/intel-power-gadget-20

Sample runs:

python measure_power_win_intel.py --browser=canary --duration=10 --delay=5
  --verbose --url="https://www.youtube.com/watch?v=0XdS37Re1XQ"
  --extra-browser-args="--no-sandbox --disable-features=UseSurfaceLayerForVideo"
"""

import csv
import datetime
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import time
import optparse

import gpu_tests.ipg_utils as ipg_utils

CHROME_STABLE_PATH = (
    r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe")
CHROME_BETA_PATH = (
    r"C:\Program Files (x86)\Google\Chrome Beta\Application\chrome.exe")
CHROME_DEV_PATH = (
    r"C:\Program Files (x86)\Google\Chrome Dev\Application\chrome.exe")
# The following two paths are relative to the LOCALAPPDATA
CHROME_CANARY_PATH = r"Google\Chrome SxS\Application\chrome.exe"
CHROMIUM_PATH = r"Chromium\Application\chrome.exe"

SUPPORTED_BROWSERS = ['stable', 'beta', 'dev', 'canary', 'chromium']


def LocateBrowser(options_browser):
  browser = None
  if not options_browser or options_browser == 'stable':
    browser = CHROME_STABLE_PATH
  elif options_browser == 'beta':
    browser = CHROME_BETA_PATH
  elif options_browser == 'dev':
    browser = CHROME_DEV_PATH
  elif options_browser == 'canary':
    browser = os.path.join(os.getenv('LOCALAPPDATA'), CHROME_CANARY_PATH)
  elif options_browser == 'chromium':
    browser = os.path.join(os.getenv('LOCALAPPDATA'), CHROMIUM_PATH)
  elif options_browser.endswith(".exe"):
    browser = options_browser
  else:
    logging.warning("Invalid value for --browser")
    logging.warning(
        "Supported values: %s, or a full path to a browser executable.",
        ", ".join(SUPPORTED_BROWSERS))
    return None
  if not os.path.exists(browser):
    logging.warning("Can't locate browser at " + browser)
    logging.warning("Please pass full path to the executable in --browser")
    return None
  return browser


def LaunchBrowser(browser, user_data_dir, url, extra_browser_args):
  args = []
  args.append(browser)
  if url:
    args.append(url)
  if browser.endswith("chrome.exe"):
    args.append('--user-data-dir=%s' % user_data_dir)
    args.append('--no-first-run')
    args.append('--no-default-browser-check')
    args.append('--autoplay-policy=no-user-gesture-required')
    if extra_browser_args:
      args.extend(extra_browser_args.split(' '))
  logging.debug(" ".join(args))
  browser_proc = subprocess.Popen(args)
  return browser_proc


def MeasurePowerOnce(browser, logfile, duration, delay, resolution, url,
                     extra_browser_args):
  logging.debug("Logging into " + logfile)
  user_data_dir = tempfile.mkdtemp()
  browser_proc = LaunchBrowser(browser, user_data_dir, url, extra_browser_args)
  ipg_utils.RunIPG(duration + delay, resolution, logfile)
  browser_proc.kill()
  for _ in range(100):
    if browser_proc.poll() is not None:
      break
    logging.debug("Waiting for browser to exit")
    time.sleep(0.05)
  try:
    shutil.rmtree(user_data_dir)
  except Exception as err:
    logging.warning("Failed to remove temporary folder: " + user_data_dir)
    logging.warning("Please kill browser and remove it manually to avoid leak")
    logging.debug(err)
  results = ipg_utils.AnalyzeIPGLogFile(logfile, delay)
  return results


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option("--browser",
                    help=("select which browser to run. Options include: " +
                          ", ".join(SUPPORTED_BROWSERS) +
                          ", or a full path to a browser executable. " +
                          "By default, stable is selected."))
  parser.add_option("--duration", default=60, type="int",
                    help="specify how many seconds Intel Power Gadget "
                    "measures. By default, 60 seconds is selected.")
  parser.add_option("--delay", default=10, type="int",
                    help="specify how many seconds we skip in the data "
                    "Intel Power Gadget collects. This time is for starting "
                    "video play, switching to fullscreen mode, etc. "
                    "By default, 10 seconds is selected.")
  parser.add_option("--resolution", default=100, type="int",
                    help="specify how often Intel Power Gadget samples "
                    "data in milliseconds. By default, 100 ms is selected.")
  parser.add_option("--logdir",
                    help="specify where Intel Power Gadget stores its log."
                    "By default, it is the current path.")
  parser.add_option("--logname",
                    help="specify the prefix for Intel Power Gadget log "
                    "filename. By default, it is PowerLog.")
  parser.add_option("-v", "--verbose", action="store_true", default=False,
                    help="print out debug information.")
  parser.add_option("--repeat", default=1, type="int",
                    help="specify how many times to run the measurements.")
  parser.add_option("--url",
                    help="specify the webpage URL the browser launches with.")
  parser.add_option("--extra-browser-args", dest="extra_browser_args",
                    help="specify extra command line switches for the browser "
                    "that are separated by spaces (quoted).")
  # TODO(zmo): add an option --start-fullscreen
  (options, _) = parser.parse_args(args=argv)
  if options.verbose:
    logging.basicConfig(level=logging.DEBUG)

  browser = LocateBrowser(options.browser)
  if not browser:
    return

  # TODO(zmo): Add code to disable a bunch of Windows services that might
  # affect power consumption.

  log_prefix = options.logname or 'PowerLog'

  all_results = []

  for run in range(1, options.repeat + 1):
    logfile = ipg_utils.GenerateIPGLogFilename(
        log_prefix, options.logdir, run, options.repeat, True)
    print "Iteration #%d out of %d" % (run, options.repeat)
    results = MeasurePowerOnce(browser, logfile, options.duration,
                               options.delay, options.resolution, options.url,
                               options.extra_browser_args)
    print results
    all_results.append(results)

  now = datetime.datetime.now()
  results_filename = '%s_%s_results.csv' % (
      log_prefix, now.strftime('%Y%m%d%H%M%S'))
  try:
    with open(results_filename, 'wb') as results_csv:
      labels = sorted(all_results[0].keys())
      w = csv.DictWriter(results_csv, fieldnames=labels)
      w.writeheader()
      w.writerows(all_results)
  except Exception as err:
    logging.warning('Failed to write results file ' + results_filename)
    logging.debug(err)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

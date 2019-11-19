# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script only works on Windows with Intel CPU. Intel Power Gadget needs
to be installed on the machine before this script works. The software can be
downloaded from:
  https://software.intel.com/en-us/articles/intel-power-gadget

To run this test on a target machine without Chromium workspace checked out:
1) inside Chromium workspace, run
   python tools/mb/mb.py zip out/Release
       telemetry_gpu_integration_test_scripts_only out/myfilename.zip
   This zip doesn't include a chrome executable. The intent is to run with
   one of the stable/beta/canary/dev channels installed on the target machine.
2) copy the zip file to the target machine, unzip
3) python content/test/gpu/run_gpu_integration_test.py power --browser=canary
   (plus options listed through --help)

This script is tested and works fine with the following video sites:
  * https://www.youtube.com
  * https://www.vimeo.com
  * https://www.pond5.com
  * http://crosvideo.appspot.com
"""

from gpu_tests import gpu_integration_test
from gpu_tests import ipg_utils
from gpu_tests import path_util

import logging
import os
import py_utils
import sys
import time

# Waits for [x] seconds after browser launch before measuring power to
# avoid startup tasks affecting results.
_POWER_MEASUREMENT_DELAY = 20

# Measures power for [x] seconds and calculates the average as results.
_POWER_MEASUREMENT_DURATION = 15

# Measures power in resolution of [x] milli-seconds.
_POWER_MEASUREMENT_RESOLUTION = 100

_GPU_RELATIVE_PATH = "content/test/data/gpu/"

_DATA_PATHS = [
    os.path.join(path_util.GetChromiumSrcDir(), _GPU_RELATIVE_PATH),
    os.path.join(path_util.GetChromiumSrcDir(), 'media', 'test', 'data')
]

_VIDEO_TEST_SCRIPT = r"""
  var _video_in_fullscreen = false;
  var _colored_box = null;
  var _video_element = null;

  function _locateVideoElement() {
    // return the video element with largest width.
    var elements = document.getElementsByTagName("video");
    if (elements.length == 0)
      return null;
    var rt = elements[0];
    var max = elements[0].width;
    for (var ii = 1; ii < elements.length; ++ii) {
      if (elements[ii].width > max) {
        rt = elements[ii];
        max = elements[ii].width;
      }
    }
    return rt;
  }

  function _setupVideoElement() {
    if (_video_element) {
      // already set up
      return;
    }
    _video_element = _locateVideoElement();
    if (!_video_element)
      return;
    _video_element.muted = true;
    _video_element.loop = true;
    _video_element.autoplay = true;
  }

  function waitForVideoToPlay() {
    _setupVideoElement();
    if (!_video_element)
      return false;
    return _video_element.currentTime > 0;
  }

  function startUnderlayMode() {
    _setupVideoElement();
    if (!_video_element)
      return;
    if (!_colored_box) {
      _colored_box = document.createElement("div");
      _colored_box.style.backgroundColor = "red";
      _colored_box.style.width = "100px";
      _colored_box.style.height = "50px";
      _colored_box.style.position = "absolute";
      _colored_box.style.zIndex = "2147483647";  // max zIndex value
      var vid_rect = _video_element.getBoundingClientRect();
      var parent_rect = _video_element.parentNode.getBoundingClientRect();
      var top = vid_rect.top - parent_rect.top + 10;
      var left = vid_rect.left - parent_rect.left + 10;
      _colored_box.style.top = top.toString() + "px";
      _colored_box.style.left = left.toString() + "px";
      _colored_box.style.visibility = "visible";
      _video_element.parentNode.appendChild(_colored_box);
    }
    // Change box color between red/blue every second. This is to emulate UI
    // or ads change (once per second) on top of a video.
    setTimeout(_setBoxColorToBlue, 1000);
  }

  function _setBoxColorToBlue() {
    if (!_colored_box)
      return;
    _colored_box.style.backgroundColor = "blue";
    setTimeout(_setBoxColorToRed, 1000);
  }

  function _setBoxColorToRed() {
    if (!_colored_box)
      return;
    _colored_box.style.backgroundColor = "red";
    setTimeout(_setBoxColorToBlue, 1000);
  }

  function _locateButton(queries) {
    var buttons = document.getElementsByTagName("button");
    for (var ii = 0; ii < buttons.length; ++ii) {
      var label = buttons[ii].textContent.toLowerCase();
      for (var jj = 0; jj < queries.length; ++jj) {
        if (label.indexOf(queries[jj]) != -1) {
          return buttons[ii];
        }
      }
      label = buttons[ii].getAttribute("title") ||
              buttons[ii].getAttribute("data-tooltip-content");
      if (label) {
        label = label.toLowerCase();
        for (var jj = 0; jj < queries.length; ++jj) {
          if (label.indexOf(queries[jj]) != -1)
            return buttons[ii];
        }
      }
    }
    return null;
  }

  function _locateFullscreenButton() {
    return _locateButton(["full screen", "fullscreen"]);
  }

  function _addFullscreenChangeListener(node) {
    if (!node)
      return;
    node.addEventListener("fullscreenchange", function() {
      _video_in_fullscreen = true;
    });
    node.addEventListener("webkitfullscreenchange", function() {
      _video_in_fullscreen = true;
    });
  }

  function isVideoInFullscreen() {
    return _video_in_fullscreen;
  }

  function startFullscreenMode() {
    // Try to locate a fullscreen button on the page and click it.
    _setupVideoElement();
    if (!_video_element)
      return;
    var button = _locateFullscreenButton();
    if (button) {
      // We don't know which layer goes fullscreen, so we add callback to all
      // layers along the path from the video element to html body.
      var cur = _video_element;
      while (cur) {
        _addFullscreenChangeListener(cur);
        cur = cur.parentNode;
      }
      button.click();
    } else {
      // If we fail to locate a fullscreen button, call requestFullscreen()
      // directly on the video element's parent node.
      _video_element.style.width = "100%";
      _video_element.style.height = "100%";
      var container = _video_element.parentNode;
      _addFullscreenChangeListener(container);
      if (container.requestFullscreen) {
        container.requestFullscreen();
      } else if (container.webkitRequestFullscreen) {
        container.webkitRequestFullscreen();
      }
    }
  }
"""


class PowerMeasurementIntegrationTest(gpu_integration_test.GpuIntegrationTest):

  _url_mode = None

  @classmethod
  def Name(cls):
    return 'power'

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(PowerMeasurementIntegrationTest, cls).AddCommandlineArgs(parser)
    parser.add_option(
        "--duration",
        default=_POWER_MEASUREMENT_DURATION,
        type="int",
        help="specify how many seconds Intel Power Gadget measures. By "
        "default, %d seconds is selected." % _POWER_MEASUREMENT_DURATION)
    parser.add_option(
        "--delay",
        default=_POWER_MEASUREMENT_DELAY,
        type="int",
        help="specify how many seconds we skip in the data Intel Power Gadget "
        "collects. This time is for starting video play, switching to "
        "fullscreen mode, etc. By default, %d seconds is selected." %
        _POWER_MEASUREMENT_DELAY)
    parser.add_option(
        "--resolution",
        default=100,
        type="int",
        help="specify how often Intel Power Gadget samples data in "
        "milliseconds. By default, 100 ms is selected.")
    parser.add_option(
        "--url", help="specify the webpage URL the browser launches with.")
    parser.add_option(
        "--fullscreen",
        action="store_true",
        default=False,
        help="specify if the browser goes to fullscreen mode automatically, "
        "specifically if there is a single video element in the page, switch "
        "it to fullsrceen mode.")
    parser.add_option(
        "--underlay",
        action="store_true",
        default=False,
        help="add a layer on top so the video layer becomes an underlay.")
    parser.add_option(
        "--logdir",
        help="Speficy where the Intel Power Gadget log file should be stored. "
        "If specified, the log file name will include a timestamp. If not "
        "specified, the log file will be PowerLog.csv at the current dir and "
        "will be overwritten at next run.")
    parser.add_option(
        "--repeat",
        default=3,
        type="int",
        help="specify how many times to repreat the measurement. By default, "
        "measure only once. If measure more than once, between each "
        "measurement, browser restarts.")
    parser.add_option(
        "--outliers",
        default=0,
        type="int",
        help="if a test is repeated multiples and outliers is set to N, then "
        "N smallest results and N largest results are discarded before "
        "computing mean and stdev.")
    parser.add_option(
        "--bypass-ipg",
        action="store_true",
        default=False,
        help="Do not launch Intel Power Gadget. This is for testing "
        "convenience on machines where Intel Power Gadget does not work.")

  @classmethod
  def GenerateGpuTests(cls, options):
    if options.url is not None:
      # This is for local testing convenience only and is not to be added to
      # any bots.
      cls._url_mode = True
      yield ('URL', options.url, {
          'test_func': 'URL',
          'repeat': options.repeat,
          'outliers': options.outliers,
          'fullscreen': options.fullscreen,
          'underlay': options.underlay,
          'logdir': options.logdir,
          'duration': options.duration,
          'delay': options.delay,
          'resolution': options.resolution,
          'bypass_ipg': options.bypass_ipg
      })
    else:
      cls._url_mode = False
      yield ('Basic', '-', {
          'test_func': 'Basic',
          'repeat': options.repeat,
          'bypass_ipg': options.bypass_ipg
      })
      yield ('Video_720_MP4',
             _GPU_RELATIVE_PATH + 'power_video_bear_1280x720_mp4.html', {
                 'test_func': 'Video',
                 'repeat': options.repeat,
                 'bypass_ipg': options.bypass_ipg,
                 'underlay': False,
                 'fullscreen': False
             })
      yield ('Video_720_MP4_Underlay',
             _GPU_RELATIVE_PATH + 'power_video_bear_1280x720_mp4.html', {
                 'test_func': 'Video',
                 'repeat': options.repeat,
                 'bypass_ipg': options.bypass_ipg,
                 'underlay': True,
                 'fullscreen': False
             })
      yield ('Video_720_MP4_Fullscreen',
             _GPU_RELATIVE_PATH + 'power_video_bear_1280x720_mp4.html', {
                 'test_func': 'Video',
                 'repeat': options.repeat,
                 'bypass_ipg': options.bypass_ipg,
                 'underlay': False,
                 'fullscreen': True
             })
      yield ('Video_720_MP4_Underlay_Fullscreen',
             _GPU_RELATIVE_PATH + 'power_video_bear_1280x720_mp4.html', {
                 'test_func': 'Video',
                 'repeat': options.repeat,
                 'bypass_ipg': options.bypass_ipg,
                 'underlay': True,
                 'fullscreen': True
             })

  @classmethod
  def SetUpProcess(cls):
    super(cls, PowerMeasurementIntegrationTest).SetUpProcess()
    path_util.SetupTelemetryPaths()
    cls.CustomizeBrowserArgs(cls._AddDefaultArgs([]))
    cls.StartBrowser()
    assert cls._url_mode is not None
    if not cls._url_mode:
      cls.SetStaticServerDirs(_DATA_PATHS)

  def RunActualGpuTest(self, test_path, *args):
    test_params = args[0]
    assert test_params is not None and 'test_func' in test_params
    prefixed_test_func_name = '_RunTest_%s' % test_params['test_func']
    getattr(self, prefixed_test_func_name)(test_path, test_params)

  @staticmethod
  def _AddDefaultArgs(browser_args):
    # All tests receive the following options.
    return ['--autoplay-policy=no-user-gesture-required'] + browser_args

  @staticmethod
  def _MeasurePowerWithIPG(bypass_ipg):
    total_time = _POWER_MEASUREMENT_DURATION + _POWER_MEASUREMENT_DELAY
    if bypass_ipg:
      logging.info("Bypassing Intel Power Gadget")
      time.sleep(total_time)
      return {}
    logfile = None  # Use the default path
    ipg_utils.RunIPG(total_time, _POWER_MEASUREMENT_RESOLUTION, logfile)
    results = ipg_utils.AnalyzeIPGLogFile(logfile, _POWER_MEASUREMENT_DELAY)
    return results

  @staticmethod
  def _AppendResults(results_sum, results):
    assert type(results_sum) is dict and type(results) is dict
    assert results
    first_append = not results_sum
    for key, value in results.items():
      if first_append:
        results_sum[key] = [value]
      else:
        assert key in results_sum
        assert type(results_sum[key]) is list
        results_sum[key].append(value)
    return results_sum

  @staticmethod
  def _LogResults(results):
    # TODO(zmo): output in a way that the results can be tracked at
    # chromeperf.appspot.com.
    logging.info("Results: %s", str(results))

  def _SetupVideo(self, fullscreen, underlay):
    self.tab.action_runner.WaitForJavaScriptCondition(
        'waitForVideoToPlay()', timeout=30)
    if fullscreen:
      self.tab.action_runner.ExecuteJavaScript(
          'startFullscreenMode();', user_gesture=True)
      try:
        self.tab.action_runner.WaitForJavaScriptCondition(
            'isVideoInFullscreen()', timeout=5)
      except py_utils.TimeoutException:
        self.fail('requestFullscreen() fails to work, possibly because '
                  '|user_gesture| is not set.')
    if underlay:
      self.tab.action_runner.ExecuteJavaScript('startUnderlayMode();')

  #########################################
  # Actual test functions

  def _RunTest_Basic(self, test_path, params):
    repeat = params['repeat']
    bypass_ipg = params['bypass_ipg']

    browser_args = PowerMeasurementIntegrationTest._AddDefaultArgs([])

    results_sum = {}
    for iteration in range(repeat):
      logging.info('')
      logging.info('Iteration #%d', iteration)
      self.RestartBrowserWithArgs(browser_args)

      results = PowerMeasurementIntegrationTest._MeasurePowerWithIPG(bypass_ipg)
      results_sum = PowerMeasurementIntegrationTest._AppendResults(
          results_sum, results)
    PowerMeasurementIntegrationTest._LogResults(results_sum)

  def _RunTest_Video(self, test_path, params):
    repeat = params['repeat']
    fullscreen = params['fullscreen']
    underlay = params['underlay']
    bypass_ipg = params['bypass_ipg']

    disabled_features = [
        'D3D11VideoDecoder', 'DirectCompositionUseNV12DecodeSwapChain',
        'DirectCompositionUnderlays'
    ]
    browser_args = PowerMeasurementIntegrationTest._AddDefaultArgs(
        [# All bots are connected with a power source, however, we want to to
         # test with the code path that's enabled with battery power.
         '--disable_vp_scaling=1',
         '--disable-features=' + ','.join(disabled_features)])

    results_sum = {}
    for iteration in range(repeat):
      logging.info('')
      logging.info('Iteration #%d', iteration)
      self.RestartBrowserWithArgs(browser_args)

      url = self.UrlOfStaticFilePath(test_path)
      self.tab.Navigate(url, script_to_evaluate_on_commit=_VIDEO_TEST_SCRIPT)
      self._SetupVideo(fullscreen=fullscreen, underlay=underlay)

      results = PowerMeasurementIntegrationTest._MeasurePowerWithIPG(bypass_ipg)
      results_sum = PowerMeasurementIntegrationTest._AppendResults(
          results_sum, results)
    PowerMeasurementIntegrationTest._LogResults(results_sum)

  def _RunTest_URL(self, test_path, params):
    repeat = params['repeat']
    outliers = params['outliers']
    fullscreen = params['fullscreen']
    underlay = params['underlay']
    ipg_logdir = params['logdir']
    ipg_duration = params['duration']
    ipg_delay = params['delay']
    ipg_resolution = params['resolution']
    bypass_ipg = params['bypass_ipg']

    if repeat > 1:
      logging.info("Total iterations: %d", repeat)
    logfiles = []
    for iteration in range(repeat):
      if repeat > 1:
        logging.info("Iteration %d", iteration)
      self.tab.action_runner.Navigate(test_path, _VIDEO_TEST_SCRIPT)
      self.tab.WaitForDocumentReadyStateToBeComplete()
      self._SetupVideo(fullscreen=fullscreen, underlay=underlay)

      if bypass_ipg:
        logging.info("Bypassing Intel Power Gadget")
        time.sleep(ipg_duration + ipg_delay)
      else:
        logfile = None
        if ipg_logdir:
          if not os.path.isdir(ipg_logdir):
            self.fail("Folder " + ipg_logdir + " doesn't exist")
          logfile = ipg_utils.GenerateIPGLogFilename(
              log_dir=ipg_logdir, timestamp=True)
        ipg_utils.RunIPG(ipg_duration + ipg_delay, ipg_resolution, logfile)
        logfiles.append(logfile)

      if repeat > 1 and iteration < repeat - 1:
        self.StopBrowser()
        self.StartBrowser()

    if bypass_ipg:
      return

    if repeat == 1:
      results = ipg_utils.AnalyzeIPGLogFile(logfiles[0], ipg_delay)
      logging.info("Results: %s", str(results))
    else:
      json_path = None
      if ipg_logdir:
        json_path = os.path.join(ipg_logdir, "output.json")
        print "Results saved in ", json_path

      summary = ipg_utils.ProcessResultsFromMultipleIPGRuns(
          logfiles, ipg_delay, outliers, json_path)
      logging.info("Summary: %s", str(summary))


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])

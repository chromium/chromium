# Copyright 2018 The Chromium Authors
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

import logging
import os
import posixpath
import sys
import time
from typing import Any, List, Optional
import unittest

import dataclasses  # Built-in, but pylint gives an ordering false positive.

from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test
from gpu_tests import ipg_utils

import gpu_path_util

import py_utils

# Waits for [x] seconds after browser launch before measuring power to
# avoid startup tasks affecting results.
_POWER_MEASUREMENT_DELAY = 20

# Measures power for [x] seconds and calculates the average as results.
_POWER_MEASUREMENT_DURATION = 15

# Measures power in resolution of [x] milli-seconds.
_POWER_MEASUREMENT_RESOLUTION = 100

_GPU_RELATIVE_PATH = gpu_path_util.GPU_DATA_RELATIVE_PATH

_DATA_PATHS = [
    gpu_path_util.GPU_DATA_DIR,
    os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'media', 'test', 'data')
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


@dataclasses.dataclass
class _PowerMeasurementTestArguments():
  """Struct-like object for passing power measurement args instead of a dict."""
  test_func: str
  repeat: int
  bypass_ipg: bool
  underlay: Optional[bool] = None
  fullscreen: Optional[bool] = None
  outliers: Optional[int] = None
  ipg_logdir: Optional[str] = None
  ipg_duration: Optional[int] = None
  ipg_delay: Optional[int] = None
  ipg_resolution: Optional[int] = None


class PowerMeasurementIntegrationTest(gpu_integration_test.GpuIntegrationTest):

  _url_mode: Optional[bool] = None

  @classmethod
  def Name(cls) -> str:
    return 'power'

  @classmethod
  def AddCommandlineArgs(cls, parser: ct.CmdArgParser) -> None:
    super(PowerMeasurementIntegrationTest, cls).AddCommandlineArgs(parser)
    parser.add_argument(
        '--duration',
        default=_POWER_MEASUREMENT_DURATION,
        type=int,
        help=('Specify how many seconds Intel Power Gadget measures. By '
              'default, %(default)s seconds is selected.'))
    parser.add_argument(
        '--delay',
        default=_POWER_MEASUREMENT_DELAY,
        type=int,
        help=('Specify how many seconds we skip in the data Intel Power Gadget '
              'collects. This time is for starting video play, switching to '
              'fullscreen mode, etc. By default, %(default)s seconds is '
              'selected.'))
    parser.add_argument(
        '--resolution',
        default=100,
        type=int,
        help=('Specify how often Intel Power Gadget samples data in '
              'milliseconds. By default, %(default)s ms is selected.'))
    parser.add_argument(
        '--url', help='specify the webpage URL the browser launches with.')
    parser.add_argument(
        '--fullscreen',
        action='store_true',
        default=False,
        help=('Specify if the browser goes to fullscreen mode automatically, '
              'specifically if there is a single video element in the page, '
              'switch it to fullsrceen mode.'))
    parser.add_argument(
        '--underlay',
        action='store_true',
        default=False,
        help='Add a layer on top so the video layer becomes an underlay.')
    parser.add_argument(
        '--logdir',
        help=('Specify where the Intel Power Gadget log file should be stored. '
              'If specified, the log file name will include a timestamp. If '
              'not specified, the log file will be PowerLog.csv at the current '
              'dir and will be overwritten at next run.'))
    parser.add_argument(
        '--repeat',
        default=3,
        type=int,
        help=('Specify how many times to repreat the measurement. By default, '
              'measure only once. If measure more than once, between each '
              'measurement, browser restarts.'))
    parser.add_argument(
        '--outliers',
        default=0,
        type=int,
        help=('If a test is repeated multiples and outliers is set to N, then '
              'N smallest results and N largest results are discarded before '
              'computing mean and stdev.'))
    parser.add_argument(
        '--bypass-ipg',
        action='store_true',
        default=False,
        help=('Do not launch Intel Power Gadget. This is for testing '
              'convenience on machines where Intel Power Gadget does not '
              'work.'))

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    if options.url is not None:
      # This is for local testing convenience only and is not to be added to
      # any bots.
      cls._url_mode = True
      yield ('URL', options.url, [
          _PowerMeasurementTestArguments(test_func='URL',
                                         repeat=options.repeat,
                                         outliers=options.outliers,
                                         fullscreen=options.fullscreen,
                                         underlay=options.underlay,
                                         ipg_logdir=options.logdir,
                                         ipg_duration=options.duration,
                                         ipg_delay=options.delay,
                                         ipg_resolution=options.resolution,
                                         bypass_ipg=options.bypass_ipg)
      ])
    else:
      cls._url_mode = False
      yield ('Basic', '-', [
          _PowerMeasurementTestArguments(test_func='Basic',
                                         repeat=options.repeat,
                                         bypass_ipg=options.bypass_ipg)
      ])
      yield ('Video_720_MP4',
             posixpath.join(_GPU_RELATIVE_PATH,
                            'power_video_bear_1280x720_mp4.html'),
             [
                 _PowerMeasurementTestArguments(test_func='Video',
                                                repeat=options.repeat,
                                                bypass_ipg=options.bypass_ipg,
                                                underlay=False,
                                                fullscreen=False)
             ])
      yield ('Video_720_MP4_Underlay',
             posixpath.join(_GPU_RELATIVE_PATH,
                            'power_video_bear_1280x720_mp4.html'),
             [
                 _PowerMeasurementTestArguments(test_func='Video',
                                                repeat=options.repeat,
                                                bypass_ipg=options.bypass_ipg,
                                                underlay=True,
                                                fullscreen=False)
             ])
      yield ('Video_720_MP4_Fullscreen',
             posixpath.join(_GPU_RELATIVE_PATH,
                            'power_video_bear_1280x720_mp4.html'),
             [
                 _PowerMeasurementTestArguments(test_func='Video',
                                                repeat=options.repeat,
                                                bypass_ipg=options.bypass_ipg,
                                                underlay=False,
                                                fullscreen=True)
             ])
      yield ('Video_720_MP4_Underlay_Fullscreen',
             posixpath.join(_GPU_RELATIVE_PATH,
                            'power_video_bear_1280x720_mp4.html'),
             [
                 _PowerMeasurementTestArguments(test_func='Video',
                                                repeat=options.repeat,
                                                bypass_ipg=options.bypass_ipg,
                                                underlay=True,
                                                fullscreen=True)
             ])

  @classmethod
  def SetUpProcess(cls) -> None:
    super(cls, PowerMeasurementIntegrationTest).SetUpProcess()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    assert cls._url_mode is not None
    if not cls._url_mode:
      cls.SetStaticServerDirs(_DATA_PATHS)

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    test_params = args[0]
    assert test_params is not None
    prefixed_test_func_name = '_RunTest_%s' % test_params.test_func
    getattr(self, prefixed_test_func_name)(test_path, test_params)

  @classmethod
  def GenerateBrowserArgs(cls, additional_args: List[str]) -> List[str]:
    """Adds default arguments to |additional_args|.

    See the parent class' method documentation for additional information.
    """
    default_args = super(PowerMeasurementIntegrationTest,
                         cls).GenerateBrowserArgs(additional_args)
    default_args.append(cba.AUTOPLAY_POLICY_NO_USER_GESTURE_REQUIRED)
    return default_args

  @staticmethod
  def _MeasurePowerWithIPG(bypass_ipg: bool) -> dict:
    total_time = _POWER_MEASUREMENT_DURATION + _POWER_MEASUREMENT_DELAY
    if bypass_ipg:
      logging.info('Bypassing Intel Power Gadget')
      time.sleep(total_time)
      return {}
    logfile = None  # Use the default path
    ipg_utils.RunIPG(total_time, _POWER_MEASUREMENT_RESOLUTION, logfile)
    results = ipg_utils.AnalyzeIPGLogFile(logfile, _POWER_MEASUREMENT_DELAY)
    return results

  @staticmethod
  def _AppendResults(results_sum: dict, results: dict) -> dict:
    assert isinstance(results_sum, dict) and isinstance(results, dict)
    assert results
    first_append = not results_sum
    for key, value in results.items():
      if first_append:
        results_sum[key] = [value]
      else:
        assert key in results_sum
        assert isinstance(results_sum[key], list)
        results_sum[key].append(value)
    return results_sum

  @staticmethod
  def _LogResults(results: dict) -> None:
    # TODO(zmo): output in a way that the results can be tracked at
    # chromeperf.appspot.com.
    logging.info('Results: %s', str(results))

  def _SetupVideo(self, fullscreen: bool, underlay: bool) -> None:
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

  def _RunTest_Basic(self, test_path: str,
                     params: _PowerMeasurementTestArguments) -> None:
    del test_path  # Unused in this particular test.

    results_sum = {}
    for iteration in range(params.repeat):
      logging.info('')
      logging.info('Iteration #%d', iteration)
      self.RestartBrowserWithArgs([])

      results = PowerMeasurementIntegrationTest._MeasurePowerWithIPG(
          params.bypass_ipg)
      results_sum = PowerMeasurementIntegrationTest._AppendResults(
          results_sum, results)
    PowerMeasurementIntegrationTest._LogResults(results_sum)

  def _RunTest_Video(self, test_path: str,
                     params: _PowerMeasurementTestArguments) -> None:
    disabled_features = ['D3D11VideoDecoder']

    results_sum = {}
    for iteration in range(params.repeat):
      logging.info('')
      logging.info('Iteration #%d', iteration)
      self.RestartBrowserWithArgs([
          # All bots are connected with a power source, however, we want to to
          # test with the code path that's enabled with battery power.
          cba.DISABLE_DIRECT_COMPOSITION_VP_SCALING,
          '--disable-features=' + ','.join(disabled_features)
      ])

      url = self.UrlOfStaticFilePath(test_path)
      self.tab.Navigate(url, script_to_evaluate_on_commit=_VIDEO_TEST_SCRIPT)
      self._SetupVideo(fullscreen=params.fullscreen, underlay=params.underlay)

      results = PowerMeasurementIntegrationTest._MeasurePowerWithIPG(
          params.bypass_ipg)
      results_sum = PowerMeasurementIntegrationTest._AppendResults(
          results_sum, results)
    PowerMeasurementIntegrationTest._LogResults(results_sum)

  def _RunTest_URL(self, test_path: str,
                   params: _PowerMeasurementTestArguments) -> None:
    repeat = params.repeat
    ipg_logdir = params.ipg_logdir
    ipg_duration = params.ipg_duration
    ipg_delay = params.ipg_delay
    bypass_ipg = params.bypass_ipg

    if repeat > 1:
      logging.info('Total iterations: %d', repeat)
    logfiles = []
    for iteration in range(repeat):
      if repeat > 1:
        logging.info('Iteration %d', iteration)
      self.tab.action_runner.Navigate(test_path, _VIDEO_TEST_SCRIPT)
      self.tab.WaitForDocumentReadyStateToBeComplete()
      self._SetupVideo(fullscreen=params.fullscreen, underlay=params.underlay)

      if bypass_ipg:
        logging.info('Bypassing Intel Power Gadget')
        time.sleep(ipg_duration + ipg_delay)
      else:
        logfile = None
        if ipg_logdir:
          if not os.path.isdir(ipg_logdir):
            self.fail('Folder ' + ipg_logdir + " doesn't exist")
          logfile = ipg_utils.GenerateIPGLogFilename(
              log_dir=ipg_logdir, timestamp=True)
        ipg_utils.RunIPG(ipg_duration + ipg_delay, params.ipg_resolution,
                         logfile)
        logfiles.append(logfile)

      if repeat > 1 and iteration < repeat - 1:
        self.StopBrowser()
        self.StartBrowser()

    if bypass_ipg:
      return

    if repeat == 1:
      results = ipg_utils.AnalyzeIPGLogFile(logfiles[0], ipg_delay)
      logging.info('Results: %s', str(results))
    else:
      json_path = None
      if ipg_logdir:
        json_path = os.path.join(ipg_logdir, 'output.json')
        print('Results saved in ', json_path)

      summary = ipg_utils.ProcessResultsFromMultipleIPGRuns(
          logfiles, ipg_delay, params.outliers, json_path)
      logging.info('Summary: %s', str(summary))

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'power_measurement_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])

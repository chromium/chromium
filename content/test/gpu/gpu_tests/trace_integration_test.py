# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(dawn:549) Move WebGPU caching tests to a separate module to trim file.
# pylint: disable=too-many-lines

from enum import Enum
import logging
import os
import posixpath
import sys
import tempfile
from typing import Any, Dict, Generator, Iterator, List, Optional, Tuple
import unittest

from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test
from gpu_tests import pixel_test_pages

import gpu_path_util

from telemetry.timeline import model as model_module
from telemetry.timeline import tracing_config

gpu_data_relative_path = gpu_path_util.GPU_DATA_RELATIVE_PATH

data_paths = [
    gpu_path_util.GPU_DATA_DIR,
    os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'media', 'test', 'data')
]

webgl_test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._finished = false;
  domAutomationController._originalLog = window.console.log;
  domAutomationController._messages = '';

  domAutomationController.log = function(msg) {
    domAutomationController._messages += msg + "\n";
    domAutomationController._originalLog.apply(window.console, [msg]);
  }

  domAutomationController.send = function(msg) {
    // Issue a read pixel to synchronize the gpu process to ensure
    // the asynchronous category enabling is finished.
    var temp_canvas = document.createElement("canvas")
    temp_canvas.width = 1;
    temp_canvas.height = 1;
    var temp_gl = temp_canvas.getContext("experimental-webgl") ||
                  temp_canvas.getContext("webgl");
    if (temp_gl) {
      temp_gl.clear(temp_gl.COLOR_BUFFER_BIT);
      var id = new Uint8Array(4);
      temp_gl.readPixels(0, 0, 1, 1, temp_gl.RGBA, temp_gl.UNSIGNED_BYTE, id);
    } else {
      console.log('Failed to get WebGL context.');
    }

    domAutomationController._finished = true;
  }

  window.domAutomationController = domAutomationController;
"""

basic_test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._proceed = false;

  domAutomationController._readyForActions = false;
  domAutomationController._succeeded = false;
  domAutomationController._finished = false;
  domAutomationController._originalLog = window.console.log;
  domAutomationController._messages = '';

  domAutomationController.log = function(msg) {
    domAutomationController._messages += msg + "\n";
    domAutomationController._originalLog.apply(window.console, [msg]);
  }

  domAutomationController.send = function(msg) {
    domAutomationController._proceed = true;
    let lmsg = msg.toLowerCase();
    if (lmsg == "ready") {
      domAutomationController._readyForActions = true;
    } else {
      domAutomationController._finished = true;
      if (lmsg == "success") {
        domAutomationController._succeeded = true;
      } else {
        domAutomationController._succeeded = false;
      }
    }
  }

  window.domAutomationController = domAutomationController;
"""

# Presentation mode enums match DXGI_FRAME_PRESENTATION_MODE
_SWAP_CHAIN_PRESENTATION_MODE_COMPOSED = 0
_SWAP_CHAIN_PRESENTATION_MODE_OVERLAY = 1
_SWAP_CHAIN_PRESENTATION_MODE_NONE = 2
_SWAP_CHAIN_PRESENTATION_MODE_COMPOSITION_FAILURE = 3
# The following is defined for Chromium testing internal use.
_SWAP_CHAIN_GET_FRAME_STATISTICS_MEDIA_FAILED = -1

_GET_STATISTICS_EVENT_NAME = 'GetFrameStatisticsMedia'
_SWAP_CHAIN_PRESENT_EVENT_NAME = 'SwapChain::Present'
_UPDATE_OVERLAY_EVENT_NAME = 'DCLayerTree::VisualTree::UpdateOverlay'

_SUPPORTED_WIN_AMD_GPUS_WITH_NV12_ROTATED_OVERLAYS = [0x7340]

_HTML_CANVAS_NOTIFY_LISTENERS_CANVAS_CHANGED_EVENT_NAME =\
    'HTMLCanvasElement::NotifyListenersCanvasChanged'

_STATIC_BITMAP_TO_VID_FRAME_CONVERT_EVENT_NAME =\
    'StaticBitmapImageToVideoFrameCopier::Convert'

# Caching events and constants
_GPU_HOST_STORE_BLOB_EVENT_NAME =\
    'GpuHostImpl::StoreBlobToDisk'
_WEBGPU_BLOB_CACHE_HIT_EVENT_NAME = \
    'DawnCachingInterface::CacheHit'
# Refers to GpuDiskCacheType::kDawnWebGPU see the following header:
#     gpu/ipc/common/gpu_disk_cache_type.h
_WEBGPU_CACHE_HANDLE_TYPE = 1

# Special 'other_args' keys that is used to pass additional arguments
_MIN_CACHE_HIT_KEY = 'min_cache_hits'
_PROFILE_DIR_KEY = 'profile_dir'
_PROFILE_TYPE_KEY = 'profile_type'


class _TraceTestOrigin(Enum):
  """Enum type for different origin types when navigating to URLs.

  The default enum DEFAULT resolves URLs using the explicit localhost IP,
  i.e. 127.0.0.1. LOCALHOST resolves URLs using 'localhost' instead. This is
  useful when we want the navigations to hit the same resource but appear to
  be from a different origin.

  As an implementation detail, the values of the enums correspond to the name of
  the SeriallyExecutedBrowserTestCase instance functions to get the URLs."""
  DEFAULT = 'UrlOfStaticFilePath'
  LOCALHOST = 'LocalhostUrlOfStaticFilePath'


class _TraceTestArguments():
  """Struct-like object for passing trace test arguments instead of dicts."""

  def __init__(  # pylint: disable=too-many-arguments
      self,
      browser_args: List[str],
      category: str,
      test_harness_script: str,
      finish_js_condition: str,
      success_eval_func: str,
      other_args: dict,
      restart_browser: bool = True,
      origin: _TraceTestOrigin = _TraceTestOrigin.DEFAULT):
    self.browser_args = browser_args
    self.category = category
    self.test_harness_script = test_harness_script
    self.finish_js_condition = finish_js_condition
    self.success_eval_func = success_eval_func
    self.other_args = other_args
    self.restart_browser = restart_browser
    self.origin = origin


class _CacheTraceTestArguments():
  """Struct-like object for passing persistent cache trace test arguments.

  Cache trace tests consist of a series of normal trace test invocations that
  are necessary in order to verify the expected caching behaviors. The tests
  start with a first load page which is generally used to populate cache
  entries. If |test_renavigation| is true, the same browser that opened the
  first load page is navigated to each cache page in |cache_pages| and the cache
  conditions are verified. Then, regardless of the value of |test_renavigation|,
  we iterate across the |cache_pages| again and restart the browser for each
  page using a new clean user data directory that is seeded with the contents
  from the first load page, and verify the cache conditions. The seeding just
  copies all files in the user data directory from the first load over, see
  *BrowserFinder classes for more details on the seeding:
    //third_party/catapult/telemetry/telemetry/internal/backends/chrome/

  The renavigation tests are suitable when we are verifying for cache hits since
  no new entries should be generated in the |cache_pages|. However, they are not
  suitable for cache miss cases because each |cache_page| may cause entries to
  be written to the cache, thereby causing subsequent |cache_pages| to see cache
  hits when we actually expect them to be misses. Note this is not a problem
  for the restarted browser case because each browser restart seeds a new
  temporary directory with only the contents after the first load page."""

  def __init__(  # pylint: disable=too-many-arguments
      self,
      browser_args: List[str],
      category: str,
      test_harness_script: str,
      finish_js_condition: str,
      first_load_eval_func: str,
      cache_eval_func: str,
      cache_pages: List[str],
      cache_page_origin: _TraceTestOrigin = _TraceTestOrigin.DEFAULT,
      test_renavigation: bool = True):
    self.browser_args = browser_args
    self.category = category
    self.test_harness_script = test_harness_script
    self.finish_js_condition = finish_js_condition
    self.first_load_eval_func = first_load_eval_func
    self.cache_eval_func = cache_eval_func
    self.cache_pages = cache_pages
    self.cache_page_origin = cache_page_origin
    self.test_renavigation = test_renavigation

  def GenerateFirstLoadTest(self) -> _TraceTestArguments:
    """Returns the trace test arguments for the first load cache test."""
    return _TraceTestArguments(browser_args=self.browser_args,
                               category=self.category,
                               test_harness_script=self.test_harness_script,
                               finish_js_condition=self.finish_js_condition,
                               success_eval_func=self.first_load_eval_func,
                               other_args={},
                               restart_browser=True)

  def GenerateCacheHitTests(
      self, cache_args: Optional[dict]
  ) -> Generator[Tuple[str, _TraceTestArguments], None, None]:
    """Returns a generator for all cache hit trace tests.

    First pass of tests just do a re-navigation, second pass restarts with a
    seeded profile directory.
    """
    if self.test_renavigation:
      for cache_hit_page in self.cache_pages:
        yield (posixpath.join(gpu_data_relative_path, cache_hit_page),
               _TraceTestArguments(browser_args=self.browser_args,
                                   category=self.category,
                                   test_harness_script=self.test_harness_script,
                                   finish_js_condition=self.finish_js_condition,
                                   success_eval_func=self.cache_eval_func,
                                   other_args=cache_args,
                                   restart_browser=False,
                                   origin=self.cache_page_origin))
    for cache_hit_page in self.cache_pages:
      yield (posixpath.join(gpu_data_relative_path, cache_hit_page),
             _TraceTestArguments(browser_args=self.browser_args,
                                 category=self.category,
                                 test_harness_script=self.test_harness_script,
                                 finish_js_condition=self.finish_js_condition,
                                 success_eval_func=self.cache_eval_func,
                                 other_args=cache_args,
                                 restart_browser=True,
                                 origin=self.cache_page_origin))


class TraceIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  """Tests GPU traces are plumbed through properly.

  Also tests that GPU Device traces show up on devices that support them."""

  @classmethod
  def Name(cls) -> str:
    return 'trace_test'

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    # Include the device level trace tests, even though they're
    # currently skipped on all platforms, to give a hint that they
    # should perhaps be enabled in the future.
    namespace = pixel_test_pages.PixelTestPages
    for p in namespace.DefaultPages('TraceTest'):
      yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
          _TraceTestArguments(
              browser_args=p.browser_args,
              category=cls._DisabledByDefaultTraceCategory('gpu.service'),
              test_harness_script=webgl_test_harness_script,
              finish_js_condition='domAutomationController._finished',
              success_eval_func='CheckGLCategory',
              other_args=p.other_args)
      ])
    for p in namespace.DirectCompositionPages('VideoPathTraceTest'):
      yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
          _TraceTestArguments(
              browser_args=p.browser_args,
              category=cls._DisabledByDefaultTraceCategory('gpu.service'),
              test_harness_script=basic_test_harness_script,
              finish_js_condition='domAutomationController._finished',
              success_eval_func='CheckVideoPath',
              other_args=p.other_args)
      ])
    for p in namespace.LowLatencyPages('SwapChainTraceTest'):
      yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
          _TraceTestArguments(
              browser_args=p.browser_args,
              category='gpu',
              test_harness_script=basic_test_harness_script,
              finish_js_condition='domAutomationController._finished',
              success_eval_func='CheckSwapChainPath',
              other_args=p.other_args)
      ])
    for p in namespace.DirectCompositionPages('OverlayModeTraceTest'):
      yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
          _TraceTestArguments(
              browser_args=p.browser_args,
              category=cls._DisabledByDefaultTraceCategory('gpu.service'),
              test_harness_script=basic_test_harness_script,
              finish_js_condition='domAutomationController._finished',
              success_eval_func='CheckOverlayMode',
              other_args=p.other_args)
      ])

    for p in namespace.VideoFromCanvasPages('WebGLCanvasCaptureTraceTest'):
      yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
          _TraceTestArguments(
              browser_args=p.browser_args,
              category='blink',
              test_harness_script=basic_test_harness_script,
              finish_js_condition='domAutomationController._finished',
              success_eval_func='CheckWebGLCanvasCapture',
              other_args=p.other_args)
      ])

    for p in namespace.WebGPUCanvasCapturePages('WebGPUTraceTest'):
      yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
          _TraceTestArguments(
              browser_args=p.browser_args,
              category='blink',
              test_harness_script=basic_test_harness_script,
              finish_js_condition='domAutomationController._finished',
              success_eval_func='CheckWebGPUCanvasCapture',
              other_args=p.other_args)
      ])

    ############################################################################
    # WebGPU caching trace tests
    #
    # The following tests are caching tests that do not render to canvas and so
    # are not a part of the pixel tests suite. Each tuple represents:
    #   (test_name, first_load_url, cache_pages)
    #
    # test_name: Name of the test.
    # first_load_url: The first URL that is loaded and when cache entries should
    #   be written. This URL determines the number of expected cache entries to
    #   expect in following loads.
    # cache_pages: List of URLs that should be both re-navigated and/or
    #   reloaded in a restarted browser to expect some cache condition.
    webgpu_cache_test_browser_args = cba.ENABLE_WEBGPU_FOR_TESTING + [
        cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
    ]
    # For the tests to run properly on Linux, we need additional args.
    if sys.platform.startswith('linux'):
      webgpu_cache_test_browser_args += ['--enable-features=Vulkan']

    # WebGPU load and reload caching tests.
    #   These tests load the |first_load_url|, records the number of cache
    #   entries written, then both re-navigates and restarts the browser for
    #   each subsequence |cache_pages| and verifies that the number of cache
    #   hits is at least equal to the number of cache entries written before.
    webgpu_caching_tests: List[Tuple[str, str, List[str]]] = [
        ('RenderPipelineMainThread', 'webgpu-caching.html?testId=render-test', [
            'webgpu-caching.html?testId=render-test',
            'webgpu-caching.html?testId=render-test-async',
            'webgpu-caching.html?testId=render-test&worker=true',
            'webgpu-caching.html?testId=render-test-async&worker=true'
        ]),
        ('RenderPipelineMainThreadAsync',
         'webgpu-caching.html?testId=render-test-async', [
             'webgpu-caching.html?testId=render-test',
             'webgpu-caching.html?testId=render-test-async',
             'webgpu-caching.html?testId=render-test&worker=true',
             'webgpu-caching.html?testId=render-test-async&worker=true'
         ]),
        ('RenderPipelineWorker',
         'webgpu-caching.html?testId=render-test&worker=true', [
             'webgpu-caching.html?testId=render-test',
             'webgpu-caching.html?testId=render-test-async',
             'webgpu-caching.html?testId=render-test&worker=true',
             'webgpu-caching.html?testId=render-test-async&worker=true'
         ]),
        ('RenderPipelineWorkerAsync',
         'webgpu-caching.html?testId=render-test-async&worker=true', [
             'webgpu-caching.html?testId=render-test',
             'webgpu-caching.html?testId=render-test-async',
             'webgpu-caching.html?testId=render-test&worker=true',
             'webgpu-caching.html?testId=render-test-async&worker=true'
         ]),
        ('RenderPipelineCrossOriginCacheHits',
         'webgpu-caching.html?testId=render-test&hostname=localhost', [
             'webgpu-caching.html?testId=render-test&hostname=localhost',
             'webgpu-caching.html?testId=render-test-async&hostname=localhost',
             'webgpu-caching.html?testId=render-test&worker=true' +
             '&hostname=localhost',
             'webgpu-caching.html?testId=render-test-async&worker=true' +
             '&hostname=localhost'
         ]),
        ('ComputePipelineMainThread', 'webgpu-caching.html?testId=compute-test',
         [
             'webgpu-caching.html?testId=compute-test',
             'webgpu-caching.html?testId=compute-test-async',
             'webgpu-caching.html?testId=compute-test&worker=true',
             'webgpu-caching.html?testId=compute-test-async&worker=true'
         ]),
        ('ComputePipelineMainThreadAsync',
         'webgpu-caching.html?testId=compute-test-async', [
             'webgpu-caching.html?testId=compute-test',
             'webgpu-caching.html?testId=compute-test-async',
             'webgpu-caching.html?testId=compute-test&worker=true',
             'webgpu-caching.html?testId=compute-test-async&worker=true'
         ]),
        ('ComputePipelineWorker',
         'webgpu-caching.html?testId=compute-test&worker=true', [
             'webgpu-caching.html?testId=compute-test',
             'webgpu-caching.html?testId=compute-test-async',
             'webgpu-caching.html?testId=compute-test&worker=true',
             'webgpu-caching.html?testId=compute-test-async&worker=true'
         ]),
        ('ComputePipelineWorkerAsync',
         'webgpu-caching.html?testId=compute-test-async&worker=true', [
             'webgpu-caching.html?testId=compute-test',
             'webgpu-caching.html?testId=compute-test-async',
             'webgpu-caching.html?testId=compute-test&worker=true',
             'webgpu-caching.html?testId=compute-test-async&worker=true'
         ]),
        ('ComputePipelineCrossOriginCacheHits',
         'webgpu-caching.html?testId=compute-test&hostname=localhost', [
             'webgpu-caching.html?testId=compute-test&hostname=localhost',
             'webgpu-caching.html?testId=compute-test-async&hostname=localhost',
             'webgpu-caching.html?testId=compute-test&worker=true' +
             '&hostname=localhost',
             'webgpu-caching.html?testId=compute-test-async&worker=true' +
             '&hostname=localhost'
         ]),
    ]
    for (name, first_load_page, cache_pages) in webgpu_caching_tests:
      yield ('WebGPUCachingTraceTest_' + name,
             posixpath.join(gpu_data_relative_path, first_load_page), [
                 _CacheTraceTestArguments(
                     browser_args=webgpu_cache_test_browser_args,
                     category='gpu',
                     test_harness_script=basic_test_harness_script,
                     finish_js_condition='domAutomationController._finished',
                     first_load_eval_func='CheckWebGPUFirstLoadCache',
                     cache_eval_func='CheckWebGPUCacheHits',
                     cache_pages=cache_pages)
             ])

    # WebGPU incognito mode caching tests
    #   These tests load the |first_load_url| (which runs the same WebGPU code
    #   multiple times) in incognito mode, verifies that the pages had some
    #   in-memory cache hits, then both re-navigates and restarts the browser
    #   for each subsequence |cache_pages| and verifies that the number of
    #   cache hits is 0 since the in-memory cache should be purged.
    webgpu_incognito_caching_tests: List[Tuple[str, str, List[str]]] = [
        ('RenderPipelineIncognito',
         'webgpu-caching.html?testId=render-test&runs=2', [
             'webgpu-caching.html?testId=render-test',
             'webgpu-caching.html?testId=render-test-async',
             'webgpu-caching.html?testId=render-test&worker=true',
             'webgpu-caching.html?testId=render-test-async&worker=true'
         ]),
        ('ComputePipelineIncognito',
         'webgpu-caching.html?testId=compute-test&runs=2', [
             'webgpu-caching.html?testId=compute-test',
             'webgpu-caching.html?testId=compute-test-async',
             'webgpu-caching.html?testId=compute-test&worker=true',
             'webgpu-caching.html?testId=compute-test-async&worker=true'
         ]),
    ]
    for (name, first_load_page, cache_pages) in webgpu_incognito_caching_tests:
      yield ('WebGPUCachingTraceTest_' + name,
             posixpath.join(gpu_data_relative_path, first_load_page), [
                 _CacheTraceTestArguments(
                     browser_args=webgpu_cache_test_browser_args +
                     ['--incognito'],
                     category='gpu',
                     test_harness_script=basic_test_harness_script,
                     finish_js_condition='domAutomationController._finished',
                     first_load_eval_func='CheckWebGPUCacheHits',
                     cache_eval_func='CheckNoWebGPUCacheHits',
                     cache_pages=cache_pages)
             ])

    # WebGPU different origin caching tests
    #   These tests load the |first_load_url| on the default origin, making sure
    #   that the load populates on-disk entries. The tests then restart the
    #   browser for subsequent |cache_pages| on localhost origin and
    #   verifies that there are no cache hits.
    webgpu_origin_caching_tests: List[Tuple[str, str, List[str]]] = [
        ('RenderPipelineDifferentOrigins',
         'webgpu-caching.html?testId=render-test', [
             'webgpu-caching.html?testId=render-test',
             'webgpu-caching.html?testId=render-test-async',
             'webgpu-caching.html?testId=render-test&worker=true',
             'webgpu-caching.html?testId=render-test-async&worker=true'
         ]),
        ('ComputePipelineDifferentOrigins',
         'webgpu-caching.html?testId=compute-test', [
             'webgpu-caching.html?testId=compute-test',
             'webgpu-caching.html?testId=compute-test-async',
             'webgpu-caching.html?testId=compute-test&worker=true',
             'webgpu-caching.html?testId=compute-test-async&worker=true'
         ]),
    ]
    for (name, first_load_page, cache_pages) in webgpu_origin_caching_tests:
      yield ('WebGPUCachingTraceTest_' + name,
             posixpath.join(gpu_data_relative_path, first_load_page), [
                 _CacheTraceTestArguments(
                     browser_args=webgpu_cache_test_browser_args,
                     category='gpu',
                     test_harness_script=basic_test_harness_script,
                     finish_js_condition='domAutomationController._finished',
                     first_load_eval_func='CheckWebGPUFirstLoadCache',
                     cache_eval_func='CheckNoWebGPUCacheHits',
                     cache_pages=cache_pages,
                     cache_page_origin=_TraceTestOrigin.LOCALHOST,
                     test_renavigation=False)
             ])

    # WebGPU cross origin cache miss tests.
    #   These tests load the |first_load_url| and ensure that the load
    #   populates on-disk entries. The tests then restart the browser for
    #   subsequent |cache_hit_pages| which are pages that should not generate
    #   cache hits because they're cross-origin with respect to the initial
    #   page, and hence should have different isolation keys.
    webgpu_xorigin_cache_miss_tests: List[Tuple[str, str, List[str]]] = [
        ('RenderPipelineCrossOriginsCacheMisses',
         'webgpu-caching.html?testId=render-test&hostname=localhost', [
             'webgpu-caching.html?testId=render-test',
             'webgpu-caching.html?testId=render-test-async',
             'webgpu-caching.html?testId=render-test&worker=true',
             'webgpu-caching.html?testId=render-test-async&worker=true'
         ]),
        ('ComputePipelineCrossOriginsCacheMisses',
         'webgpu-caching.html?testId=compute-test&hostname=localhost', [
             'webgpu-caching.html?testId=compute-test',
             'webgpu-caching.html?testId=compute-test-async',
             'webgpu-caching.html?testId=compute-test&worker=true',
             'webgpu-caching.html?testId=compute-test-async&worker=true'
         ]),
    ]
    for (name, first_load_page, cache_pages) in webgpu_xorigin_cache_miss_tests:
      yield ('WebGPUCachingTraceTest_' + name,
             posixpath.join(gpu_data_relative_path, first_load_page), [
                 _CacheTraceTestArguments(
                     browser_args=webgpu_cache_test_browser_args,
                     category='gpu',
                     test_harness_script=basic_test_harness_script,
                     finish_js_condition='domAutomationController._finished',
                     first_load_eval_func='CheckWebGPUFirstLoadCache',
                     cache_eval_func='CheckNoWebGPUCacheHits',
                     cache_pages=cache_pages,
                     test_renavigation=False)
             ])

  def _RunActualGpuTraceTest(self,
                             test_path: str,
                             args: _TraceTestArguments,
                             profile_dir: Optional[str] = None,
                             profile_type: Optional[str] = None) -> dict:
    """Returns a dictionary generated via the success evaluation."""
    if args.restart_browser:
      # The version of this test in the old GPU test harness restarted the
      # browser after each test, so continue to do that to match its behavior
      # by default for legacy tests.
      self.RestartBrowserWithArgs(args.browser_args,
                                  profile_dir=profile_dir,
                                  profile_type=profile_type)

    # Set up tracing.
    config = tracing_config.TracingConfig()
    config.chrome_trace_config.category_filter.AddExcludedCategory('*')
    config.chrome_trace_config.category_filter.AddFilter(args.category)
    config.enable_chrome_trace = True
    tab = self.tab
    tab.browser.platform.tracing_controller.StartTracing(config, 60)

    # Perform page navigation.
    url = getattr(self, args.origin.value)(test_path)
    tab.Navigate(url, script_to_evaluate_on_commit=args.test_harness_script)

    try:
      tab.action_runner.WaitForJavaScriptCondition(args.finish_js_condition,
                                                   timeout=30)
    finally:
      test_messages = tab.EvaluateJavaScript(
          'domAutomationController._messages')
      if test_messages:
        logging.info('Logging messages from the test:\n%s', test_messages)

    # Stop tracing.
    timeline_data = tab.browser.platform.tracing_controller.StopTracing()

    # Evaluate success.
    if args.success_eval_func:
      timeline_model = model_module.TimelineModel(timeline_data)
      event_iter = timeline_model.IterAllEvents(
          event_type_predicate=timeline_model.IsSliceOrAsyncSlice)
      prefixed_func_name = '_EvaluateSuccess_' + args.success_eval_func
      results = getattr(self, prefixed_func_name)(args.category, event_iter,
                                                  args.other_args)
      return results if results else {}
    return {}

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    params = args[0]
    if isinstance(params, _TraceTestArguments):
      self._RunActualGpuTraceTest(test_path, params)
    elif isinstance(params, _CacheTraceTestArguments):
      # Create a new temporary directory for each cache test that is run.
      cache_profile_dir = tempfile.TemporaryDirectory()

      # Run the first load page and get the number of expected cache hits.
      load_params = params.GenerateFirstLoadTest()
      results =\
        self._RunActualGpuTraceTest(test_path,
                                    load_params,
                                    profile_dir=cache_profile_dir.name,
                                    profile_type='exact')

      # Generate and run the cache hit tests using the seeded cache dir.
      for (hit_path, trace_params) in params.GenerateCacheHitTests(results):
        self._RunActualGpuTraceTest(hit_path,
                                    trace_params,
                                    profile_dir=cache_profile_dir.name,
                                    profile_type='clean')

  @classmethod
  def SetUpProcess(cls) -> None:
    super(TraceIntegrationTest, cls).SetUpProcess()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs(data_paths)

  @classmethod
  def GenerateBrowserArgs(cls, additional_args: List[str]) -> List[str]:
    """Adds default arguments to |additional_args|.

    See the parent class' method documentation for additional information.
    """
    default_args = super(TraceIntegrationTest,
                         cls).GenerateBrowserArgs(additional_args)
    default_args.extend([
        cba.ENABLE_LOGGING,
        cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
    ])
    return default_args

  def _GetAndAssertOverlayBotConfig(self) -> Dict[str, str]:
    overlay_bot_config = self._GetOverlayBotConfig()
    if overlay_bot_config is None:
      self.fail('Overlay bot config can not be determined')
    assert overlay_bot_config.get('direct_composition', False)
    return overlay_bot_config

  @staticmethod
  def _SwapChainPresentationModeToStr(presentation_mode: str) -> str:
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_COMPOSED:
      return 'COMPOSED'
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_OVERLAY:
      return 'OVERLAY'
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_NONE:
      return 'NONE'
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_COMPOSITION_FAILURE:
      return 'COMPOSITION_FAILURE'
    if presentation_mode == _SWAP_CHAIN_GET_FRAME_STATISTICS_MEDIA_FAILED:
      return 'GET_STATISTICS_FAILED'
    return str(presentation_mode)

  @staticmethod
  def _SwapChainPresentationModeListToStr(presentation_mode_list: List[str]
                                          ) -> str:
    list_str = None
    for mode in presentation_mode_list:
      mode_str = TraceIntegrationTest._SwapChainPresentationModeToStr(mode)
      if list_str is None:
        list_str = mode_str
      else:
        list_str = '%s,%s' % (list_str, mode_str)
    return '[%s]' % list_str

  @staticmethod
  def _DisabledByDefaultTraceCategory(category: str) -> str:
    return 'disabled-by-default-%s' % category

  #########################################
  # The test success evaluation functions

  def _EvaluateSuccess_CheckGLCategory(self, category: str,
                                       event_iterator: Iterator,
                                       other_args: dict) -> None:
    del other_args  # Unused in this particular success evaluation.
    for event in event_iterator:
      if (event.category == category
          and event.args.get('gl_category', None) == 'gpu_toplevel'):
        break
    else:
      self.fail('Trace markers for GPU category %s were not found' % category)

  def _GetVideoExpectations(self, other_args: dict) -> '_VideoExpectations':
    """Helper for creating expectations for CheckVideoPath and CheckOverlayMode.

    Args:
      other_args: The |other_args| arg passed into the test.

    Returns:
      A _VideoExpectations instance with zero_copy, pixel_format, no_overlay,
      and presentation_mode filled in.
    """
    overlay_bot_config = self._GetAndAssertOverlayBotConfig()
    expected = _VideoExpectations()
    expected.zero_copy = other_args.get('zero_copy', None)
    expected.pixel_format = other_args.get('pixel_format', None)
    expected.no_overlay = other_args.get('no_overlay', False)
    video_is_rotated = other_args.get('video_is_rotated', False)
    video_is_not_scaled = other_args.get('full_size', False)

    if overlay_bot_config.get('supports_overlays', False):
      supports_hw_nv12_overlays = overlay_bot_config[
          'nv12_overlay_support'] in ['DIRECT', 'SCALING']
      supports_hw_yuy2_overlays = overlay_bot_config[
          'yuy2_overlay_support'] in ['DIRECT', 'SCALING']
      supports_sw_nv12_overlays = overlay_bot_config[
          'nv12_overlay_support'] == 'SOFTWARE'

      if expected.pixel_format is None:
        if supports_hw_nv12_overlays:
          expected.pixel_format = 'NV12'
        elif supports_hw_yuy2_overlays:
          expected.pixel_format = 'YUY2'
        else:
          assert supports_sw_nv12_overlays
          expected.pixel_format = 'BGRA'
      else:
        if (not supports_hw_nv12_overlays and not supports_hw_yuy2_overlays):
          expected.pixel_format = 'BGRA'

      gpu = self.browser.GetSystemInfo().gpu.devices[0]
      supports_rotated_video_overlays = (
          gpu.vendor_id == 0x1002 and
          gpu.device_id in _SUPPORTED_WIN_AMD_GPUS_WITH_NV12_ROTATED_OVERLAYS)

      supports_downscaled_overlay_promotion = gpu.vendor_id != 0x8086
      no_issue_with_downscaled_overlay_promotion = (
          video_is_not_scaled or supports_downscaled_overlay_promotion)

      if (((supports_hw_nv12_overlays and expected.pixel_format == 'NV12')
           or supports_hw_yuy2_overlays)
          and (not video_is_rotated or supports_rotated_video_overlays)):
        expected.presentation_mode = 'OVERLAY'
      else:
        expected.presentation_mode = 'COMPOSED'

      if expected.zero_copy is None:
        # TODO(sunnyps): Check for overlay scaling support after making the same
        # change in SwapChainPresenter.
        expected.zero_copy = (expected.presentation_mode == 'OVERLAY'
                              and expected.pixel_format == 'NV12'
                              and supports_hw_nv12_overlays
                              and no_issue_with_downscaled_overlay_promotion
                              and not video_is_rotated)

    return expected

  def _EvaluateSuccess_CheckVideoPath(self, category: str,
                                      event_iterator: Iterator,
                                      other_args: dict) -> None:
    """Verifies Chrome goes down the code path as expected.

    Depending on whether hardware overlays are supported or not, which formats
    are supported in overlays, whether video is downscaled or not, whether
    video is rotated or not, Chrome's video presentation code path can be
    different.
    """
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    other_args = other_args or {}
    expected = self._GetVideoExpectations(other_args)

    # Verify expectations through captured trace events.
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name != _SWAP_CHAIN_PRESENT_EVENT_NAME:
        continue
      if expected.no_overlay:
        self.fail('Expected no overlay got %s' % _SWAP_CHAIN_PRESENT_EVENT_NAME)
      detected_pixel_format = event.args.get('PixelFormat', None)
      if detected_pixel_format is None:
        self.fail('PixelFormat is missing from event %s' %
                  _SWAP_CHAIN_PRESENT_EVENT_NAME)
      if expected.pixel_format != detected_pixel_format:
        self.fail('SwapChain pixel format mismatch, expected %s got %s' %
                  (expected.pixel_format, detected_pixel_format))
      detected_zero_copy = event.args.get('ZeroCopy', None)
      if detected_zero_copy is None:
        self.fail('ZeroCopy is missing from event %s' %
                  _SWAP_CHAIN_PRESENT_EVENT_NAME)
      if expected.zero_copy != detected_zero_copy:
        self.fail('ZeroCopy mismatch, expected %s got %s' %
                  (expected.zero_copy, detected_zero_copy))
      break
    else:
      if expected.no_overlay:
        return
      self.fail(
          'Events with name %s were not found' % _SWAP_CHAIN_PRESENT_EVENT_NAME)

  def _EvaluateSuccess_CheckOverlayMode(self, category: str,
                                        event_iterator: Iterator,
                                        other_args: dict) -> None:
    """Verifies video frames are promoted to overlays when supported."""
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    other_args = other_args or {}
    expected = self._GetVideoExpectations(other_args)

    presentation_mode_history = []
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name != _GET_STATISTICS_EVENT_NAME:
        continue
      if expected.no_overlay:
        self.fail('Expected no overlay got %s' % _GET_STATISTICS_EVENT_NAME)
      detected_presentation_mode = event.args.get('CompositionMode', None)
      if detected_presentation_mode is None:
        self.fail('PresentationMode is missing from event %s' %
                  _GET_STATISTICS_EVENT_NAME)
      presentation_mode_history.append(detected_presentation_mode)

    if expected.no_overlay:
      return

    valid_entry_found = False
    for index, mode in enumerate(reversed(presentation_mode_history)):
      # Be more tolerant for the beginning frames in non-overlay mode.
      # Only check the last three entries.
      if index >= 3:
        break
      if mode in (_SWAP_CHAIN_PRESENTATION_MODE_NONE,
                  _SWAP_CHAIN_GET_FRAME_STATISTICS_MEDIA_FAILED):
        # Be more tolerant to avoid test flakiness
        continue
      if (TraceIntegrationTest._SwapChainPresentationModeToStr(mode) !=
          expected.presentation_mode):
        self.fail('SwapChain presentation mode mismatch, expected %s got %s' %
                  (expected.presentation_mode,
                   TraceIntegrationTest._SwapChainPresentationModeListToStr(
                       presentation_mode_history)))
      valid_entry_found = True
    if not valid_entry_found:
      self.fail(
          'No valid frame statistics being collected: %s' % TraceIntegrationTest
          ._SwapChainPresentationModeListToStr(presentation_mode_history))

  def _EvaluateSuccess_CheckSwapChainPath(self, category: str,
                                          event_iterator: Iterator,
                                          other_args: dict) -> None:
    """Verifies that swap chains are used as expected for low latency canvas."""
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    overlay_bot_config = self._GetOverlayBotConfig()
    if overlay_bot_config is None:
      self.fail('Overlay bot config can not be determined')
    assert overlay_bot_config.get('direct_composition', False)

    expect_no_overlay = other_args and other_args.get('no_overlay', False)
    expect_overlay = not expect_no_overlay
    found_overlay = False

    # Verify expectations through captured trace events.
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name != _UPDATE_OVERLAY_EVENT_NAME:
        continue
      image_type = event.args.get('image_type', None)
      if image_type == 'DCompVisualContent':
        found_overlay = True
        break
    if expect_overlay and not found_overlay:
      self.fail(
          'Overlay expected but not found: matching %s events were not found' %
          _UPDATE_OVERLAY_EVENT_NAME)
    elif expect_no_overlay and found_overlay:
      self.fail(
          'Overlay not expected but found: matching %s events were found' %
          _UPDATE_OVERLAY_EVENT_NAME)

  def _EvaluateSuccess_CheckWebGLCanvasCapture(self, category: str,
                                               event_iterator: Iterator,
                                               other_args: dict) -> None:
    if other_args is None:
      return
    expected_one_copy = other_args.get('one_copy', None)
    expected_accelerated_two_copy = other_args.get('accelerated_two_copy', None)
    if expected_one_copy and expected_accelerated_two_copy:
      self.fail('one_copy and accelerated_two_copy are mutually exclusive')

    found_one_copy_event = False
    found_accelerated_two_copy_event = False
    # Verify expectations through captured trace events.
    for event in event_iterator:
      if event.category != category:
        continue

      if (expected_one_copy is not None and event.name ==
          _HTML_CANVAS_NOTIFY_LISTENERS_CANVAS_CHANGED_EVENT_NAME):
        detected_one_copy = event.args.get('one_copy_canvas_capture', None)

        if detected_one_copy is not None:
          found_one_copy_event = True
          if expected_one_copy != detected_one_copy:
            self.fail('one_copy_canvas_capture mismatch, expected %s got %s' %
                      (expected_one_copy, detected_one_copy))

      elif (expected_accelerated_two_copy is not None
            and event.name == _STATIC_BITMAP_TO_VID_FRAME_CONVERT_EVENT_NAME):
        detected_accelerated_two_copy = event.args.get(
            'accelerated_frame_pool_copy', None)

        if detected_accelerated_two_copy is not None:
          found_accelerated_two_copy_event = True
          if expected_accelerated_two_copy != detected_accelerated_two_copy:
            self.fail(
                'accelerated_frame_pool_copy mismatch, expected %s got %s' %
                (expected_accelerated_two_copy, detected_accelerated_two_copy))

    if expected_one_copy is not None and found_one_copy_event is False:
      self.fail('%s events with one_copy_canvas_capture were not found' %
                _HTML_CANVAS_NOTIFY_LISTENERS_CANVAS_CHANGED_EVENT_NAME)

    if (expected_accelerated_two_copy is not None
        and found_accelerated_two_copy_event is False):
      self.fail('%s events with accelerated_frame_pool_copy were not found' %
                _STATIC_BITMAP_TO_VID_FRAME_CONVERT_EVENT_NAME)

  def _EvaluateSuccess_CheckWebGPUCanvasCapture(self, category: str,
                                                event_iterator: Iterator,
                                                other_args: dict) -> None:
    expected_one_copy = other_args.get('one_copy', None)
    expected_accelerated_two_copy = other_args.get('accelerated_two_copy', None)
    if expected_one_copy and expected_accelerated_two_copy:
      self.fail('one_copy and accelerated_two_copy are mutually exclusive')

    found_one_copy_event = False
    found_accelerated_two_copy_event = False
    # Verify expectations through captured trace events.
    for event in event_iterator:
      if event.category != category:
        continue

      if (expected_one_copy is not None and event.name ==
          _HTML_CANVAS_NOTIFY_LISTENERS_CANVAS_CHANGED_EVENT_NAME):
        detected_one_copy = event.args.get('one_copy_canvas_capture', None)

        if detected_one_copy is not None:
          found_one_copy_event = True
          if expected_one_copy != detected_one_copy:
            self.fail('one_copy_canvas_capture mismatch, expected %s got %s' %
                      (expected_one_copy, detected_one_copy))

      elif (expected_accelerated_two_copy is not None
            and event.name == _STATIC_BITMAP_TO_VID_FRAME_CONVERT_EVENT_NAME):
        detected_accelerated_two_copy = event.args.get(
            'accelerated_frame_pool_copy', None)

        if detected_accelerated_two_copy is not None:
          found_accelerated_two_copy_event = True
          if expected_accelerated_two_copy != detected_accelerated_two_copy:
            self.fail(
                'accelerated_frame_pool_copy mismatch, expected %s got %s' %
                (expected_accelerated_two_copy, detected_accelerated_two_copy))

    if expected_one_copy is not None and found_one_copy_event is False:
      self.fail('%s events with one_copy_canvas_capture were not found' %
                _HTML_CANVAS_NOTIFY_LISTENERS_CANVAS_CHANGED_EVENT_NAME)

    if (expected_accelerated_two_copy is not None
        and found_accelerated_two_copy_event is False):
      self.fail('%s events with accelerated_frame_pool_copy were not found' %
                _STATIC_BITMAP_TO_VID_FRAME_CONVERT_EVENT_NAME)

  def _EvaluateSuccess_CheckWebGPUFirstLoadCache(self, category: str,
                                                 event_iterator: Iterator,
                                                 _other_args: dict) -> dict:
    stored_blobs = 0
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name == _GPU_HOST_STORE_BLOB_EVENT_NAME:
        if event.args.get('handle_type', None) == _WEBGPU_CACHE_HANDLE_TYPE:
          stored_blobs += 1
    if stored_blobs == 0:
      self.fail('Expected at least 1 cache entry to be written.')
    return {_MIN_CACHE_HIT_KEY: stored_blobs}

  def _EvaluateSuccess_CheckWebGPUCacheHits(self, category: str,
                                            event_iterator: Iterator,
                                            other_args: dict) -> None:
    cache_hits = 0
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name == _GPU_HOST_STORE_BLOB_EVENT_NAME:
        if event.args.get('handle_type', None) == _WEBGPU_CACHE_HANDLE_TYPE:
          self.fail('Unexpected WebGPU cache entry was stored on reloaded page')
      if event.name == _WEBGPU_BLOB_CACHE_HIT_EVENT_NAME:
        cache_hits += 1
    stored_blobs = other_args.get(_MIN_CACHE_HIT_KEY, 1)
    if cache_hits == 0 or cache_hits < stored_blobs:
      self.fail('WebGPU cache hits (%d) is 0 or less than blobs stored (%d).' %
                (cache_hits, stored_blobs))

  def _EvaluateSuccess_CheckNoWebGPUCacheHits(self, category: str,
                                              event_iterator: Iterator,
                                              _other_args: dict) -> None:
    cache_hits = 0
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name == _WEBGPU_BLOB_CACHE_HIT_EVENT_NAME:
        cache_hits += 1
    if cache_hits != 0:
      self.fail('Expected 0 WebGPU cache hits, but got %d.' % cache_hits)

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'trace_test_expectations.txt')
    ]


class _VideoExpectations():
  """Struct-like object for passing around video test expectations."""

  def __init__(self):
    self.pixel_format = None  # str
    self.zero_copy = None  # bool
    self.no_overlay = None  # bool
    self.presentation_mode = None  # str


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])

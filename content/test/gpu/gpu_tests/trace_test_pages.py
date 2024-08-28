# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is more akin to a .pyl/JSON file, so it's expected to be long.
# pylint: disable=too-many-lines

import functools
from typing import Any, Dict, List

from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import pixel_test_pages
from gpu_tests.util import host_information

import dataclasses  # Built-in, but pylint gives an ordering false positive.


# Can be changed to functools.cache on Python 3.9+.
@functools.lru_cache(maxsize=None)
def _GetWebGpuCacheTestBrowserArgs() -> List[str]:
  browser_args = cba.ENABLE_WEBGPU_FOR_TESTING + [
      cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
      '--enable-features=WebGPUBlobCache',
  ]

  if host_information.IsLinux():
    browser_args.append('--enable-features=Vulkan')

  return browser_args


@dataclasses.dataclass
class WebGpuCacheTracingTest():
  # The name of the test, including the prefix.
  name: str
  # The first URL that is loaded and when cache entries should be written. This
  # will determine the number of expected cache entries in following loads.
  first_load_page: str
  # List of URLs that should be both re-navigated and/or reloaded in a
  # restarted browser to expect some cache condition.
  cache_pages: List[str]
  # Additional arguments to start the browser with. Defaults to the return value
  # of _GetWebGpuCacheTestBrowserArgs().
  browser_args: List[str] = dataclasses.field(
      default_factory=_GetWebGpuCacheTestBrowserArgs)


@dataclasses.dataclass
class SimpleTracingTest:
  # The name of the test, including the prefix.
  name: str
  # The URL to load.
  url: str
  # Additional args to start the browser with.
  browser_args: List[str] = ct.EmptyList()
  # Additional test harness arguments.
  other_args: Dict[str, Any] = ct.EmptyDict()


# Inherits from PixelTestPages since a number of trace tests are just
# re-used pixel tests.
class TraceTestPages(pixel_test_pages.PixelTestPages):
  ##############################################################################
  # WebGPU caching trace tests
  #
  # These tests are caching tests that do not render to canvas, and thus are
  # not a part of the pixel tests.
  ##############################################################################

  RENDER_CACHE_PAGES = [
      'webgpu-caching.html?testId=render-test',
      'webgpu-caching.html?testId=render-test-async',
      'webgpu-caching.html?testId=render-test&worker=true',
      'webgpu-caching.html?testId=render-test-async&worker=true',
  ]

  COMPUTE_CACHE_PAGES = [
      'webgpu-caching.html?testId=compute-test',
      'webgpu-caching.html?testId=compute-test-async',
      'webgpu-caching.html?testId=compute-test&worker=true',
      'webgpu-caching.html?testId=compute-test-async&worker=true',
  ]

  @staticmethod
  def WebGpuLoadReloadCachingTests(prefix: str) -> List[WebGpuCacheTracingTest]:
    # WebGPU load and reload caching tests.
    #   These tests load the |first_load_url|, records the number of cache
    #   entries written, then both re-navigates and restarts the browser for
    #   each subsequence |cache_pages| and verifies that the number of cache
    #   hits is at least equal to the number of cache entries written before.
    return [
        WebGpuCacheTracingTest(
            name=f'{prefix}_RenderPipelineMainThread',
            first_load_page='webgpu-caching.html?testId=render-test',
            cache_pages=TraceTestPages.RENDER_CACHE_PAGES,
        ),
        WebGpuCacheTracingTest(
            name=f'{prefix}_RenderPipelineMainThreadAsync',
            first_load_page='webgpu-caching.html?testId=render-test-async',
            cache_pages=TraceTestPages.RENDER_CACHE_PAGES,
        ),
        WebGpuCacheTracingTest(
            name=f'{prefix}_RenderPipelineWorker',
            first_load_page=('webgpu-caching.html?'
                             'testId=render-test&worker=true'),
            cache_pages=TraceTestPages.RENDER_CACHE_PAGES),
        WebGpuCacheTracingTest(
            name=f'{prefix}_RenderPipelineWorkerAsync',
            first_load_page=('webgpu-caching.html?'
                             'testId=render-test-async&worker=true'),
            cache_pages=TraceTestPages.RENDER_CACHE_PAGES,
            browser_args=_GetWebGpuCacheTestBrowserArgs(),
        ),
        WebGpuCacheTracingTest(
            name=f'{prefix}_RenderPipelineCrossOriginCacheHits',
            first_load_page=('webgpu-caching.html?'
                             'testId=render-test&hostname=localhost'),
            cache_pages=[
                'webgpu-caching.html?testId=render-test&hostname=localhost',
                ('webgpu-caching.html?'
                 'testId=render-test-async&hostname=localhost'),
                ('webgpu-caching.html?'
                 'testId=render-test&worker=true&hostname=localhost'),
                ('webgpu-caching.html?'
                 'testId=render-test-async&worker=true&hostname=localhost'),
            ],
        ),
        WebGpuCacheTracingTest(
            name=f'{prefix}_ComputePipelineMainThread',
            first_load_page='webgpu-caching.html?testId=compute-test',
            cache_pages=TraceTestPages.COMPUTE_CACHE_PAGES,
        ),
        WebGpuCacheTracingTest(
            name=f'{prefix}_ComputePipelineMainThreadAsync',
            first_load_page='webgpu-caching.html?testId=compute-test-async',
            cache_pages=TraceTestPages.COMPUTE_CACHE_PAGES,
        ),
        WebGpuCacheTracingTest(
            name=f'{prefix}_ComputePipelineWorker',
            first_load_page=('webgpu-caching.html?'
                             'testId=compute-test&worker=true'),
            cache_pages=TraceTestPages.COMPUTE_CACHE_PAGES,
        ),
        WebGpuCacheTracingTest(
            name=f'{prefix}_ComputePipelineWorkerAsync',
            first_load_page=('webgpu-caching.html?'
                             'testId=compute-test-async&worker=true'),
            cache_pages=TraceTestPages.COMPUTE_CACHE_PAGES,
        ),
        WebGpuCacheTracingTest(
            name=f'{prefix}_ComputePipelineCrossOriginCacheHits',
            first_load_page=('webgpu-caching.html?'
                             'testId=compute-test&hostname=localhost'),
            cache_pages=[
                'webgpu-caching.html?testId=compute-test&hostname=localhost',
                ('webgpu-caching.html?'
                 'testId=compute-test-async&hostname=localhost'),
                ('webgpu-caching.html?'
                 'testId=compute-test&worker=true&hostname=localhost'),
                ('webgpu-caching.html?'
                 'testId=compute-test-async&worker=true&hostname=localhost'),
            ],
        ),
    ]

  @staticmethod
  def WebGpuIncognitoCachingTests(prefix: str) -> List[WebGpuCacheTracingTest]:
    # WebGPU incognito mode caching tests
    #   These tests load the |first_load_url| (which runs the same WebGPU code
    #   multiple times) in incognito mode, verifies that the pages had some
    #   in-memory cache hits, then both re-navigates and restarts the browser
    #   for each subsequence |cache_pages| and verifies that the number of
    #   cache hits is 0 since the in-memory cache should be purged.
    return [
        WebGpuCacheTracingTest(
            name=f'{prefix}_RenderPipelineIncognito',
            first_load_page='webgpu-caching.html?testId=render-test&runs=2',
            cache_pages=TraceTestPages.RENDER_CACHE_PAGES,
            browser_args=(_GetWebGpuCacheTestBrowserArgs() + ['--incognito']),
        ),
        WebGpuCacheTracingTest(
            name=f'{prefix}_ComputePipelineIncognito',
            first_load_page='webgpu-caching.html?testId=compute-test&runs=2',
            cache_pages=TraceTestPages.COMPUTE_CACHE_PAGES,
            browser_args=(_GetWebGpuCacheTestBrowserArgs() + ['--incognito']),
        ),
    ]

  @staticmethod
  def WebGpuDifferentOriginCachingTests(
      prefix: str) -> List[WebGpuCacheTracingTest]:
    # WebGPU different origin caching tests
    #   These tests load the |first_load_url| on the default origin, making sure
    #   that the load populates on-disk entries. The tests then restart the
    #   browser for subsequent |cache_pages| on localhost origin and
    #   verifies that there are no cache hits.
    return [
        WebGpuCacheTracingTest(
            name=f'{prefix}_RenderPipelineDifferentOrigins',
            first_load_page='webgpu-caching.html?testId=render-test',
            cache_pages=TraceTestPages.RENDER_CACHE_PAGES,
        ),
        WebGpuCacheTracingTest(
            name=f'{prefix}_ComputePipelineDifferentOrigins',
            first_load_page='webgpu-caching.html?testId=compute-test',
            cache_pages=TraceTestPages.COMPUTE_CACHE_PAGES,
        ),
    ]

  @staticmethod
  def WebGpuCrossOriginCacheMissTests(
      prefix: str) -> List[WebGpuCacheTracingTest]:
    # WebGPU cross origin cache miss tests.
    #   These tests load the |first_load_url| and ensure that the load
    #   populates on-disk entries. The tests then restart the browser for
    #   subsequent |cache_hit_pages| which are pages that should not generate
    #   cache hits because they're cross-origin with respect to the initial
    #   page, and hence should have different isolation keys.
    return [
        WebGpuCacheTracingTest(
            name=f'{prefix}_RenderPipelineCrossOriginsCacheMisses',
            first_load_page=('webgpu-caching.html?'
                             'testId=render-test&hostname=localhost'),
            cache_pages=TraceTestPages.RENDER_CACHE_PAGES,
        ),
        WebGpuCacheTracingTest(
            name=f'{prefix}_ComputePipelineCrossOriginsCacheMisses',
            first_load_page=('webgpu-caching.html?'
                             'testId=compute-test&hostname=localhost'),
            cache_pages=TraceTestPages.COMPUTE_CACHE_PAGES,
        ),
    ]

  @staticmethod
  def RootSwapChainTests(prefix: str) -> List[SimpleTracingTest]:
    return [
        # Check that the root swap chain claims to be opaque. A root swap chain
        # with a premultiplied alpha mode has a large negative battery impact
        # (even if all the pixels are opaque).
        SimpleTracingTest(name=f'{prefix}_IsOpaque',
                          url='wait_for_compositing.html',
                          other_args={
                              'has_alpha': False,
                          }),
    ]

  @staticmethod
  def MediaFoundationD3D11VideoCaptureTests(
      prefix: str) -> List[SimpleTracingTest]:
    return [
        # Check what MediaFoundationD3D11VideoCapture works
        SimpleTracingTest(
            name=f'{prefix}_MediaFoundationD3D11VideoCapture',
            url='media_foundation_d3d11_video_capture.html',
            browser_args=[
                '--use-fake-ui-for-media-stream',
                '--enable-features=MediaFoundationD3D11VideoCapture',
            ]),
    ]

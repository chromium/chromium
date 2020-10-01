# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is more akin to a .pyl/JSON file, so it's expected to be long.
# pylint: disable=too-many-lines

import os

from gpu_tests import common_browser_args as cba
from gpu_tests import skia_gold_matching_algorithms as algo
from gpu_tests import path_util

CRASH_TYPE_GPU = 'gpu'

# These tests attempt to use test rects that are larger than the small screen
# on some Fuchsia devices, so we need to use a less-desirable screenshot capture
# method to get the entire page contents instead of just the visible portion.
PROBLEMATIC_FUCHSIA_TESTS = [
    'Maps_maps',
    'Pixel_BackgroundImage',
    'Pixel_PrecisionRoundedCorner',
    'Pixel_SolidColorBackground',
]

# Meant to be used when we know a test is going to be noisy, and we want any
# images it generates to be auto-triaged until we have enough data to calculate
# more suitable/less permissive parameters.
VERY_PERMISSIVE_SOBEL_ALGO = algo.SobelMatchingAlgorithm(
    max_different_pixels=100000000,
    pixel_delta_threshold=255,
    edge_threshold=0,
    ignored_border_thickness=1)


class PixelTestPage(object):
  """A wrapper class mimicking the functionality of the PixelTestsStorySet
  from the old-style GPU tests.
  """

  def __init__(  # pylint: disable=too-many-arguments
      self,
      url,
      name,
      test_rect,
      browser_args=None,
      gpu_process_disabled=False,
      optional_action=None,
      restart_browser_after_test=False,
      other_args=None,
      grace_period_end=None,
      expected_per_process_crashes=None,
      matching_algorithm=None):
    super(PixelTestPage, self).__init__()
    self.url = url
    self.name = name
    self.test_rect = test_rect
    self.browser_args = browser_args
    # Only a couple of tests run with the GPU process completely
    # disabled. To prevent regressions, only allow the GPU information
    # to be incomplete in these cases.
    self.gpu_process_disabled = gpu_process_disabled
    # Some of the tests require custom actions to be run. These are
    # specified as a string which is the name of a method to call in
    # PixelIntegrationTest. For example if the action here is
    # "CrashGpuProcess" then it would be defined in a
    # "_CrashGpuProcess" method in PixelIntegrationTest.
    self.optional_action = optional_action
    # Whether the browser should be forcibly restarted after the test
    # runs. The browser is always restarted after running tests with
    # optional_actions.
    self.restart_browser_after_test = restart_browser_after_test
    # These are used to pass additional arguments to the test harness.
    # VideoPathTraceTest and OverlayModeTest support the following boolean
    # arguments: expect_yuy2, zero_copy, video_is_rotated, and no_overlay.
    self.other_args = other_args
    # This allows a newly added test to be exempted from failures for a
    # (hopefully) short period after being added. This is so that any slightly
    # different but valid images that get produced by the waterfall bots can
    # be triaged without turning the bots red.
    # This should be a datetime.date object.
    self.grace_period_end = grace_period_end
    # This lets the test runner know that one or more crashes are expected as
    # part of the test. Should be a map of process type (str) to expected number
    # of crashes (int).
    self.expected_per_process_crashes = expected_per_process_crashes or {}
    # This should be a child of
    # skia_gold_matching_algorithms.SkiaGoldMatchingAlgorithm. This specifies
    # which matching algorithm Skia Gold should use for the test.
    self.matching_algorithm = (matching_algorithm
                               or algo.ExactMatchingAlgorithm())

  def CopyWithNewBrowserArgsAndSuffix(self, browser_args, suffix):
    return PixelTestPage(self.url, self.name + suffix, self.test_rect,
                         browser_args)

  def CopyWithNewBrowserArgsAndPrefix(self, browser_args, prefix):
    # Assuming the test name is 'Pixel'.
    split = self.name.split('_', 1)
    return PixelTestPage(self.url, split[0] + '_' + prefix + split[1],
                         self.test_rect, browser_args)


def CopyPagesWithNewBrowserArgsAndSuffix(pages, browser_args, suffix):
  return [
      p.CopyWithNewBrowserArgsAndSuffix(browser_args, suffix) for p in pages
  ]


def CopyPagesWithNewBrowserArgsAndPrefix(pages, browser_args, prefix):
  return [
      p.CopyWithNewBrowserArgsAndPrefix(browser_args, prefix) for p in pages
  ]


def GetMediaStreamTestBrowserArgs(media_stream_source_relpath):
  return [
      '--use-fake-device-for-media-stream', '--use-fake-ui-for-media-stream',
      '--use-file-for-fake-video-capture=' +
      os.path.join(path_util.GetChromiumSrcDir(), media_stream_source_relpath)
  ]


class PixelTestPages(object):
  @staticmethod
  def DefaultPages(base_name):
    sw_compositing_args = [cba.DISABLE_GPU_COMPOSITING]

    # The optimizer script spat out pretty similar values for most MP4 tests, so
    # combine into a single set of parameters.
    general_mp4_algo = algo.SobelMatchingAlgorithm(max_different_pixels=56300,
                                                   pixel_delta_threshold=35,
                                                   edge_threshold=80)

    return [
        PixelTestPage('pixel_background_image.html',
                      base_name + '_BackgroundImage',
                      test_rect=[20, 20, 370, 370]),
        PixelTestPage('pixel_reflected_div.html',
                      base_name + '_ReflectedDiv',
                      test_rect=[0, 0, 100, 300]),
        PixelTestPage('pixel_canvas2d.html',
                      base_name + '_Canvas2DRedBox',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_canvas2d_untagged.html',
                      base_name + '_Canvas2DUntagged',
                      test_rect=[0, 0, 257, 257]),
        PixelTestPage('pixel_css3d.html',
                      base_name + '_CSS3DBlueBox',
                      test_rect=[0, 0, 300, 300],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=0,
                          pixel_delta_threshold=0,
                          edge_threshold=100)),
        PixelTestPage('pixel_webgl_aa_alpha.html',
                      base_name + '_WebGLGreenTriangle_AA_Alpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_noaa_alpha.html',
                      base_name + '_WebGLGreenTriangle_NoAA_Alpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_aa_noalpha.html',
                      base_name + '_WebGLGreenTriangle_AA_NoAlpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_noaa_noalpha.html',
                      base_name + '_WebGLGreenTriangle_NoAA_NoAlpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_noalpha_implicit_clear.html',
                      base_name +
                      '_WebGLTransparentGreenTriangle_NoAlpha_ImplicitClear',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_sad_canvas.html',
                      base_name + '_WebGLSadCanvas',
                      test_rect=[0, 0, 300, 300],
                      optional_action='CrashGpuProcess'),
        PixelTestPage('pixel_scissor.html',
                      base_name + '_ScissorTestWithPreserveDrawingBuffer',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_canvas2d_webgl.html',
                      base_name + '_2DCanvasWebGL',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_background.html',
                      base_name + '_SolidColorBackground',
                      test_rect=[500, 500, 100, 100]),
        PixelTestPage(
            'pixel_video_mp4.html',
            base_name + '_Video_MP4',
            test_rect=[0, 0, 240, 135],
            # Most images are actually very similar, but Pixel 2
            # tends to produce images with all colors shifted by a
            # small amount.
            matching_algorithm=general_mp4_algo),
        # Surprisingly stable, does not appear to require inexact matching.
        PixelTestPage('pixel_video_mp4.html',
                      base_name + '_Video_MP4_DXVA',
                      browser_args=[cba.DISABLE_FEATURES_D3D11_VIDEO_DECODER],
                      test_rect=[0, 0, 240, 135]),
        PixelTestPage('pixel_video_mp4_four_colors_aspect_4x3.html',
                      base_name + '_Video_MP4_FourColors_Aspect_4x3',
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=41700,
                          pixel_delta_threshold=15,
                          edge_threshold=40)),
        PixelTestPage('pixel_video_mp4_four_colors_rot_90.html',
                      base_name + '_Video_MP4_FourColors_Rot_90',
                      test_rect=[0, 0, 270, 240],
                      matching_algorithm=general_mp4_algo),
        PixelTestPage('pixel_video_mp4_four_colors_rot_180.html',
                      base_name + '_Video_MP4_FourColors_Rot_180',
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=general_mp4_algo),
        PixelTestPage('pixel_video_mp4_four_colors_rot_270.html',
                      base_name + '_Video_MP4_FourColors_Rot_270',
                      test_rect=[0, 0, 270, 240],
                      matching_algorithm=general_mp4_algo),
        PixelTestPage('pixel_video_mp4_rounded_corner.html',
                      base_name + '_Video_MP4_Rounded_Corner',
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=30500,
                          pixel_delta_threshold=15,
                          edge_threshold=70)),
        PixelTestPage('pixel_video_vp9.html',
                      base_name + '_Video_VP9',
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=114000,
                          pixel_delta_threshold=30,
                          edge_threshold=20)),
        PixelTestPage('pixel_video_vp9.html',
                      base_name + '_Video_VP9_DXVA',
                      browser_args=[cba.DISABLE_FEATURES_D3D11_VIDEO_DECODER],
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=31100,
                          pixel_delta_threshold=30,
                          edge_threshold=250)),
        PixelTestPage(
            'pixel_video_media_stream_incompatible_stride.html',
            base_name + '_Video_Media_Stream_Incompatible_Stride',
            browser_args=GetMediaStreamTestBrowserArgs(
                'media/test/data/four-colors-incompatible-stride.y4m'),
            test_rect=[0, 0, 240, 135],
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),

        # The MP4 contains H.264 which is primarily hardware decoded on bots.
        PixelTestPage(
            'pixel_video_context_loss.html?src='
            '/media/test/data/four-colors.mp4',
            base_name + '_Video_Context_Loss_MP4',
            test_rect=[0, 0, 240, 135],
            # Optimizer script spat out a value of 255 for the Sobel edge
            # threshold, so use fuzzy for now since it's slightly more
            # efficient.
            matching_algorithm=algo.FuzzyMatchingAlgorithm(
                max_different_pixels=31700, pixel_delta_threshold=20),
            expected_per_process_crashes={
                CRASH_TYPE_GPU: 1,
            }),

        # The VP9 test clip is primarily software decoded on bots.
        PixelTestPage(('pixel_video_context_loss.html'
                       '?src=/media/test/data/four-colors-vp9.webm'),
                      base_name + '_Video_Context_Loss_VP9',
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=54400,
                          pixel_delta_threshold=30,
                          edge_threshold=250),
                      expected_per_process_crashes={
                          CRASH_TYPE_GPU: 1,
                      }),
        PixelTestPage('pixel_video_backdrop_filter.html',
                      base_name + '_Video_BackdropFilter',
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=1000,
                          pixel_delta_threshold=20,
                          edge_threshold=40,
                          ignored_border_thickness=1)),
        PixelTestPage('pixel_webgl_premultiplied_alpha_false.html',
                      base_name + '_WebGL_PremultipliedAlpha_False',
                      test_rect=[0, 0, 150, 150]),
        PixelTestPage('pixel_webgl2_blitframebuffer_result_displayed.html',
                      base_name + '_WebGL2_BlitFramebuffer_Result_Displayed',
                      test_rect=[0, 0, 200, 200]),
        PixelTestPage('pixel_webgl2_clearbufferfv_result_displayed.html',
                      base_name + '_WebGL2_ClearBufferfv_Result_Displayed',
                      test_rect=[0, 0, 200, 200]),
        PixelTestPage('pixel_repeated_webgl_to_2d.html',
                      base_name + '_RepeatedWebGLTo2D',
                      test_rect=[0, 0, 256, 256]),
        PixelTestPage('pixel_repeated_webgl_to_2d.html',
                      base_name + '_RepeatedWebGLTo2D_SoftwareCompositing',
                      test_rect=[0, 0, 256, 256],
                      browser_args=sw_compositing_args),
        PixelTestPage('pixel_canvas2d_tab_switch.html',
                      base_name + '_Canvas2DTabSwitch',
                      test_rect=[0, 0, 100, 100],
                      optional_action='SwitchTabs'),
        PixelTestPage('pixel_canvas2d_tab_switch.html',
                      base_name + '_Canvas2DTabSwitch_SoftwareCompositing',
                      test_rect=[0, 0, 100, 100],
                      browser_args=sw_compositing_args,
                      optional_action='SwitchTabs'),
        PixelTestPage('pixel_webgl_copy_image.html',
                      base_name + '_WebGLCopyImage',
                      test_rect=[0, 0, 200, 100]),
        PixelTestPage('pixel_webgl_read_pixels_tab_switch.html',
                      base_name + '_WebGLReadPixelsTabSwitch',
                      test_rect=[0, 0, 100, 100],
                      optional_action='SwitchTabs'),
        PixelTestPage('pixel_webgl_read_pixels_tab_switch.html',
                      base_name +
                      '_WebGLReadPixelsTabSwitch_SoftwareCompositing',
                      test_rect=[0, 0, 100, 100],
                      browser_args=sw_compositing_args,
                      optional_action='SwitchTabs'),
        PixelTestPage('pixel_offscreen_canvas_ibrc_webgl_main.html',
                      base_name + '_OffscreenCanvasIBRCWebGLMain',
                      test_rect=[0, 0, 300, 300],
                      optional_action='RunOffscreenCanvasIBRCWebGLTest'),
        PixelTestPage('pixel_offscreen_canvas_ibrc_webgl_worker.html',
                      base_name + '_OffscreenCanvasIBRCWebGLWorker',
                      test_rect=[0, 0, 300, 300],
                      optional_action='RunOffscreenCanvasIBRCWebGLTest'),
    ]

  # Pages that should be run with GPU rasterization enabled.
  @staticmethod
  def GpuRasterizationPages(base_name):
    browser_args = [
        cba.ENABLE_GPU_RASTERIZATION,
        cba.DISABLE_SOFTWARE_COMPOSITING_FALLBACK,
    ]
    return [
        PixelTestPage('pixel_background.html',
                      base_name + '_GpuRasterization_BlueBox',
                      test_rect=[0, 0, 220, 220],
                      browser_args=browser_args),
        PixelTestPage('concave_paths.html',
                      base_name + '_GpuRasterization_ConcavePaths',
                      test_rect=[0, 0, 100, 100],
                      browser_args=browser_args),
        PixelTestPage('pixel_precision_rounded_corner.html',
                      base_name + '_PrecisionRoundedCorner',
                      test_rect=[0, 0, 400, 400],
                      browser_args=browser_args,
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=10,
                          pixel_delta_threshold=30,
                          edge_threshold=100)),
    ]

  # Pages that should be run with off-thread paint worklet flags.
  @staticmethod
  def PaintWorkletPages(base_name):
    browser_args = [
        '--enable-blink-features=OffMainThreadCSSPaint',
        '--enable-gpu-rasterization', '--enable-oop-rasterization'
    ]

    return [
        PixelTestPage(
            'pixel_paintWorklet_transform.html',
            base_name + '_PaintWorkletTransform',
            test_rect=[0, 0, 200, 200],
            browser_args=browser_args),
    ]

  # Pages that should be run with experimental canvas features.
  @staticmethod
  def ExperimentalCanvasFeaturesPages(base_name):
    browser_args = [
        cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
    ]
    accelerated_args = [
        cba.DISABLE_SOFTWARE_COMPOSITING_FALLBACK,
    ]
    unaccelerated_args = [
        cba.DISABLE_ACCELERATED_2D_CANVAS,
        cba.DISABLE_GPU_COMPOSITING,
    ]

    return [
        PixelTestPage('pixel_offscreenCanvas_transfer_after_style_resize.html',
                      base_name + '_OffscreenCanvasTransferAfterStyleResize',
                      test_rect=[0, 0, 350, 350],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_transfer_before_style_resize.html',
                      base_name + '_OffscreenCanvasTransferBeforeStyleResize',
                      test_rect=[0, 0, 350, 350],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_webgl_paint_after_resize.html',
                      base_name + '_OffscreenCanvasWebGLPaintAfterResize',
                      test_rect=[0, 0, 200, 200],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_transferToImageBitmap_main.html',
                      base_name + '_OffscreenCanvasTransferToImageBitmap',
                      test_rect=[0, 0, 300, 300],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_transferToImageBitmap_worker.html',
                      base_name + '_OffscreenCanvasTransferToImageBitmapWorker',
                      test_rect=[0, 0, 300, 300],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_webgl_commit_main.html',
                      base_name + '_OffscreenCanvasWebGLDefault',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_webgl_commit_worker.html',
                      base_name + '_OffscreenCanvasWebGLDefaultWorker',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_webgl_commit_main.html',
                      base_name + '_OffscreenCanvasWebGLSoftwareCompositing',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args +
                      [cba.DISABLE_GPU_COMPOSITING]),
        PixelTestPage(
            'pixel_offscreenCanvas_webgl_commit_worker.html',
            base_name + '_OffscreenCanvasWebGLSoftwareCompositingWorker',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + [cba.DISABLE_GPU_COMPOSITING]),
        PixelTestPage('pixel_offscreenCanvas_2d_commit_main.html',
                      base_name + '_OffscreenCanvasAccelerated2D',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args + accelerated_args),
        PixelTestPage('pixel_offscreenCanvas_2d_commit_worker.html',
                      base_name + '_OffscreenCanvasAccelerated2DWorker',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args + accelerated_args),
        PixelTestPage('pixel_offscreenCanvas_2d_commit_main.html',
                      base_name + '_OffscreenCanvasUnaccelerated2D',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args + unaccelerated_args),
        PixelTestPage('pixel_offscreenCanvas_2d_commit_worker.html',
                      base_name + '_OffscreenCanvasUnaccelerated2DWorker',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args + unaccelerated_args),
        PixelTestPage(
            'pixel_offscreenCanvas_2d_commit_main.html',
            base_name + '_OffscreenCanvasUnaccelerated2DGPUCompositing',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + [cba.DISABLE_ACCELERATED_2D_CANVAS]),
        PixelTestPage(
            'pixel_offscreenCanvas_2d_commit_worker.html',
            base_name + '_OffscreenCanvasUnaccelerated2DGPUCompositingWorker',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + [cba.DISABLE_ACCELERATED_2D_CANVAS]),
        PixelTestPage('pixel_offscreenCanvas_2d_resize_on_worker.html',
                      base_name + '_OffscreenCanvas2DResizeOnWorker',
                      test_rect=[0, 0, 200, 200],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_webgl_resize_on_worker.html',
                      base_name + '_OffscreenCanvasWebglResizeOnWorker',
                      test_rect=[0, 0, 200, 200],
                      browser_args=browser_args),
        PixelTestPage('pixel_canvas_display_srgb.html',
                      base_name + '_CanvasDisplaySRGBAccelerated2D',
                      test_rect=[0, 0, 140, 140],
                      browser_args=browser_args + accelerated_args),
        PixelTestPage('pixel_canvas_display_srgb.html',
                      base_name + '_CanvasDisplaySRGBUnaccelerated2D',
                      test_rect=[0, 0, 140, 140],
                      browser_args=browser_args + unaccelerated_args),
        PixelTestPage(
            'pixel_canvas_display_srgb.html',
            base_name + '_CanvasDisplaySRGBUnaccelerated2DGPUCompositing',
            test_rect=[0, 0, 140, 140],
            browser_args=browser_args + [cba.DISABLE_ACCELERATED_2D_CANVAS]),
    ]

  @staticmethod
  def LowLatencyPages(base_name):
    unaccelerated_args = [
        cba.DISABLE_ACCELERATED_2D_CANVAS,
        cba.DISABLE_GPU_COMPOSITING,
    ]
    return [
        PixelTestPage('pixel_canvas_low_latency_2d.html',
                      base_name + '_CanvasLowLatency2D',
                      test_rect=[0, 0, 100, 100]),
        PixelTestPage('pixel_canvas_low_latency_2d.html',
                      base_name + '_CanvasUnacceleratedLowLatency2D',
                      test_rect=[0, 0, 100, 100],
                      browser_args=unaccelerated_args),
        PixelTestPage('pixel_canvas_low_latency_webgl.html',
                      base_name + '_CanvasLowLatencyWebGL',
                      test_rect=[0, 0, 200, 200]),
        PixelTestPage('pixel_canvas_low_latency_webgl_alpha_false.html',
                      base_name + '_CanvasLowLatencyWebGLAlphaFalse',
                      test_rect=[0, 0, 200, 200]),
        PixelTestPage('pixel_canvas_low_latency_2d_draw_image.html',
                      base_name + '_CanvasLowLatency2DDrawImage',
                      test_rect=[0, 0, 200, 100]),
        PixelTestPage('pixel_canvas_low_latency_webgl_draw_image.html',
                      base_name + '_CanvasLowLatencyWebGLDrawImage',
                      test_rect=[0, 0, 200, 100]),
        PixelTestPage('pixel_canvas_low_latency_2d_image_data.html',
                      base_name + '_CanvasLowLatency2DImageData',
                      test_rect=[0, 0, 200, 100]),
        PixelTestPage('pixel_canvas_low_latency_webgl_rounded_corners.html',
                      base_name + '_CanvasLowLatencyWebGLRoundedCorners',
                      test_rect=[0, 0, 100, 100],
                      other_args={'no_overlay': True}),
        PixelTestPage('pixel_canvas_low_latency_webgl_occluded.html',
                      base_name + '_CanvasLowLatencyWebGLOccluded',
                      test_rect=[0, 0, 100, 100],
                      other_args={'no_overlay': True}),
    ]

  # Only add these tests on platforms where SwiftShader is enabled.
  # Currently this is Windows and Linux.
  @staticmethod
  def SwiftShaderPages(base_name):
    browser_args = [cba.DISABLE_GPU]
    suffix = "_SwiftShader"
    return [
        PixelTestPage('pixel_canvas2d.html',
                      base_name + '_Canvas2DRedBox' + suffix,
                      test_rect=[0, 0, 300, 300],
                      browser_args=browser_args),
        PixelTestPage('pixel_css3d.html',
                      base_name + '_CSS3DBlueBox' + suffix,
                      test_rect=[0, 0, 300, 300],
                      browser_args=browser_args),
        PixelTestPage('pixel_webgl_aa_alpha.html',
                      base_name + '_WebGLGreenTriangle_AA_Alpha' + suffix,
                      test_rect=[0, 0, 300, 300],
                      browser_args=browser_args),
        PixelTestPage('pixel_repeated_webgl_to_2d.html',
                      base_name + '_RepeatedWebGLTo2D' + suffix,
                      test_rect=[0, 0, 256, 256],
                      browser_args=browser_args),
    ]

  # Test rendering where GPU process is blocked.
  @staticmethod
  def NoGpuProcessPages(base_name):
    browser_args = [cba.DISABLE_GPU, cba.DISABLE_SOFTWARE_RASTERIZER]
    suffix = "_NoGpuProcess"
    return [
        PixelTestPage(
            'pixel_canvas2d.html',
            base_name + '_Canvas2DRedBox' + suffix,
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args,
            gpu_process_disabled=True),
        PixelTestPage(
            'pixel_css3d.html',
            base_name + '_CSS3DBlueBox' + suffix,
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args,
            gpu_process_disabled=True),
    ]

  # Pages that should be run with various macOS specific command line
  # arguments.
  @staticmethod
  def MacSpecificPages(base_name):
    iosurface_2d_canvas_args = ['--enable-accelerated-2d-canvas']

    non_chromium_image_args = ['--disable-webgl-image-chromium']

    # This disables the Core Animation compositor, falling back to the
    # old GLRenderer path, but continuing to allocate IOSurfaces for
    # WebGL's back buffer.
    no_overlays_args = ['--disable-mac-overlays']

    # The filter effect tests produce images with lots of gradients and blurs
    # which don't play nicely with Sobel filters, so a fuzzy algorithm instead
    # of Sobel. The images are also relatively large (360k pixels), and large
    # portions of the image are prone to noise, hence the large max different
    # pixels value.
    filter_effect_fuzzy_algo = algo.FuzzyMatchingAlgorithm(
        max_different_pixels=57500, pixel_delta_threshold=10)

    return [
        # On macOS, test the IOSurface 2D Canvas compositing path.
        PixelTestPage('pixel_canvas2d_accelerated.html',
                      base_name + '_IOSurface2DCanvas',
                      test_rect=[0, 0, 400, 400],
                      browser_args=iosurface_2d_canvas_args),
        PixelTestPage('pixel_canvas2d_webgl.html',
                      base_name + '_IOSurface2DCanvasWebGL',
                      test_rect=[0, 0, 300, 300],
                      browser_args=iosurface_2d_canvas_args),

        # On macOS, test WebGL non-Chromium Image compositing path.
        PixelTestPage('pixel_webgl_aa_alpha.html',
                      base_name +
                      '_WebGLGreenTriangle_NonChromiumImage_AA_Alpha',
                      test_rect=[0, 0, 300, 300],
                      browser_args=non_chromium_image_args),
        PixelTestPage('pixel_webgl_noaa_alpha.html',
                      base_name +
                      '_WebGLGreenTriangle_NonChromiumImage_NoAA_Alpha',
                      test_rect=[0, 0, 300, 300],
                      browser_args=non_chromium_image_args),
        PixelTestPage('pixel_webgl_aa_noalpha.html',
                      base_name +
                      '_WebGLGreenTriangle_NonChromiumImage_AA_NoAlpha',
                      test_rect=[0, 0, 300, 300],
                      browser_args=non_chromium_image_args),
        PixelTestPage('pixel_webgl_noaa_noalpha.html',
                      base_name +
                      '_WebGLGreenTriangle_NonChromiumImage_NoAA_NoAlpha',
                      test_rect=[0, 0, 300, 300],
                      browser_args=non_chromium_image_args),

        # On macOS, test CSS filter effects with and without the CA compositor.
        PixelTestPage('filter_effects.html',
                      base_name + '_CSSFilterEffects',
                      test_rect=[0, 0, 300, 300],
                      matching_algorithm=filter_effect_fuzzy_algo),
        PixelTestPage('filter_effects.html',
                      base_name + '_CSSFilterEffects_NoOverlays',
                      test_rect=[0, 0, 300, 300],
                      browser_args=no_overlays_args,
                      matching_algorithm=filter_effect_fuzzy_algo),

        # Test WebGL's premultipliedAlpha:false without the CA compositor.
        PixelTestPage('pixel_webgl_premultiplied_alpha_false.html',
                      base_name + '_WebGL_PremultipliedAlpha_False_NoOverlays',
                      test_rect=[0, 0, 150, 150],
                      browser_args=no_overlays_args),
    ]

  # Pages that should be run only on dual-GPU MacBook Pros (at the
  # present time, anyway).
  @staticmethod
  def DualGPUMacSpecificPages(base_name):
    return [
        PixelTestPage('pixel_webgl_high_to_low_power.html',
                      base_name + '_WebGLHighToLowPower',
                      test_rect=[0, 0, 300, 300],
                      optional_action='RunTestWithHighPerformanceTab'),
        PixelTestPage('pixel_webgl_low_to_high_power.html',
                      base_name + '_WebGLLowToHighPower',
                      test_rect=[0, 0, 300, 300],
                      optional_action='RunLowToHighPowerTest'),
        PixelTestPage('pixel_webgl_low_to_high_power_alpha_false.html',
                      base_name + '_WebGLLowToHighPowerAlphaFalse',
                      test_rect=[0, 0, 300, 300],
                      optional_action='RunLowToHighPowerTest'),
        PixelTestPage(
            'pixel_offscreen_canvas_ibrc_webgl_main.html',
            base_name + '_OffscreenCanvasIBRCWebGLHighPerfMain',
            test_rect=[0, 0, 300, 300],
            optional_action='RunOffscreenCanvasIBRCWebGLHighPerfTest'),
        PixelTestPage(
            'pixel_offscreen_canvas_ibrc_webgl_worker.html',
            base_name + '_OffscreenCanvasIBRCWebGLHighPerfWorker',
            test_rect=[0, 0, 300, 300],
            optional_action='RunOffscreenCanvasIBRCWebGLHighPerfTest'),
    ]

  @staticmethod
  def DirectCompositionPages(base_name):
    browser_args = [
        '--enable-direct-composition-video-overlays',
        # All bots are connected with a power source, however, we want to to
        # test with the code path that's enabled with battery power.
        cba.DISABLE_VP_SCALING,
    ]
    browser_args_YUY2 = browser_args + [
        '--disable-features=DirectCompositionPreferNV12Overlays'
    ]
    browser_args_DXVA = browser_args + [
        cba.DISABLE_FEATURES_D3D11_VIDEO_DECODER
    ]

    # Most tests fall roughly into 3 tiers of noisiness.
    # Parameter values were determined using the automated optimization script,
    # and similar values combined into a single set using the most permissive
    # value for each parameter in that tier.
    strict_dc_sobel_algorithm = algo.SobelMatchingAlgorithm(
        max_different_pixels=1000,
        pixel_delta_threshold=5,
        edge_threshold=250,
        ignored_border_thickness=1)
    permissive_dc_sobel_algorithm = algo.SobelMatchingAlgorithm(
        max_different_pixels=16800,
        pixel_delta_threshold=20,
        edge_threshold=30,
        ignored_border_thickness=1)
    very_permissive_dc_sobel_algorithm = algo.SobelMatchingAlgorithm(
        max_different_pixels=30400,
        pixel_delta_threshold=45,
        edge_threshold=10,
        ignored_border_thickness=1,
    )

    return [
        PixelTestPage('pixel_video_mp4.html',
                      base_name + '_DirectComposition_Video_MP4',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4.html',
                      base_name + '_DirectComposition_Video_MP4_DXVA',
                      browser_args=browser_args_DXVA,
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4_fullsize.html',
                      base_name + '_DirectComposition_Video_MP4_Fullsize',
                      browser_args=browser_args,
                      test_rect=[0, 0, 960, 540],
                      other_args={'zero_copy': True},
                      matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4.html',
                      base_name + '_DirectComposition_Video_MP4_YUY2',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args_YUY2,
                      other_args={'expect_yuy2': True},
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4_four_colors_aspect_4x3.html',
                      base_name +
                      '_DirectComposition_Video_MP4_FourColors_Aspect_4x3',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4_four_colors_rot_90.html',
                      base_name +
                      '_DirectComposition_Video_MP4_FourColors_Rot_90',
                      test_rect=[0, 0, 270, 240],
                      browser_args=browser_args,
                      other_args={'video_is_rotated': True},
                      matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4_four_colors_rot_180.html',
                      base_name +
                      '_DirectComposition_Video_MP4_FourColors_Rot_180',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      other_args={'video_is_rotated': True},
                      matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4_four_colors_rot_270.html',
                      base_name +
                      '_DirectComposition_Video_MP4_FourColors_Rot_270',
                      test_rect=[0, 0, 270, 240],
                      browser_args=browser_args,
                      other_args={'video_is_rotated': True},
                      matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage('pixel_video_vp9.html',
                      base_name + '_DirectComposition_Video_VP9',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      matching_algorithm=very_permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_vp9.html',
                      base_name + '_DirectComposition_Video_VP9_DXVA',
                      browser_args=browser_args_DXVA,
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=very_permissive_dc_sobel_algorithm),
        PixelTestPage(
            'pixel_video_vp9_fullsize.html',
            base_name + '_DirectComposition_Video_VP9_Fullsize',
            test_rect=[0, 0, 960, 540],
            browser_args=browser_args,
            other_args={'zero_copy': True},
            # Much larger image than other VP9 tests.
            matching_algorithm=algo.SobelMatchingAlgorithm(
                max_different_pixels=504000,
                pixel_delta_threshold=10,
                edge_threshold=10,
                ignored_border_thickness=1,
            )),
        PixelTestPage('pixel_video_vp9.html',
                      base_name + '_DirectComposition_Video_VP9_YUY2',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args_YUY2,
                      other_args={'expect_yuy2': True},
                      matching_algorithm=very_permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_vp9_i420a.html',
                      base_name + '_DirectComposition_Video_VP9_I420A',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      other_args={'no_overlay': True},
                      matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage('pixel_video_underlay.html',
                      base_name + '_DirectComposition_Underlay',
                      test_rect=[0, 0, 240, 136],
                      browser_args=browser_args,
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_underlay.html',
                      base_name + '_DirectComposition_Underlay_DXVA',
                      test_rect=[0, 0, 240, 136],
                      browser_args=browser_args_DXVA,
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_underlay_fullsize.html',
                      base_name + '_DirectComposition_Underlay_Fullsize',
                      test_rect=[0, 0, 960, 540],
                      browser_args=browser_args,
                      other_args={'zero_copy': True},
                      matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4_rounded_corner.html',
                      base_name + '_DirectComposition_Video_MP4_Rounded_Corner',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      other_args={'no_overlay': True}),
        PixelTestPage('pixel_video_backdrop_filter.html',
                      base_name + '_DirectComposition_Video_BackdropFilter',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      other_args={'no_overlay': True}),
        PixelTestPage(
            'pixel_video_mp4.html',
            base_name + '_DirectComposition_Video_Disable_Overlays',
            test_rect=[0, 0, 240, 135],
            browser_args=['--disable-direct-composition-video-overlays'],
            other_args={'no_overlay': True},
            matching_algorithm=very_permissive_dc_sobel_algorithm),
    ]

  @staticmethod
  def HdrTestPages(base_name):
    return [
        PixelTestPage(
            'pixel_canvas2d.html',
            base_name + '_Canvas2DRedBoxScrgbLinear',
            test_rect=[0, 0, 300, 300],
            browser_args=['--force-color-profile=scrgb-linear']),
        PixelTestPage(
            'pixel_canvas2d.html',
            base_name + '_Canvas2DRedBoxHdr10',
            test_rect=[0, 0, 300, 300],
            browser_args=['--force-color-profile=hdr10']),
    ]

  @staticmethod
  def ForceFullDamagePages(base_name):
    return [
        PixelTestPage('wait_for_compositing.html',
                      base_name + '_ForceFullDamage',
                      test_rect=[0, 0, 0, 0],
                      other_args={'full_damage': True},
                      browser_args=[cba.ENABLE_FORCE_FULL_DAMAGE]),
        PixelTestPage('wait_for_compositing.html',
                      base_name + '_ForcePartialDamage',
                      test_rect=[0, 0, 0, 0],
                      other_args={'full_damage': False},
                      browser_args=[cba.DISABLE_FORCE_FULL_DAMAGE]),
    ]

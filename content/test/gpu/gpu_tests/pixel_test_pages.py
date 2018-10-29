# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class PixelTestPage(object):
  """A wrapper class mimicking the functionality of the PixelTestsStorySet
  from the old-style GPU tests.
  """
  def __init__(self, url, name, test_rect, revision,
               tolerance=2, browser_args=None, expected_colors=None,
               gpu_process_disabled=False, optional_action=None):
    super(PixelTestPage, self).__init__()
    self.url = url
    self.name = name
    self.test_rect = test_rect
    self.revision = revision
    # The tolerance when comparing against the reference image.
    self.tolerance = tolerance
    self.browser_args = browser_args
    # The expected colors can be specified as a list of dictionaries,
    # in which case these specific pixels will be sampled instead of
    # comparing the entire image snapshot. The format is only defined
    # by contract with _CompareScreenshotSamples in
    # cloud_storage_integration_test_base.py.
    self.expected_colors = expected_colors
    # Only a couple of tests run with the GPU process completely
    # disabled. To prevent regressions, only allow the GPU information
    # to be incomplete in these cases.
    self.gpu_process_disabled = gpu_process_disabled
    # One of the tests (WebGLSadCanvas) requires custom actions to
    # be run. These are specified as a string which is the name of a
    # method to call in PixelIntegrationTest. For example if the
    # action here is "CrashGpuProcess" then it would be defined in a
    # "_CrashGpuProcess" method in PixelIntegrationTest.
    self.optional_action = optional_action

  def CopyWithNewBrowserArgsAndSuffix(self, browser_args, suffix):
    return PixelTestPage(
      self.url, self.name + suffix, self.test_rect, self.revision,
      self.tolerance, browser_args, self.expected_colors)

  def CopyWithNewBrowserArgsAndPrefix(self, browser_args, prefix):
    # Assuming the test name is 'Pixel'.
    split = self.name.split('_', 1)
    return PixelTestPage(
      self.url, split[0] + '_' + prefix + split[1], self.test_rect,
      self.revision, self.tolerance, browser_args, self.expected_colors)


def CopyPagesWithNewBrowserArgsAndSuffix(pages, browser_args, suffix):
  return [
    p.CopyWithNewBrowserArgsAndSuffix(browser_args, suffix) for p in pages]


def CopyPagesWithNewBrowserArgsAndPrefix(pages, browser_args, prefix):
  return [
    p.CopyWithNewBrowserArgsAndPrefix(browser_args, prefix) for p in pages]


# TODO(kbr): consider refactoring this into pixel_integration_test.py.
SCALE_FACTOR_OVERRIDES = {
  "comment": "scale factor overrides",
  "scale_factor_overrides": [
    {
      "device_type": "Nexus 5",
      "scale_factor": 1.105
    },
    {
      "device_type": "Nexus 5X",
      "scale_factor": 1.105
    },
    {
      "device_type": "Nexus 6",
      "scale_factor": 1.47436
    },
    {
      "device_type": "Nexus 6P",
      "scale_factor": 1.472
    },
    {
      "device_type": "Nexus 9",
      "scale_factor": 1.566
    },
    {
      "comment": "NVIDIA Shield",
      "device_type": "sb_na_wf",
      "scale_factor": 1.226
    }
  ]
}


def DefaultPages(base_name):
  return [
    PixelTestPage(
      'pixel_background_image.html',
      base_name + '_BackgroundImage',
      test_rect=[20, 20, 370, 370],
      revision=1),

    PixelTestPage(
      'pixel_canvas2d.html',
      base_name + '_Canvas2DRedBox',
      test_rect=[0, 0, 300, 300],
      revision=10),

    PixelTestPage(
      'pixel_canvas2d_untagged.html',
      base_name + '_Canvas2DUntagged',
      test_rect=[0, 0, 257, 257],
      revision=0),

    PixelTestPage(
      'pixel_css3d.html',
      base_name + '_CSS3DBlueBox',
      test_rect=[0, 0, 300, 300],
      revision=23),

    PixelTestPage(
      'pixel_webgl_aa_alpha.html',
      base_name + '_WebGLGreenTriangle_AA_Alpha',
      test_rect=[0, 0, 300, 300],
      revision=7),

    PixelTestPage(
      'pixel_webgl_noaa_alpha.html',
      base_name + '_WebGLGreenTriangle_NoAA_Alpha',
      test_rect=[0, 0, 300, 300],
      revision=4),

    PixelTestPage(
      'pixel_webgl_aa_noalpha.html',
      base_name + '_WebGLGreenTriangle_AA_NoAlpha',
      test_rect=[0, 0, 300, 300],
      revision=8),

    PixelTestPage(
      'pixel_webgl_noaa_noalpha.html',
      base_name + '_WebGLGreenTriangle_NoAA_NoAlpha',
      test_rect=[0, 0, 300, 300],
      revision=4),

    PixelTestPage(
      'pixel_webgl_noalpha_implicit_clear.html',
      base_name + '_WebGLTransparentGreenTriangle_NoAlpha_ImplicitClear',
      test_rect=[0, 0, 300, 300],
      revision=4),

    PixelTestPage(
      'pixel_webgl_sad_canvas.html',
      base_name + '_WebGLSadCanvas',
      test_rect=[0, 0, 300, 300],
      revision=1,
      optional_action='CrashGpuProcess'),

    PixelTestPage(
      'pixel_scissor.html',
      base_name + '_ScissorTestWithPreserveDrawingBuffer',
      test_rect=[0, 0, 300, 300],
      revision=0, # This is not used.
      expected_colors=[
        {
          'comment': 'red top',
          'location': [1, 1],
          'size': [198, 188],
          'color': [255, 0, 0],
          'tolerance': 3
        },
        {
          'comment': 'green bottom left',
          'location': [1, 191],
          'size': [8, 8],
          'color': [0, 255, 0],
          'tolerance': 3
        },
        {
          'comment': 'red bottom right',
          'location': [11, 191],
          'size': [188, 8],
          'color': [255, 0, 0],
          'tolerance': 3
        }
      ]),

    PixelTestPage(
      'pixel_canvas2d_webgl.html',
      base_name + '_2DCanvasWebGL',
      test_rect=[0, 0, 300, 300],
      revision=10),

    PixelTestPage(
      'pixel_background.html',
      base_name + '_SolidColorBackground',
      test_rect=[500, 500, 100, 100],
      revision=1),

    PixelTestPage(
      'pixel_video_mp4.html',
      base_name + '_Video_MP4',
      test_rect=[0, 0, 300, 300],
      revision=10),

    PixelTestPage(
      'pixel_video_vp9.html',
      base_name + '_Video_VP9',
      test_rect=[0, 0, 300, 300],
      revision=10),

    PixelTestPage(
      'pixel_webgl_premultiplied_alpha_false.html',
      base_name + '_WebGL_PremultipliedAlpha_False',
      test_rect=[0, 0, 150, 150],
      revision=0, # This is not used.
      expected_colors=[
        SCALE_FACTOR_OVERRIDES,
        {
          'comment': 'brown',
          'location': [1, 1],
          'size': [148, 148],
          # This is the color on an NVIDIA based MacBook Pro if the
          # sRGB profile's applied correctly.
          'color': [102, 77, 0],
          # This is the color if it isn't.
          # 'color': [101, 76, 12],
          'tolerance': 3
        },
      ]),

    PixelTestPage(
      'pixel_webgl2_blitframebuffer_result_displayed.html',
      base_name + '_WebGL2_BlitFramebuffer_Result_Displayed',
      test_rect=[0, 0, 200, 200],
      revision=0, # This is not used.
      expected_colors=[
        SCALE_FACTOR_OVERRIDES,
        {
          'comment': 'green',
          'location': [1, 1],
          'size': [180, 180],
          'color': [0, 255, 0],
          'tolerance': 3
        },
      ]),

    PixelTestPage(
      'pixel_webgl2_clearbufferfv_result_displayed.html',
      base_name + '_WebGL2_ClearBufferfv_Result_Displayed',
      test_rect=[0, 0, 200, 200],
      revision=0, # This is not used.
      expected_colors=[
        SCALE_FACTOR_OVERRIDES,
        {
          'comment': 'green',
          'location': [1, 1],
          'size': [180, 180],
          'color': [0, 255, 0],
          'tolerance': 3
        },
      ]),
  ]


# Pages that should be run with GPU rasterization enabled.
def GpuRasterizationPages(base_name):
  browser_args = ['--force-gpu-rasterization']
  return [
    PixelTestPage(
      'pixel_background.html',
      base_name + '_GpuRasterization_BlueBox',
      test_rect=[0, 0, 220, 220],
      revision=0, # This is not used.
      browser_args=browser_args,
      expected_colors=[
        {
          'comment': 'body-t',
          'location': [5, 5],
          'size': [1, 1],
          'color': [0, 128, 0],
          'tolerance': 0
        },
        {
          'comment': 'body-r',
          'location': [215, 5],
          'size': [1, 1],
          'color': [0, 128, 0],
          'tolerance': 0
        },
        {
          'comment': 'body-b',
          'location': [215, 215],
          'size': [1, 1],
          'color': [0, 128, 0],
          'tolerance': 0
        },
        {
          'comment': 'body-l',
          'location': [5, 215],
          'size': [1, 1],
          'color': [0, 128, 0],
          'tolerance': 0
        },
        {
          'comment': 'background-t',
          'location': [30, 30],
          'size': [1, 1],
          'color': [0, 0, 0],
          'tolerance': 0
        },
        {
          'comment': 'background-r',
          'location': [170, 30],
          'size': [1, 1],
          'color': [0, 0, 0],
          'tolerance': 0
        },
        {
          'comment': 'background-b',
          'location': [170, 170],
          'size': [1, 1],
          'color': [0, 0, 0],
          'tolerance': 0
        },
        {
          'comment': 'background-l',
          'location': [30, 170],
          'size': [1, 1],
          'color': [0, 0, 0],
          'tolerance': 0
        },
        {
          'comment': 'box-t',
          'location': [70, 70],
          'size': [1, 1],
          'color': [0, 0, 255],
          'tolerance': 0
        },
        {
          'comment': 'box-r',
          'location': [140, 70],
          'size': [1, 1],
          'color': [0, 0, 255],
          'tolerance': 0
        },
        {
          'comment': 'box-b',
          'location': [140, 140],
          'size': [1, 1],
          'color': [0, 0, 255],
          'tolerance': 0
        },
        {
          'comment': 'box-l',
          'location': [70, 140],
          'size': [1, 1],
          'color': [0, 0, 255],
          'tolerance': 0
        }
      ]),
    PixelTestPage(
      'concave_paths.html',
      base_name + '_GpuRasterization_ConcavePaths',
      test_rect=[0, 0, 100, 100],
      revision=0, # This is not used.
      browser_args=browser_args,
      expected_colors=[
        {
          'comment': 'outside',
          'location': [80, 60],
          'size': [1, 1],
          'color': [255, 255, 255],
          'tolerance': 0
        },
        {
          'comment': 'outside',
          'location': [28, 20],
          'size': [1, 1],
          'color': [255, 255, 255],
          'tolerance': 0
        },
        {
          'comment': 'inside',
          'location': [32, 25],
          'size': [1, 1],
          'color': [255, 215, 0],
          'tolerance': 0
        },
        {
          'comment': 'inside',
          'location': [80, 80],
          'size': [1, 1],
          'color': [255, 215, 0],
          'tolerance': 0
        }
      ])
  ]

# Pages that should be run with experimental canvas features.
def ExperimentalCanvasFeaturesPages(base_name):
  browser_args = [
    '--enable-experimental-web-platform-features'] # for lowLatency
  unaccelerated_args = [
    '--disable-accelerated-2d-canvas',
    '--disable-gpu-compositing']

  return [
    PixelTestPage(
      'pixel_offscreenCanvas_transfer_after_style_resize.html',
      base_name + '_OffscreenCanvasTransferAfterStyleResize',
      test_rect=[0, 0, 350, 350],
      revision=9,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_transfer_before_style_resize.html',
      base_name + '_OffscreenCanvasTransferBeforeStyleResize',
      test_rect=[0, 0, 350, 350],
      revision=9,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_webgl_paint_after_resize.html',
      base_name + '_OffscreenCanvasWebGLPaintAfterResize',
      test_rect=[0, 0, 200, 200],
      browser_args=browser_args,
      revision=0, # This is not used.
      expected_colors=[
        SCALE_FACTOR_OVERRIDES,
        {
          'comment': 'resized area',
          'location': [1, 1],
          'size': [48, 98],
          'color': [0, 255, 0],
          'tolerance': 0
        },
        {
          'comment': 'outside resized area',
          'location': [51, 1],
          'size': [48, 98],
          'color': [255, 255, 255],
          'tolerance': 0
        },
      ]),

    PixelTestPage(
      'pixel_offscreenCanvas_transferToImageBitmap_main.html',
      base_name + '_OffscreenCanvasTransferToImageBitmap',
      test_rect=[0, 0, 300, 300],
      revision=6,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_transferToImageBitmap_worker.html',
      base_name + '_OffscreenCanvasTransferToImageBitmapWorker',
      test_rect=[0, 0, 300, 300],
      revision=6,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_webgl_commit_main.html',
      base_name + '_OffscreenCanvasWebGLDefault',
      test_rect=[0, 0, 360, 200],
      revision=11,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_webgl_commit_worker.html',
      base_name + '_OffscreenCanvasWebGLDefaultWorker',
      test_rect=[0, 0, 360, 200],
      revision=11,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_webgl_commit_main.html',
      base_name + '_OffscreenCanvasWebGLSoftwareCompositing',
      test_rect=[0, 0, 360, 200],
      revision=7,
      browser_args=browser_args + ['--disable-gpu-compositing']),

    PixelTestPage(
      'pixel_offscreenCanvas_webgl_commit_worker.html',
      base_name + '_OffscreenCanvasWebGLSoftwareCompositingWorker',
      test_rect=[0, 0, 360, 200],
      revision=7,
      browser_args=browser_args + ['--disable-gpu-compositing']),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_main.html',
      base_name + '_OffscreenCanvasAccelerated2D',
      test_rect=[0, 0, 360, 200],
      revision=11,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_worker.html',
      base_name + '_OffscreenCanvasAccelerated2DWorker',
      test_rect=[0, 0, 360, 200],
      revision=11,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_main.html',
      base_name + '_OffscreenCanvasUnaccelerated2D',
      test_rect=[0, 0, 360, 200],
      revision=8,
      browser_args=browser_args + unaccelerated_args),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_worker.html',
      base_name + '_OffscreenCanvasUnaccelerated2DWorker',
      test_rect=[0, 0, 360, 200],
      revision=8,
      browser_args=browser_args + unaccelerated_args),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_main.html',
      base_name + '_OffscreenCanvasUnaccelerated2DGPUCompositing',
      test_rect=[0, 0, 360, 200],
      revision=13,
      browser_args=browser_args + ['--disable-accelerated-2d-canvas']),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_worker.html',
      base_name + '_OffscreenCanvasUnaccelerated2DGPUCompositingWorker',
      test_rect=[0, 0, 360, 200],
      revision=13,
      browser_args=browser_args + ['--disable-accelerated-2d-canvas']),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_resize_on_worker.html',
      base_name + '_OffscreenCanvas2DResizeOnWorker',
      test_rect=[0, 0, 200, 200],
      revision=7,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_webgl_resize_on_worker.html',
      base_name + '_OffscreenCanvasWebglResizeOnWorker',
      test_rect=[0, 0, 200, 200],
      revision=9,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_canvas_display_linear-rgb.html',
      base_name + '_CanvasDisplayLinearRGBAccelerated2D',
      test_rect=[0, 0, 140, 140],
      revision=5,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_canvas_display_linear-rgb.html',
      base_name + '_CanvasDisplayLinearRGBUnaccelerated2D',
      test_rect=[0, 0, 140, 140],
      revision=1,
      browser_args=browser_args + unaccelerated_args),

    PixelTestPage(
      'pixel_canvas_display_linear-rgb.html',
      base_name + '_CanvasDisplayLinearRGBUnaccelerated2DGPUCompositing',
      test_rect=[0, 0, 140, 140],
      revision=5,
      browser_args=browser_args + ['--disable-accelerated-2d-canvas']),

    PixelTestPage(
      'pixel_canvas_low_latency_2d.html',
      base_name + '_CanvasLowLatency2D',
      test_rect=[0, 0, 100, 100],
      revision=2,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_canvas_low_latency_2d.html',
      base_name + '_CanvasUnacceleratedLowLatency2D',
      test_rect=[0, 0, 100, 100],
      revision=2,
      browser_args=browser_args + unaccelerated_args),

    PixelTestPage(
      'pixel_canvas_low_latency_webgl.html',
      base_name + '_CanvasLowLatencyWebGL',
      test_rect=[0, 0, 200, 200],
      revision=0, # not used
      browser_args=browser_args,
      expected_colors=[
        SCALE_FACTOR_OVERRIDES,
        {
          'comment': 'green',
          'location': [1, 1],
          'size': [98, 98],
          'color': [0, 255, 0],
          'tolerance': 0
        },
      ]),
  ]

# Only add these tests on platforms where SwiftShader is enabled.
# Currently this is Windows and Linux.
def SwiftShaderPages(base_name):
  browser_args = ['--disable-gpu']
  suffix = "_SwiftShader"
  return [
    PixelTestPage(
      'pixel_canvas2d.html',
      base_name + '_Canvas2DRedBox' + suffix,
      test_rect=[0, 0, 300, 300],
      revision=1,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_css3d.html',
      base_name + '_CSS3DBlueBox' + suffix,
      test_rect=[0, 0, 300, 300],
      revision=1,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_webgl_aa_alpha.html',
      base_name + '_WebGLGreenTriangle_AA_Alpha' + suffix,
      test_rect=[0, 0, 300, 300],
      revision=2,
      browser_args=browser_args),
  ]

# Test rendering where GPU process is blocked.
def NoGpuProcessPages(base_name):
  browser_args = ['--disable-gpu', '--disable-software-rasterizer']
  suffix = "_NoGpuProcess"
  return [
    PixelTestPage(
      'pixel_canvas2d.html',
      base_name + '_Canvas2DRedBox' + suffix,
      test_rect=[0, 0, 300, 300],
      revision=2,
      browser_args=browser_args,
      gpu_process_disabled=True),

    PixelTestPage(
      'pixel_css3d.html',
      base_name + '_CSS3DBlueBox' + suffix,
      test_rect=[0, 0, 300, 300],
      revision=2,
      browser_args=browser_args,
      gpu_process_disabled=True),
  ]

# Pages that should be run with various macOS specific command line
# arguments.
def MacSpecificPages(base_name):
  iosurface_2d_canvas_args = [
    '--enable-accelerated-2d-canvas']

  non_chromium_image_args = ['--disable-webgl-image-chromium']

  # This disables the Core Animation compositor, falling back to the
  # old GLRenderer path, but continuing to allocate IOSurfaces for
  # WebGL's back buffer.
  no_overlays_args = ['--disable-mac-overlays']

  return [
    # On macOS, test the IOSurface 2D Canvas compositing path.
    PixelTestPage(
      'pixel_canvas2d_accelerated.html',
      base_name + '_IOSurface2DCanvas',
      test_rect=[0, 0, 400, 400],
      revision=1,
      browser_args=iosurface_2d_canvas_args),
    PixelTestPage(
      'pixel_canvas2d_webgl.html',
      base_name + '_IOSurface2DCanvasWebGL',
      test_rect=[0, 0, 300, 300],
      revision=4,
      browser_args=iosurface_2d_canvas_args),

    # On macOS, test WebGL non-Chromium Image compositing path.
    PixelTestPage(
      'pixel_webgl_aa_alpha.html',
      base_name + '_WebGLGreenTriangle_NonChromiumImage_AA_Alpha',
      test_rect=[0, 0, 300, 300],
      revision=3,
      browser_args=non_chromium_image_args),
    PixelTestPage(
      'pixel_webgl_noaa_alpha.html',
      base_name + '_WebGLGreenTriangle_NonChromiumImage_NoAA_Alpha',
      test_rect=[0, 0, 300, 300],
      revision=1,
      browser_args=non_chromium_image_args),
    PixelTestPage(
      'pixel_webgl_aa_noalpha.html',
      base_name + '_WebGLGreenTriangle_NonChromiumImage_AA_NoAlpha',
      test_rect=[0, 0, 300, 300],
      revision=3,
      browser_args=non_chromium_image_args),
    PixelTestPage(
      'pixel_webgl_noaa_noalpha.html',
      base_name + '_WebGLGreenTriangle_NonChromiumImage_NoAA_NoAlpha',
      test_rect=[0, 0, 300, 300],
      revision=1,
      browser_args=non_chromium_image_args),

    # On macOS, test CSS filter effects with and without the CA compositor.
    PixelTestPage(
      'filter_effects.html',
      base_name + '_CSSFilterEffects',
      test_rect=[0, 0, 300, 300],
      revision=8),
    PixelTestPage(
      'filter_effects.html',
      base_name + '_CSSFilterEffects_NoOverlays',
      test_rect=[0, 0, 300, 300],
      revision=9,
      tolerance=10,
      browser_args=no_overlays_args),

    # Test WebGL's premultipliedAlpha:false without the CA compositor.
    PixelTestPage(
      'pixel_webgl_premultiplied_alpha_false.html',
      base_name + '_WebGL_PremultipliedAlpha_False_NoOverlays',
      test_rect=[0, 0, 150, 150],
      revision=0, # This is not used.
      browser_args=no_overlays_args,
      expected_colors=[
        SCALE_FACTOR_OVERRIDES,
        {
          'comment': 'brown',
          'location': [1, 1],
          'size': [148, 148],
          # This is the color on an NVIDIA based MacBook Pro if the
          # sRGB profile's applied correctly.
          'color': [102, 77, 0],
          # This is the color if it isn't.
          # 'color': [101, 76, 12],
          'tolerance': 3
        },
      ]),
  ]

def DirectCompositionPages(base_name):
  browser_args = ['--enable-direct-composition-layers']
  browser_args_Underlay = browser_args + [
    '--enable-features=DirectCompositionUnderlays']
  browser_args_Nonroot = browser_args +[
    '--enable-features=DirectCompositionNonrootOverlays,' +
    'DirectCompositionUnderlays']
  browser_args_Complex = browser_args + [
    '--enable-features=DirectCompositionComplexOverlays,' +
    'DirectCompositionNonrootOverlays,' +
    'DirectCompositionUnderlays']
  return [
    PixelTestPage(
      'pixel_video_mp4.html',
      base_name + '_DirectComposition_Video_MP4',
      test_rect=[0, 0, 300, 300],
      revision=8,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_video_vp9.html',
      base_name + '_DirectComposition_Video_VP9',
      test_rect=[0, 0, 300, 300],
      revision=10,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_video_underlay.html',
      base_name + '_DirectComposition_Underlay',
      test_rect=[0, 0, 240, 136],
      revision=0, # Golden image revision is not used
      browser_args=browser_args_Underlay,
      expected_colors=[
        {
          'comment': 'black top left',
          'location': [4, 4],
          'size': [20, 20],
          'color': [0, 0, 0],
          'tolerance': 3
        },
        {
          'comment': 'yellow top left quadrant',
          'location': [4, 34],
          'size': [110, 30],
          'color': [255, 255, 15],
          'tolerance': 3
        },
        {
          'comment': 'red top right quadrant',
          'location': [124, 4],
          'size': [110, 60],
          'color': [255, 17, 24],
          'tolerance': 3
        },
        {
          'comment': 'blue bottom left quadrant',
          'location': [4, 72],
          'size': [110, 60],
          'color': [12, 12, 255],
          'tolerance': 3
        },
        {
          'comment': 'green bottom right quadrant',
          'location': [124, 72],
          'size': [110, 60],
          'color': [44, 255, 16],
          'tolerance': 3
        }
      ]),

    PixelTestPage(
      'pixel_video_nonroot.html',
      base_name + '_DirectComposition_Nonroot',
      test_rect=[0, 0, 240, 136],
      revision=0, # Golden image revision is not used
      browser_args=browser_args_Nonroot,
      expected_colors=[
        {
          'comment': 'black top left',
          'location': [4, 4],
          'size': [20, 20],
          'color': [0, 0, 0],
          'tolerance': 3
        },
        {
          'comment': 'yellow top left quadrant',
          'location': [4, 34],
          'size': [110, 30],
          'color': [255, 255, 15],
          'tolerance': 3
        },
        {
          'comment': 'red top right quadrant',
          'location': [124, 4],
          'size': [50, 60],
          'color': [255, 17, 24],
          'tolerance': 3
        },
        {
          'comment': 'blue bottom left quadrant',
          'location': [4, 72],
          'size': [110, 60],
          'color': [12, 12, 255],
          'tolerance': 3
        },
        {
          'comment': 'green bottom right quadrant',
          'location': [124, 72],
          'size': [50, 60],
          'color': [44, 255, 16],
          'tolerance': 3
        }
      ]),

    PixelTestPage(
      'pixel_video_complex_overlays.html',
      base_name + '_DirectComposition_ComplexOverlays',
      test_rect=[0, 0, 240, 136],
      revision=0, # Golden image revision is not used
      browser_args=browser_args_Complex,
      expected_colors=[
        {
          'comment': 'black top left',
          'location': [4, 4],
          'size': [20, 20],
          'color': [0, 0, 0],
          'tolerance': 3
        },
        {
          'comment': 'yellow top left quadrant',
          'location': [60, 10],
          'size': [65, 30],
          'color': [255, 255, 15],
          'tolerance': 3
        },
        {
          'comment': 'red top right quadrant',
          'location': [150, 45],
          'size': [65, 30],
          'color': [255, 17, 24],
          'tolerance': 3
        },
        {
          'comment': 'blue bottom left quadrant',
          'location': [30, 70],
          'size': [65, 30],
          'color': [12, 12, 255],
          'tolerance': 3
        },
        {
          'comment': 'green bottom right quadrant',
          'location': [130, 100],
          'size': [65, 30],
          'color': [44, 255, 16],
          'tolerance': 3
        }
      ]),
    ]

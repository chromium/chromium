# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import json
import itertools
from typing import Any
import unittest

import gpu_path_util
from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test
from gpu_tests.util import host_information

html_path = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'content', 'test',
                         'data', 'gpu', 'webcodecs')
data_path = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'media', 'test',
                         'data')
four_colors_img_path = os.path.join(data_path, 'four-colors.y4m')

frame_sources = [
    'camera', 'capture', 'offscreen', 'arraybuffer', 'hw_decoder', 'sw_decoder'
]
hbd_frame_sources = ['hbd_arraybuffer']
video_codecs = [
    'avc1.42001E', 'hvc1.1.6.L123.00', 'vp8', 'vp09.00.10.08', 'av01.0.04M.08'
]
accelerations = ['prefer-hardware', 'prefer-software']


class WebCodecsIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  @classmethod
  def Name(cls) -> str:
    return 'webcodecs'

  @classmethod
  def _SuiteSupportsParallelTests(cls) -> bool:
    return True

  def _GetSerialGlobs(self) -> set[str]:
    serial_globs = set()
    if host_information.IsWindows() and host_information.IsNvidiaGpu():
      serial_globs |= {
          # crbug.com/1473480. Windows + NVIDIA has a maximum parallel encode
          # limit of 2, so serialize hardware encoding tests on Windows.
          'WebCodecs_*prefer-hardware*',
      }
    return serial_globs

  def _GetSerialTests(self) -> set[str]:
    serial_tests = set()
    if host_information.IsWindows() and host_information.IsArmCpu():
      serial_tests |= {
          # crbug.com/323824490. Seems to flakily lose the D3D11 device when
          # run in parallel.
          'WebCodecs_FrameSizeChange_vp09.00.10.08_hw_decoder',
      }
    return serial_tests

# pylint: disable=too-many-branches

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    tests = itertools.chain(cls.GenerateFrameTests(), cls.GenerateVideoTests(),
                            cls.GenerateAudioTests(), cls.BitrateTests())
    yield from tests

  @classmethod
  def GenerateFrameTests(cls) -> ct.TestGenerator:
    for source_type in frame_sources:
      yield (
          f'WebCodecs_DrawImage_{source_type}',
          'draw-image.html',
          [{
              'source_type': source_type,
          }],
      )
      yield (
          f'WebCodecs_TexImage2d_{source_type}',
          'tex-image-2d.html',
          [{
              'source_type': source_type,
          }],
      )
      yield (
          f'WebCodecs_copyTo_{source_type}',
          'copyTo.html',
          [{
              'source_type': source_type,
          }],
      )
      yield (
          f'WebCodecs_convertToRGB_{source_type}',
          'convert-to-rgb.html',
          [{
              'source_type': source_type,
          }],
      )
    for source_type in hbd_frame_sources:
      yield (
          f'WebCodecs_DrawImage_{source_type}',
          'draw-image.html',
          [{
              'source_type': source_type,
          }],
      )

  @classmethod
  def GenerateAudioTests(cls) -> ct.TestGenerator:
    yield (
        'WebCodecs_AudioEncoding_AAC_LC',
        'audio-encode-decode.html',
        [{
            'codec': 'mp4a.67',
            'sample_rate': 48000,
            'channels': 2,
            'aac_format': 'aac',
        }],
    )
    yield (
        'WebCodecs_AudioEncoding_AAC_LC_ADTS',
        'audio-encode-decode.html',
        [{
            'codec': 'mp4a.67',
            'sample_rate': 48000,
            'channels': 2,
            'aac_format': 'adts',
        }],
    )

  @classmethod
  def BitrateTests(cls) -> ct.TestGenerator:
    high_res_codecs = [
        'avc1.420034',
        'hvc1.1.6.L123.00',
        'vp8',
        'vp09.00.10.08',
        'av01.0.04M.08',
    ]
    for codec, acc, bitrate_mode, bitrate in itertools.product(
        high_res_codecs, accelerations, ['constant', 'variable'],
        [1500000, 2000000, 3000000]):
      yield (
          f'WebCodecs_EncodingRateControl_'
          f'{codec}_{acc}_{bitrate_mode}_{bitrate}',
          'encoding-rate-control.html',
          [{
              'codec': codec,
              'acceleration': acc,
              'bitrate_mode': bitrate_mode,
              'bitrate': bitrate,
          }],
      )

  @classmethod
  def GenerateVideoTests(cls) -> ct.TestGenerator:
    yield (
        'WebCodecs_WebRTCPeerConnection_Window',
        'webrtc-peer-connection.html',
        [{
            'use_worker': False,
        }],
    )
    yield (
        'WebCodecs_WebRTCPeerConnection_Worker',
        'webrtc-peer-connection.html',
        [{
            'use_worker': True,
        }],
    )
    yield (
        'WebCodecs_Terminate_Worker',
        'terminate-worker.html',
        [{
            'source_type': 'offscreen',
        }],
    )

    source_type = 'offscreen'
    acc = 'prefer-hardware'
    for codec in ['avc1.42001E', 'hvc1.1.6.L93.B0']:
      yield (
          f'WebCodecs_PerFrameQpEncoding_{source_type}_{codec}_{acc}',
          'frame-qp-encoding.html',
          [{
              'source_type': source_type,
              'codec': codec,
              'acceleration': acc,
          }],
      )

    codec = 'av01.0.04M.08'
    acc = 'prefer-software'
    for layers in range(4):
      yield (
          f'WebCodecs_ManualSVC_{codec}_{acc}_layers_{layers}',
          'manual-svc.html',
          [{
              'codec': codec,
              'acceleration': acc,
              'layers': layers,
          }],
      )

    for source_type, codec, acc in itertools.product(frame_sources,
                                                     video_codecs,
                                                     accelerations):
      yield (
          f'WebCodecs_EncodeDecode_{source_type}_{codec}_{acc}',
          'encode-decode.html',
          [{
              'source_type': source_type,
              'codec': codec,
              'acceleration': acc,
          }],
      )

    # Also verify we can deal with the encoder's frame delay that we can
    # encounter for the AVC High profile (avc1.64).
    for source_type, codec, acc in itertools.product(
        frame_sources, video_codecs + ['avc1.64001E'], accelerations):
      yield (
          f'WebCodecs_Encode_{source_type}_{codec}_{acc}',
          'encode.html',
          [{
              'source_type': source_type,
              'codec': codec,
              'acceleration': acc,
          }],
      )

    resolutions = ['1920x1080', '3840x2160', '7680x3840']
    framerates = [30, 60, 120, 240]
    # Use at least level 6.2 (H.264/H.265/VP9), or level 6.3 (AV1) mimetypes to
    # test 8k 120fps support.
    codecs = [
        'avc1.64003E',
        'hvc1.1.6.L186.B0',
        'vp09.00.62.08',
        'av01.1.19M.08',
    ]
    for resolution, framerate, codec in itertools.product(
        resolutions, framerates, codecs):
      acc = 'prefer-hardware'
      latency_mode = 'quality'
      yield (
          f'WebCodecs_EncodingFramerateResolutions_'
          f'{resolution}_{framerate}_{codec}_{acc}_{latency_mode}',
          'encoding-framerate-resolutions.html',
          [{
              'resolution': resolution,
              'framerate': framerate,
              'codec': codec,
              'acceleration': acc,
              'latency_mode': latency_mode,
          }],
      )

    for codec, acc, bitrate_mode, latency_mode in itertools.product(
        video_codecs, accelerations, ['constant', 'variable'],
        ['realtime', 'quality']):
      source_type = 'offscreen'
      content_hint = 'motion'
      yield (
          f'WebCodecs_EncodingModes_'
          f'{source_type}_{codec}_{acc}_{bitrate_mode}_{latency_mode}',
          'encoding-modes.html',
          [{
              'source_type': source_type,
              'codec': codec,
              'acceleration': acc,
              'bitrate_mode': bitrate_mode,
              'latency_mode': latency_mode,
              'content_hint': content_hint,
          }],
      )

    for codec, content_hint in itertools.product(video_codecs,
                                                 ['detail', 'text', 'motion']):
      source_type = 'offscreen'
      acc = 'prefer-hardware'
      bitrate_mode = 'constant'
      latency_mode = 'realtime'
      yield (
          f'WebCodecs_ContentHint_{codec}_{content_hint}',
          'encoding-modes.html',
          [{
              'source_type': source_type,
              'codec': codec,
              'acceleration': acc,
              'bitrate_mode': bitrate_mode,
              'latency_mode': latency_mode,
              'content_hint': content_hint,
          }],
      )

    for codec, acc, layers in itertools.product(video_codecs, accelerations,
                                                [2, 3]):
      yield (
          f'WebCodecs_SVC_{codec}_{acc}_layers_{layers}',
          'svc.html',
          [{
              'codec': codec,
              'acceleration': acc,
              'layers': layers,
          }],
      )

    for codec, acc in itertools.product(video_codecs, accelerations):
      yield (
          f'WebCodecs_EncodeColorSpace_{codec}_{acc}',
          'encode-color-space.html',
          [{
              'codec': codec,
              'acceleration': acc,
          }],
      )
      yield (
          f'WebCodecs_MixedSourceEncoding_{codec}_{acc}',
          'mixed-source-encoding.html',
          [{
              'codec': codec,
              'acceleration': acc,
          }],
      )

    for codec, source_type in itertools.product(video_codecs, frame_sources):
      yield (
          f'WebCodecs_FrameSizeChange_{codec}_{source_type}',
          'frame-size-change.html',
          [{
              'codec': codec,
              'source_type': source_type,
          }],
      )
# pylint: enable=too-many-branches

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    url = self.UrlOfStaticFilePath(html_path + '/' + test_path)
    tab = self.tab
    arg_obj = args[0]
    os_name = self.platform.GetOSName()
    arg_obj['validate_camera_frames'] = self.CameraCanShowFourColors(os_name)
    tab.Navigate(url)
    tab.action_runner.WaitForJavaScriptCondition(
        'document.readyState == "complete"')
    tab.EvaluateJavaScript('TEST.run(' + json.dumps(arg_obj) + ')')
    tab.action_runner.WaitForJavaScriptCondition('TEST.finished', timeout=60)
    if tab.EvaluateJavaScript('TEST.skipped'):
      self.skipTest('Skipping test:' + tab.EvaluateJavaScript('TEST.summary()'))
    if not tab.EvaluateJavaScript('TEST.success'):
      self.fail('Test failure:' + tab.EvaluateJavaScript('TEST.summary()'))

  @staticmethod
  def CameraCanShowFourColors(os_name: str) -> bool:
    return os_name not in ('android', 'chromeos')

  @classmethod
  def SetUpProcess(cls) -> None:
    super(WebCodecsIntegrationTest, cls).SetUpProcess()
    args = [
        '--use-fake-device-for-media-stream',
        '--use-fake-ui-for-media-stream',
        '--enable-blink-features=SharedArrayBuffer',
        '--enable-features=VideoFrameAsyncCopyTo',
        cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
    ] + cba.ENABLE_WEBGPU_FOR_TESTING

    # If we don't call CustomizeBrowserArgs cls.platform is None
    cls.CustomizeBrowserArgs(args)

    if cls.CameraCanShowFourColors(cls.platform.GetOSName()):
      args.append('--use-file-for-fake-video-capture=' + four_colors_img_path)
      cls.CustomizeBrowserArgs(args)

    cls.StartBrowser()
    cls.SetStaticServerDirs([html_path, data_path])

  @classmethod
  def ExpectationsFiles(cls) -> list[str]:
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', 'webcodecs_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])

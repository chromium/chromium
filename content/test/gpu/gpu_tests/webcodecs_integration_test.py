# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import json
import itertools
from typing import Any, List, Set
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

  def _GetSerialGlobs(self) -> Set[str]:
    serial_globs = set()
    if host_information.IsWindows() and host_information.IsNvidiaGpu():
      serial_globs |= {
          # crbug.com/1473480. Windows + NVIDIA has a maximum parallel encode
          # limit of 2, so serialize hardware encoding tests on Windows.
          'WebCodecs_*prefer-hardware*',
      }
    return serial_globs

  def _GetSerialTests(self) -> Set[str]:
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
    for test in tests:
      yield test

  @classmethod
  def GenerateFrameTests(cls) -> ct.TestGenerator:
    for source_type in frame_sources:
      yield ('WebCodecs_DrawImage_' + source_type, 'draw-image.html', [{
          'source_type':
          source_type
      }])
      yield ('WebCodecs_TexImage2d_' + source_type, 'tex-image-2d.html', [{
          'source_type':
          source_type
      }])
      yield ('WebCodecs_copyTo_' + source_type, 'copyTo.html', [{
          'source_type':
          source_type
      }])
      yield ('WebCodecs_convertToRGB_' + source_type, 'convert-to-rgb.html', [{
          'source_type':
          source_type
      }])
    for source_type in hbd_frame_sources:
      yield ('WebCodecs_DrawImage_' + source_type, 'draw-image.html', [{
          'source_type':
          source_type
      }])

  @classmethod
  def GenerateAudioTests(cls) -> ct.TestGenerator:
    yield ('WebCodecs_AudioEncoding_AAC_LC', 'audio-encode-decode.html', [{
        'codec':
        'mp4a.67',
        'sample_rate':
        48000,
        'channels':
        2,
        'aac_format':
        'aac'
    }])
    yield ('WebCodecs_AudioEncoding_AAC_LC_ADTS', 'audio-encode-decode.html', [{
        'codec':
        'mp4a.67',
        'sample_rate':
        48000,
        'channels':
        2,
        'aac_format':
        'adts'
    }])

  @classmethod
  def BitrateTests(cls) -> ct.TestGenerator:
    high_res_codecs = [
        'avc1.420034', 'hvc1.1.6.L123.00', 'vp8', 'vp09.00.10.08',
        'av01.0.04M.08'
    ]
    for codec in high_res_codecs:
      for acc in accelerations:
        for bitrate_mode in ['constant', 'variable']:
          for bitrate in [1500000, 2000000, 3000000]:
            args = (codec, acc, bitrate_mode, bitrate)
            yield ('WebCodecs_EncodingRateControl_%s_%s_%s_%s' % args,
                   'encoding-rate-control.html', [{
                       'codec': codec,
                       'acceleration': acc,
                       'bitrate_mode': bitrate_mode,
                       'bitrate': bitrate
                   }])

  @classmethod
  def GenerateVideoTests(cls) -> ct.TestGenerator:
    yield ('WebCodecs_WebRTCPeerConnection_Window',
           'webrtc-peer-connection.html', [{
               'use_worker': False
           }])
    yield ('WebCodecs_WebRTCPeerConnection_Worker',
           'webrtc-peer-connection.html', [{
               'use_worker': True
           }])
    yield ('WebCodecs_Terminate_Worker', 'terminate-worker.html', [{
        'source_type':
        'offscreen',
    }])

    source_type = 'offscreen'
    codec = 'avc1.42001E'
    acc = 'prefer-hardware'
    args = (source_type, codec, acc)
    yield ('WebCodecs_PerFrameQpEncoding_%s_%s_%s' % args,
           'frame-qp-encoding.html', [{
               'source_type': source_type,
               'codec': codec,
               'acceleration': acc
           }])

    codec = 'av01.0.04M.08'
    acc = 'prefer-software'
    for layers in range(4):
      args = (codec, acc, layers)
      yield ('WebCodecs_ManualSVC_%s_%s_layers_%d' % args, 'manual-svc.html', [{
          'codec':
          codec,
          'acceleration':
          acc,
          'layers':
          layers
      }])

    for source_type in frame_sources:
      for codec in video_codecs:
        for acc in accelerations:
          args = (source_type, codec, acc)
          yield ('WebCodecs_EncodeDecode_%s_%s_%s' % args, 'encode-decode.html',
                 [{
                     'source_type': source_type,
                     'codec': codec,
                     'acceleration': acc
                 }])

    for source_type in frame_sources:
      # Also verify we can deal with the encoder's frame delay that we can
      # encounter for the AVC High profile (avc1.64).
      for codec in video_codecs + ['avc1.64001E']:
        for acc in accelerations:
          args = (source_type, codec, acc)
          yield ('WebCodecs_Encode_%s_%s_%s' % args, 'encode.html', [{
              'source_type':
              source_type,
              'codec':
              codec,
              'acceleration':
              acc
          }])

    for codec in video_codecs:
      for acc in accelerations:
        for bitrate_mode in ['constant', 'variable']:
          for latency_mode in ['realtime', 'quality']:
            source_type = 'offscreen'
            content_hint = 'motion'
            args = (source_type, codec, acc, bitrate_mode, latency_mode)
            yield ('WebCodecs_EncodingModes_%s_%s_%s_%s_%s' % args,
                   'encoding-modes.html', [{
                       'source_type': source_type,
                       'codec': codec,
                       'acceleration': acc,
                       'bitrate_mode': bitrate_mode,
                       'latency_mode': latency_mode,
                       'content_hint': content_hint
                   }])

    for codec in video_codecs:
      for content_hint in ['detail', 'text', 'motion']:
        source_type = 'offscreen'
        acc = 'prefer-hardware'
        bitrate_mode = 'constant'
        latency_mode = 'realtime'
        yield ('WebCodecs_ContentHint_%s_%s' % (codec, content_hint),
               'encoding-modes.html', [{
                   'source_type': source_type,
                   'codec': codec,
                   'acceleration': acc,
                   'bitrate_mode': bitrate_mode,
                   'latency_mode': latency_mode,
                   'content_hint': content_hint
               }])

    for codec in video_codecs:
      for acc in accelerations:
        for layers in [2, 3]:
          args = (codec, acc, layers)
          yield ('WebCodecs_SVC_%s_%s_layers_%d' % args, 'svc.html', [{
              'codec':
              codec,
              'acceleration':
              acc,
              'layers':
              layers
          }])

    for codec in video_codecs:
      for acc in accelerations:
        args = (codec, acc)
        yield ('WebCodecs_EncodeColorSpace_%s_%s' % args,
               'encode-color-space.html', [{
                   'codec': codec,
                   'acceleration': acc
               }])

    for codec in video_codecs:
      for source_type in frame_sources:
        args = (codec, source_type)
        yield ('WebCodecs_FrameSizeChange_%s_%s' % args,
               'frame-size-change.html', [{
                   'codec': codec,
                   'source_type': source_type
               }])
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
        cba.ENABLE_PLATFORM_HEVC_ENCODER_SUPPORT,
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
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', 'webcodecs_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])

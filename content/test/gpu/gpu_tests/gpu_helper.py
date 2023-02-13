# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
from typing import Dict, FrozenSet, List, Match, Optional, Tuple, Union
import unittest.mock as mock

from telemetry.internal.platform import gpu_info as tgi

# This set must be the union of the driver tags used in WebGL and WebGL2
# expectations files.
# Examples:
#   intel_lt_25.20.100.6577
#   mesa_ge_20.1
EXPECTATIONS_DRIVER_TAGS = frozenset([
    'mesa_lt_19.1',
    'mesa_ge_21.0',
])

# Driver tag format: VENDOR_OPERATION_VERSION
DRIVER_TAG_MATCHER = re.compile(
    r'^([a-z\d]+)_(eq|ne|ge|gt|le|lt)_([a-z\d\.]+)$')

REMOTE_BROWSER_TYPES = [
    'android-chromium',
    'android-webview-instrumentation',
    'cros-chrome',
    'fuchsia-chrome',
    'web-engine-shell',
    'cast-streaming-shell',
]

TAG_SUBSTRING_REPLACEMENTS = {
    # nvidia on desktop, nvidia-coproration on Android.
    'nvidia-corporation': 'nvidia',
}

ENTIRE_TAG_REPLACEMENTS = {
    # Includes a Vulkan and LLVM version.
    re.compile('google-vulkan.*swiftshader-device.*', re.IGNORECASE):
    'google-vulkan',
}

VENDOR_AMD = 0x1002
VENDOR_INTEL = 0x8086
VENDOR_NVIDIA = 0x10DE

INTEL_DEVICE_ID_MASK = 0xFF00
INTEL_GEN_9 = {0x1900, 0x3100, 0x3E00, 0x5900, 0x5A00, 0x9B00}
INTEL_GEN_12 = {0x4C00, 0x9A00, 0x4900, 0x4600, 0x4F00, 0x5600, 0xA700}


def _ParseANGLEGpuVendorString(device_string: str) -> Optional[str]:
  if not device_string:
    return None
  # ANGLE's device (renderer) string is of the form:
  # "ANGLE (vendor_string, renderer_string, gl_version profile)"
  # This function will be used to get the first value in the tuple
  match = re.search(r'ANGLE \((.*), .*, .*\)', device_string)
  if match:
    return match.group(1)
  return None


def _GetANGLEGpuDeviceId(device_string: str) -> Optional[str]:
  if not device_string:
    return None
  # ANGLE's device (renderer) string is of the form:
  # "ANGLE (vendor_string, renderer_string, gl_version profile)"
  # This function will be used to get the second value in the tuple
  match = re.search(r'ANGLE \(.*, (.*), .*\)', device_string)
  if match:
    return match.group(1)
  return None


def GetGpuVendorString(gpu_info: Optional[tgi.GPUInfo], index: int) -> str:
  if gpu_info:
    primary_gpu = gpu_info.devices[index]
    if primary_gpu:
      vendor_string = primary_gpu.vendor_string
      angle_vendor_string = _ParseANGLEGpuVendorString(
          primary_gpu.device_string)
      vendor_id = primary_gpu.vendor_id
      if vendor_id == VENDOR_NVIDIA:
        return 'nvidia'
      if vendor_id == VENDOR_AMD:
        return 'amd'
      if vendor_id == VENDOR_INTEL:
        return 'intel'
      if angle_vendor_string:
        return angle_vendor_string.lower()
      if vendor_string:
        return vendor_string.split(' ')[0].lower()
  return 'unknown_gpu'


def GetGpuDeviceId(gpu_info: Optional[tgi.GPUInfo],
                   index: int) -> Union[int, str]:
  if gpu_info:
    primary_gpu = gpu_info.devices[index]
    if primary_gpu:
      return (primary_gpu.device_id
              or _GetANGLEGpuDeviceId(primary_gpu.device_string)
              or primary_gpu.device_string)
  return 0


def IsIntel(vendor_id: int) -> bool:
  return vendor_id == VENDOR_INTEL


# Intel GPU architectures
def IsIntelGen9(gpu_device_id: int) -> bool:
  return gpu_device_id & INTEL_DEVICE_ID_MASK in INTEL_GEN_9


def IsIntelGen12(gpu_device_id: int) -> bool:
  return gpu_device_id & INTEL_DEVICE_ID_MASK in INTEL_GEN_12


def GetGpuDriverVendor(gpu_info: Optional[tgi.GPUInfo]) -> Optional[str]:
  if gpu_info:
    primary_gpu = gpu_info.devices[0]
    if primary_gpu:
      return primary_gpu.driver_vendor
  return None


def GetGpuDriverVersion(gpu_info: Optional[tgi.GPUInfo]) -> Optional[str]:
  if gpu_info:
    primary_gpu = gpu_info.devices[0]
    if primary_gpu:
      return primary_gpu.driver_version
  return None


def GetANGLERenderer(gpu_info: Optional[tgi.GPUInfo]) -> str:
  retval = 'angle-disabled'
  if gpu_info and gpu_info.aux_attributes:
    gl_renderer = gpu_info.aux_attributes.get('gl_renderer')
    if gl_renderer and 'ANGLE' in gl_renderer:
      if 'Direct3D11' in gl_renderer:
        retval = 'angle-d3d11'
      elif 'Direct3D9' in gl_renderer:
        retval = 'angle-d3d9'
      elif 'OpenGL ES' in gl_renderer:
        retval = 'angle-opengles'
      elif 'OpenGL' in gl_renderer:
        retval = 'angle-opengl'
      elif 'Metal' in gl_renderer:
        retval = 'angle-metal'
      # SwiftShader first because it also contains Vulkan
      elif 'SwiftShader' in gl_renderer:
        retval = 'angle-swiftshader'
      elif 'Vulkan' in gl_renderer:
        retval = 'angle-vulkan'
  return retval


def GetCommandDecoder(gpu_info: Optional[tgi.GPUInfo]) -> str:
  if gpu_info and gpu_info.aux_attributes and \
      gpu_info.aux_attributes.get('passthrough_cmd_decoder', False):
    return 'passthrough'
  return 'no_passthrough'


def GetSkiaRenderer(gpu_info: Optional[tgi.GPUInfo],
                    extra_browser_args: List[str]) -> str:
  retval = 'renderer-software'
  if gpu_info:
    gpu_feature_status = gpu_info.feature_status
    skia_renderer_enabled = (
        gpu_feature_status
        and gpu_feature_status.get('gpu_compositing') == 'enabled')
    if skia_renderer_enabled:
      if HasDawnSkiaRenderer(extra_browser_args):
        retval = 'renderer-skia-dawn'
      elif HasVulkanSkiaRenderer(gpu_feature_status):
        retval = 'renderer-skia-vulkan'
      # The check for GL must come after Vulkan since the 'opengl' feature can
      # be enabled for WebGL and interop even if SkiaRenderer is using Vulkan.
      elif HasGlSkiaRenderer(gpu_feature_status):
        retval = 'renderer-skia-gl'
  return retval


def GetDisplayServer(browser_type: str) -> Optional[str]:
  # Browser types run on a remote device aren't Linux, but the host running
  # this code uses Linux, so return early to avoid erroneously reporting a
  # display server.
  if browser_type in REMOTE_BROWSER_TYPES:
    return None
  if sys.platform.startswith('linux'):
    if 'WAYLAND_DISPLAY' in os.environ:
      return 'display-server-wayland'
    return 'display-server-x'
  return None


def GetOOPCanvasStatus(gpu_info: Optional[tgi.GPUInfo]) -> str:
  if gpu_info and gpu_info.feature_status and gpu_info.feature_status.get(
      'canvas_oop_rasterization') == 'enabled_on':
    return 'oop-c'
  return 'no-oop-c'


def GetAsanStatus(gpu_info: Optional[tgi.GPUInfo]) -> str:
  if gpu_info and gpu_info.aux_attributes.get('is_asan', False):
    return 'asan'
  return 'no-asan'


def GetTargetCpuStatus(gpu_info: Optional[tgi.GPUInfo]) -> str:
  suffix = 'unknown'
  if gpu_info:
    suffix = gpu_info.aux_attributes.get('target_cpu_bits', 'unknown')
  return 'target-cpu-%s' % suffix


def GetClangCoverage(gpu_info: Optional[tgi.GPUInfo]) -> str:
  if gpu_info and gpu_info.aux_attributes.get('is_clang_coverage', False):
    return 'clang-coverage'
  return 'no-clang-coverage'


# TODO(rivr): Use GPU feature status for Dawn instead of command line.
def HasDawnSkiaRenderer(extra_browser_args: List[str]) -> bool:
  if extra_browser_args:
    for arg in extra_browser_args:
      if arg.startswith('--enable-features') and 'SkiaDawn' in arg:
        return True
  return False


def HasGlSkiaRenderer(gpu_feature_status: Dict[str, str]) -> bool:
  return (bool(gpu_feature_status)
          and gpu_feature_status.get('opengl') == 'enabled_on')


def HasVulkanSkiaRenderer(gpu_feature_status: Dict[str, str]) -> bool:
  return (bool(gpu_feature_status)
          and gpu_feature_status.get('vulkan') == 'enabled_on')


def ReplaceTags(tags: List[str]) -> List[str]:
  """Replaces certain strings in tags to make them consistent across platforms.

  Args:
    tags: A list of strings containing expectation tags.

  Returns:
    |tags| but potentially with some elements replaced.
  """
  replaced_tags = []
  for t in tags:
    continue_to_next_tag = False
    for regex, replacement in ENTIRE_TAG_REPLACEMENTS.items():
      if regex.match(t):
        replaced_tags.append(replacement)
        continue_to_next_tag = True
        break
    if continue_to_next_tag:
      continue

    for original, replacement in TAG_SUBSTRING_REPLACEMENTS.items():
      if original in t:
        replaced_tags.append(t.replace(original, replacement))
        continue_to_next_tag = True
        break
    if continue_to_next_tag:
      continue

    replaced_tags.append(t)
  return replaced_tags


# used by unittests to create a mock arguments object
def GetMockArgs(webgl_version: str = '1.0.0') -> mock.MagicMock:
  args = mock.MagicMock()
  args.webgl_conformance_version = webgl_version
  args.webgl2_only = False
  # for power_measurement_integration_test.py, .url has to be None to
  # generate the correct test lists for bots.
  args.url = None
  args.duration = 10
  args.delay = 10
  args.resolution = 100
  args.fullscreen = False
  args.underlay = False
  args.logdir = '/tmp'
  args.repeat = 1
  args.outliers = 0
  args.bypass_ipg = False
  args.expected_vendor_id = 0
  args.expected_device_id = 0
  args.browser_options = []
  return args


def MatchDriverTag(tag: str) -> Match[str]:
  return DRIVER_TAG_MATCHER.match(tag.lower())


# No good way to reduce the number of local variables, particularly since each
# argument is also considered a local. Also no good way to reduce the number of
# branches without harming readability.
# pylint: disable=too-many-locals,too-many-branches
def EvaluateVersionComparison(version: str,
                              operation: str,
                              ref_version: str,
                              os_name: Optional[str] = None,
                              driver_vendor: Optional[str] = None) -> bool:
  def parse_version(ver: str) -> Union[Tuple[int, str], Tuple[None, None]]:
    if ver.isdigit():
      return int(ver), ''
    for i, digit in enumerate(ver):
      if not digit.isdigit():
        return int(ver[:i]) if i > 0 else 0, ver[i:]
    return None, None

  def versions_can_be_compared(ver_list1, ver_list2):
    # If either of the two versions doesn't match the Intel driver version
    # schema, they should not be compared.
    if len(ver_list1) != 4 or len(ver_list2) != 4:
      return False
    return True

  ver_list1 = version.split('.')
  ver_list2 = ref_version.split('.')
  # On Windows, if the driver vendor is Intel, the driver version should be
  # compared based on the Intel graphics driver version schema.
  # https://www.intel.com/content/www/us/en/support/articles/000005654/graphics-drivers.html
  if os_name == 'win' and driver_vendor == 'intel':
    if not versions_can_be_compared(ver_list1, ver_list2):
      return operation == 'ne'

    ver_list1 = ver_list1[2:]
    ver_list2 = ver_list2[2:]

  for i in range(0, max(len(ver_list1), len(ver_list2))):
    ver1 = ver_list1[i] if i < len(ver_list1) else '0'
    ver2 = ver_list2[i] if i < len(ver_list2) else '0'
    num1, suffix1 = parse_version(ver1)
    num2, suffix2 = parse_version(ver2)

    if num1 is None:
      continue

    # This comes from EXPECTATIONS_DRIVER_TAGS, so we should never fail to
    # parse a version.
    assert num2 is not None

    if not num1 == num2:
      diff = num1 - num2
    elif suffix1 == suffix2:
      continue
    elif suffix1 > suffix2:
      diff = 1
    else:
      diff = -1

    if operation == 'eq':
      return False
    if operation == 'ne':
      return True
    if operation in ('ge', 'gt'):
      return diff > 0
    if operation in ('le', 'lt'):
      return diff < 0
    raise Exception('Invalid operation: ' + operation)

  return operation in ('eq', 'ge', 'le')
# pylint: enable=too-many-locals,too-many-branches


def ExpectationsDriverTags() -> FrozenSet[str]:
  return EXPECTATIONS_DRIVER_TAGS

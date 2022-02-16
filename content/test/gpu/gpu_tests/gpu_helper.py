# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import unittest.mock as mock

# This set must be the union of the driver tags used in WebGL and WebGL2
# expectations files.
# Examples:
#   intel_lt_25.20.100.6577
#   mesa_ge_20.1
EXPECTATIONS_DRIVER_TAGS = frozenset([
    'mesa_lt_19.1',
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
]


def _ParseANGLEGpuVendorString(device_string):
  if not device_string:
    return None
  # ANGLE's device (renderer) string is of the form:
  # "ANGLE (vendor_string, renderer_string, gl_version profile)"
  # This function will be used to get the first value in the tuple
  match = re.search(r'ANGLE \((.*), .*, .*\)', device_string)
  if match:
    return match.group(1)
  return None


def _GetANGLEGpuDeviceId(device_string):
  if not device_string:
    return None
  # ANGLE's device (renderer) string is of the form:
  # "ANGLE (vendor_string, renderer_string, gl_version profile)"
  # This function will be used to get the second value in the tuple
  match = re.search(r'ANGLE \(.*, (.*), .*\)', device_string)
  if match:
    return match.group(1)
  return None


def GetGpuVendorString(gpu_info, index):
  if gpu_info:
    primary_gpu = gpu_info.devices[index]
    if primary_gpu:
      vendor_string = primary_gpu.vendor_string
      angle_vendor_string = _ParseANGLEGpuVendorString(
          primary_gpu.device_string)
      vendor_id = primary_gpu.vendor_id
      if vendor_id == 0x10DE:
        return 'nvidia'
      if vendor_id == 0x1002:
        return 'amd'
      if vendor_id == 0x8086:
        return 'intel'
      if angle_vendor_string:
        return angle_vendor_string.lower()
      if vendor_string:
        return vendor_string.split(' ')[0].lower()
  return 'unknown_gpu'


def GetGpuDeviceId(gpu_info, index):
  if gpu_info:
    primary_gpu = gpu_info.devices[index]
    if primary_gpu:
      return (primary_gpu.device_id
              or _GetANGLEGpuDeviceId(primary_gpu.device_string)
              or primary_gpu.device_string)
  return 0


def GetGpuDriverVendor(gpu_info):
  if gpu_info:
    primary_gpu = gpu_info.devices[0]
    if primary_gpu:
      return primary_gpu.driver_vendor
  return None


def GetGpuDriverVersion(gpu_info):
  if gpu_info:
    primary_gpu = gpu_info.devices[0]
    if primary_gpu:
      return primary_gpu.driver_version
  return None


def GetANGLERenderer(gpu_info):
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


def GetSwiftShaderGLRenderer(gpu_info):
  if gpu_info and gpu_info.aux_attributes:
    gl_renderer = gpu_info.aux_attributes.get('gl_renderer')
    # Filter out ANGLE on top of SwiftShader Vulkan,
    # as we are only interested in SwiftShader GL
    if (gl_renderer and 'ANGLE' not in gl_renderer
        and 'SwiftShader' in gl_renderer):
      return 'swiftshader-gl'
  return 'no-swiftshader-gl'


def GetCommandDecoder(gpu_info):
  if gpu_info and gpu_info.aux_attributes and \
      gpu_info.aux_attributes.get('passthrough_cmd_decoder', False):
    return 'passthrough'
  return 'no_passthrough'


def GetSkiaRenderer(gpu_feature_status, extra_browser_args):
  retval = 'skia-renderer-disabled'
  skia_renderer_enabled = (
      gpu_feature_status
      and gpu_feature_status.get('skia_renderer') == 'enabled_on'
      and gpu_feature_status.get('gpu_compositing') == 'enabled')
  if skia_renderer_enabled:
    if HasDawnSkiaRenderer(extra_browser_args):
      retval = 'skia-renderer-dawn'
    elif HasVulkanSkiaRenderer(gpu_feature_status):
      retval = 'skia-renderer-vulkan'
    # The check for GL must come after Vulkan since the 'opengl' feature can be
    # enabled for WebGL and interop even if SkiaRenderer is using Vulkan.
    elif HasGlSkiaRenderer(gpu_feature_status):
      retval = 'skia-renderer-gl'
  return retval


def GetDisplayServer(browser_type):
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


def GetOOPCanvasStatus(gpu_feature_status):
  if gpu_feature_status and gpu_feature_status.get(
      'canvas_oop_rasterization') == 'enabled_on':
    return 'oop-c'
  return 'no-oop-c'


def GetAsanStatus(gpu_info):
  if gpu_info.aux_attributes.get('is_asan', False):
    return 'asan'
  return 'no-asan'


# TODO(rivr): Use GPU feature status for Dawn instead of command line.
def HasDawnSkiaRenderer(extra_browser_args):
  if extra_browser_args:
    for arg in extra_browser_args:
      if arg.startswith('--enable-features') and 'SkiaDawn' in arg:
        return True
  return False


def HasGlSkiaRenderer(gpu_feature_status):
  return gpu_feature_status and gpu_feature_status.get('opengl') == 'enabled_on'


def HasVulkanSkiaRenderer(gpu_feature_status):
  return gpu_feature_status and gpu_feature_status.get('vulkan') == 'enabled_on'


# used by unittests to create a mock arguments object
def GetMockArgs(webgl_version='1.0.0'):
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


def MatchDriverTag(tag):
  return DRIVER_TAG_MATCHER.match(tag.lower())


# No good way to reduce the number of local variables, particularly since each
# argument is also considered a local. Also no good way to reduce the number of
# branches without harming readability.
# pylint: disable=too-many-locals,too-many-branches
def EvaluateVersionComparison(version,
                              operation,
                              ref_version,
                              os_name=None,
                              driver_vendor=None):
  def parse_version(ver):
    if ver.isdigit():
      return int(ver), ''
    for i, digit in enumerate(ver):
      if not digit.isdigit():
        return int(ver[:i]) if i > 0 else 0, ver[i:]
    return None

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


def ExpectationsDriverTags():
  return EXPECTATIONS_DRIVER_TAGS

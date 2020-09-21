# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import mock

# This set must be the union of the driver tags used in WebGL and WebGL2
# expectations files.
EXPECTATIONS_DRIVER_TAGS = frozenset([
    'intel_lt_25.20.100.6444',
    'intel_lt_25.20.100.6577',
    'intel_lt_26.20.100.7000',
    'intel_lt_26.20.100.7323',
    'intel_lt_26.20.100.7870',
    'intel_lt_26.20.100.8141',
    'intel_lt_27.20.100.8280',
    'mesa_lt_19.1',
    'mesa_ge_20.1',
])

# Driver tag format: VENDOR_OPERATION_VERSION
DRIVER_TAG_MATCHER = re.compile(
    r'^([a-z\d]+)_(eq|ne|ge|gt|le|lt)_([a-z\d\.]+)$')


def _ParseANGLEGpuVendorString(device_string):
  if not device_string:
    return None
  # ANGLE's device (renderer) string is of the form:
  # "ANGLE (vendor_string, renderer_string, gl_version profile)"
  # This function will be used to get the first value in the tuple
  match = re.search(r'ANGLE \((.*), .*, .*\)', device_string)
  if match:
    return match.group(1)
  else:
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
  else:
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
      elif vendor_id == 0x1002:
        return 'amd'
      elif vendor_id == 0x8086:
        return 'intel'
      elif angle_vendor_string:
        return angle_vendor_string.lower()
      elif vendor_string:
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
  retval = 'no_angle'
  if gpu_info and gpu_info.aux_attributes:
    gl_renderer = gpu_info.aux_attributes.get('gl_renderer')
    if gl_renderer and 'ANGLE' in gl_renderer:
      if 'Direct3D11' in gl_renderer:
        retval = 'd3d11'
      elif 'Direct3D9' in gl_renderer:
        retval = 'd3d9'
      elif 'OpenGL ES' in gl_renderer:
        retval = 'opengles'
      elif 'OpenGL' in gl_renderer:
        retval = 'opengl'
      elif 'Metal' in gl_renderer:
        retval = 'metal'
      # SwiftShader first because it also contains Vulkan
      elif 'SwiftShader' in gl_renderer:
        retval = 'swiftshader'
      elif 'Vulkan' in gl_renderer:
        retval = 'vulkan'
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


# Used to check GPU feature status to see if SkiaRenderer is enabled.
def GetSkiaRenderer(gpu_feature_status):
  if gpu_feature_status and 'skia_renderer' in gpu_feature_status:
    if gpu_feature_status['skia_renderer'] == 'enabled_on':
      return 'skia-renderer'
  return 'no-skia-renderer'


# Used to check GPU feature status to see if Vulkan is enabled.
def GetVulkan(gpu_feature_status):
  if gpu_feature_status and 'vulkan' in gpu_feature_status:
    if gpu_feature_status['vulkan'] == 'enabled_on':
      return 'use-vulkan'
  return 'no-use-vulkan'


# Used to parse additional options sent to the browser instance via
# '--extra-browser-args', looking for '--use-gl='.
def GetGL(extra_browser_args):
  if extra_browser_args:
    for o in extra_browser_args:
      if "--use-gl=" in o:
        return 'use-gl'
  return 'no-use-gl'


# Used to parse additional options sent to the browser instance via
# '--extra-browser-args', looking for '--enable-features=SkiaDawn' which
# may be merged with additional feature flags.
# TODO(sgilhuly): Use GPU feature status for Dawn instead of command line.
def GetSkiaDawn(extra_browser_args):
  if extra_browser_args:
    for o in extra_browser_args:
      if o.startswith('--enable-features') and "SkiaDawn" in o:
        return 'use-skia-dawn'
  return 'no-use-skia-dawn'


# used by unittests to create a mock arguments object
def GetMockArgs(is_asan=False, webgl_version='1.0.0'):
  args = mock.MagicMock()
  args.is_asan = is_asan
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
    for i in range(0, len(ver)):
      if not ver[i].isdigit():
        return int(ver[:i]) if i > 0 else 0, ver[i:]

  def is_old_intel_driver(ver_list):
    assert len(ver_list) == 4
    num, suffix = parse_version(ver_list[2])
    assert not suffix
    return num < 100

  def versions_can_be_compared(ver_list1, ver_list2):
    # If either of the two versions doesn't match the Intel driver version
    # schema, or they belong to different generation of version schema, they
    # should not be compared.
    if len(ver_list1) != 4 or len(ver_list2) != 4:
      return False
    if is_old_intel_driver(ver_list1) != is_old_intel_driver(ver_list2):
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
    if is_old_intel_driver(ver_list1):
      ver_list1 = ver_list1[3:]
      ver_list2 = ver_list2[3:]
    else:
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
    elif operation == 'ne':
      return True
    elif operation == 'ge' or operation == 'gt':
      return diff > 0
    elif operation == 'le' or operation == 'lt':
      return diff < 0
    raise Exception('Invalid operation: ' + operation)

  return operation == 'eq' or operation == 'ge' or operation == 'le'
# pylint: enable=too-many-locals,too-many-branches


def ExpectationsDriverTags():
  return EXPECTATIONS_DRIVER_TAGS

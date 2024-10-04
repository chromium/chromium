#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate data struct from GPU blocklist and driver bug workarounds json."""

import json
import os
import platform
import sys
import zlib
from optparse import OptionParser
from subprocess import call

_LICENSE = """// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"""

_DO_NOT_EDIT_WARNING = """// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

"""

_OS_TYPE_MAP = {
    'win': 'kOsWin',
    'macosx': 'kOsMacosx',
    'android': 'kOsAndroid',
    'linux': 'kOsLinux',
    'chromeos': 'kOsChromeOS',
    'fuchsia': 'kOsFuchsia',
    '': 'kOsAny',
  }

INTEL_DRIVER_VERSION_SCHEMA = '''
The version format of Intel graphics driver is AA.BB.CC.DDDD (legacy schema)
and AA.BB.CCC.DDDD (new schema).

AA.BB: You are free to specify the real number here, but they are meaningless
when comparing two version numbers. Usually it's okay to leave it to "0.0".

CC or CCC: It's meaningful to indicate different branches. Different CC means
different branch, while all CCCs share the same branch.

DDDD: It's always meaningful.
'''

NVIDIA_DRIVER_VERSION_SCHEMA = '''
The version format used by Nvidia is ABC.DE, where A-E are any digit.  When
queried by Chrome, it will detect XX.XX.XXXA.BCDE, where 'X' is any digit and
can be ignored.  Chrome will re-format this to ABC.DE, and compare to the
version listed here.

So, Chrome might detect 26.21.0014.4575, which would be given here as 445.75 in
the Nvidia version schema.  The 26.21.001 is ignored.
'''


def check_intel_driver_version(version):
  ver_list = version.split('.')
  if len(ver_list) != 4:
    return False
  for ver in ver_list:
    if not ver.isdigit():
      return False
  return True

def check_nvidia_driver_version(version):
  ver_list = version.split('.')
  # Allow "456" to match "456.*", so allow a single-entry list.
  if len(ver_list) == 0 or len(ver_list) > 2:
    return False
  if len(ver_list) == 2 and len(ver_list[1]) != 2:
    return False
  # Must start with three digits, whether it's "456.*" or "456.78".
  if len(ver_list[0]) != 3:
    return False
  for ver in ver_list:
    if not ver.isdigit():
      return False
  return True

def load_software_rendering_list_features(feature_type_filename):
  header_file = open(feature_type_filename, 'r')
  start = False
  features = []
  for line in header_file:
    if line.startswith('enum GpuFeatureType {'):
      assert not start
      start = True
      continue
    if not start:
      continue
    line = line.strip()
    line = line.split(' ', 1)[0]
    line = line.split(',', 1)[0]
    if line.startswith('NUMBER_OF_GPU_FEATURE_TYPES'):
      assert start
      start = False
      break
    if line.startswith('GPU_FEATURE_TYPE_'):
      name = line[len('GPU_FEATURE_TYPE_'):]
      features.append(name.lower())
    else:
      assert False
  assert not start
  assert len(features) > 0
  header_file.close()
  return features


def load_gpu_driver_bug_workarounds(workaround_type_filename):
  header_file = open(workaround_type_filename, 'r')
  start = False
  workaround = None
  workarounds = []
  for line in header_file:
    if line.startswith('#define GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)'):
      assert not start
      start = True
      continue
    if not start:
      continue
    line = line.strip()
    if line.startswith('GPU_OP('):
      assert not workaround
      workaround = line[len('GPU_OP('):]
      workaround = workaround.split(',', 1)[0].lower()
      continue
    if workaround:
      line = line.split(')', 1)[0]
      assert line == workaround
      workarounds.append(line)
      workaround = None
      continue
    start = False
    break
  assert not start
  assert len(workarounds) > 0
  header_file.close()
  return workarounds


def get_feature_set(features, total_feature_set):
  assert len(features) > 0
  feature_set = dict()
  for feature in features:
    if feature == 'all':
      feature_set = {k:1 for k in total_feature_set}
    elif isinstance(feature, dict):
      for key in feature:
        if key == 'exceptions':
          for exception in feature['exceptions']:
            assert exception in feature_set
            del feature_set[exception]
        else:
          raise KeyError('only exceptions are allowed')
    else:
      assert feature in total_feature_set
      feature_set[feature] = 1
  return feature_set


def write_features(feature_set, feature_name_prefix, var_name,
                   data_helper_file):
  data_helper_file.write('static const std::array<int, %d> %s = {\n' %
                         (len(feature_set), var_name))
  for feature in feature_set.keys():
    data_helper_file.write(feature_name_prefix + feature.upper())
    data_helper_file.write(',\n')
  data_helper_file.write('};\n\n')


def write_disabled_extension_list(entry_kind, entry_id, data, data_file,
                                  data_helper_file):
  if data:
    var_name = 'k%sForEntry%d' % (entry_kind, entry_id)
    # define the list
    data_helper_file.write(
        'static const std::array<const char* const, %d> %s = {\n' %
        (len(data), var_name))
    for item in data:
      write_string(item, data_helper_file)
      data_helper_file.write(',\n')
    data_helper_file.write('};\n\n')
    # use the list
    data_file.write('base::span(%s),  // %s\n' % (var_name, entry_kind))
  else:
    data_file.write('base::span<const char* const>(),  // %s\n' % entry_kind)


def write_gl_strings(entry_id, is_exception, exception_id, data,
                     unique_symbol_id, data_file, data_helper_file):
  if data:
    var_name = 'kGLStringsFor%sEntry%d' % (unique_symbol_id, entry_id)
    if is_exception:
      var_name += 'Exception' + str(exception_id)
    # define the GL strings
    data_helper_file.write('static const GpuControlList::GLStrings %s = {\n' %
                           var_name)
    for item in data:
      write_string(item, data_helper_file)
      data_helper_file.write(',\n')
    data_helper_file.write('};\n\n')
    # reference the GL strings
    data_file.write('&%s,  // GL strings\n' % var_name)
  else:
    data_file.write('nullptr,  // GL strings\n')


def write_version(version_info, name_tag, data_file):
  op = ''
  style = ''
  schema = ''
  version1 = ''
  version2 = ''
  if version_info:
    op = version_info['op']
    if 'style' in version_info:
      style = version_info['style']
    if 'schema' in version_info:
      schema = version_info['schema']
    version1 = version_info['value']
    if 'value2' in version_info:
      version2 = version_info['value2']
  data_file.write('{')
  op_map = {
    '=': 'kEQ',
    '<': 'kLT',
    '<=': 'kLE',
    '>': 'kGT',
    '>=': 'kGE',
    'any': 'kAny',
    'between': 'kBetween',
    '': 'kUnknown',
  }
  assert op in op_map
  data_file.write('GpuControlList::%s, ' % op_map[op])
  style_map = {
    'lexical': 'Lexical',
    'numerical': 'Numerical',
    '': 'Numerical',
  }
  assert style in style_map
  data_file.write('GpuControlList::kVersionStyle%s, ' % style_map[style])
  schema_map = {
    'common': 'Common',
    'intel_driver': 'IntelDriver',
    'nvidia_driver': 'NvidiaDriver',
    '': 'Common',
  }
  assert schema in schema_map
  data_file.write('GpuControlList::kVersionSchema%s, ' % schema_map[schema])
  write_string(version1, data_file)
  data_file.write(', ')
  write_string(version2, data_file)
  data_file.write('},  // %s\n' % name_tag)


def write_driver_info(entry_id, is_exception, exception_id, driver_vendor,
                      driver_version, unique_symbol_id,
                      data_file, data_helper_file):
  var_name = 'kDriverInfoFor%sEntry%d' % (unique_symbol_id, entry_id)
  if is_exception:
    var_name += 'Exception' + str(exception_id)
  # define the GL strings
  data_helper_file.write('static const GpuControlList::DriverInfo %s = {\n' %
                         var_name)
  write_string_value(driver_vendor, 'driver_vendor', data_helper_file)
  write_version(driver_version, 'driver_version', data_helper_file)
  data_helper_file.write('};\n\n')
  # reference the GL strings
  data_file.write('&%s,  // driver info\n' % var_name)


def write_number_list(entry_id, data_type, name_tag, data, is_exception,
                      exception_id, unique_symbol_id, data_file,
                      data_helper_file):
  if data:
    var_name = 'k%sFor%sEntry%d' % (name_tag, unique_symbol_id, entry_id)
    if is_exception:
      var_name += 'Exception' + str(exception_id)
    # define the list
    data_helper_file.write('static const std::array<%s, %d> %s = {\n' %
                           (data_type, len(data), var_name))
    for item in data:
      data_helper_file.write(str(item))
      data_helper_file.write(',\n')
    data_helper_file.write('};\n\n')
    # reference the list
    data_file.write('base::span(%s),  // %s\n' % (var_name, name_tag))
  else:
    data_file.write('base::span<const %s>(),  // %s\n' % (data_type, name_tag))


def write_string(string, data_file):
  if string == '':
    data_file.write('nullptr')
  else:
    data_file.write('"%s"' % string.replace('\\', '\\\\'))


def write_string_value(string, name_tag, data_file):
  write_string(string, data_file)
  data_file.write(',  // %s\n' % name_tag)


def write_boolean_value(value, name_tag, data_file):
  data_file.write('%s,  // %s\n' % (str(value).lower(), name_tag))


def write_integer_value(value, name_tag, data_file):
  data_file.write('%s,  // %s\n' % (str(value), name_tag))


def write_device_list(entry_id, device_id, device_revision, is_exception,
                      exception_id, unique_symbol_id, data_file,
                      data_helper_file):
  if device_id:
    # It's one of the three ways to specify devices:
    #  1) only specify device IDs
    #  2) specify one device ID, associated with multiple revisions
    #  3) specify k device IDs associated with k device revisions.
    device_size = len(device_id)
    if device_size == 1 and device_revision and len(device_revision) > 1:
      device_size = len(device_revision)
      for ii in range(device_size - 1):
        device_id.append(device_id[0])
    if device_revision is None:
      device_revision = []
      for ii in range(device_size):
        device_revision.append('0x0')
    assert len(device_id) == len(device_revision)
    var_name = 'kDevicesFor%sEntry%d' % (unique_symbol_id, entry_id)
    if is_exception:
      var_name += 'Exception' + str(exception_id)
    # define the list
    data_helper_file.write(
        'static const std::array<GpuControlList::Device, %d> %s = {{\n' %
        (len(device_id), var_name))
    for ii in range(device_size):
      data_helper_file.write('{%s, %s},\n' %
                             (device_id[ii], device_revision[ii]))
    data_helper_file.write('}};\n\n')
    # reference the list
    data_file.write('base::span(%s),  // Devices\n' % var_name)
  else:
    assert not device_revision
    data_file.write('base::span<const GpuControlList::Device>(),  // Devices\n')


def write_machine_model_info(entry_id, is_exception, exception_id,
                             machine_model_name, machine_model_version,
                             data_file, data_helper_file):
  model_name_var_name = None
  if machine_model_name:
    assert isinstance(machine_model_name, list)
    model_name_var_name = 'kMachineModelNameForEntry' + str(entry_id)
    if is_exception:
      model_name_var_name += 'Exception' + str(exception_id)
    data_helper_file.write(
        'static const std::array<const char* const, %d> %s = {{\n' %
        (len(machine_model_name), model_name_var_name))
    for item in machine_model_name:
      write_string(item, data_helper_file)
      data_helper_file.write(',\n')
    data_helper_file.write('}};\n\n')
  var_name = None
  if machine_model_name or machine_model_version:
    var_name = 'kMachineModelInfoForEntry' + str(entry_id)
    if is_exception:
      var_name += 'Exception' + str(exception_id)
    # define machine model info
    data_helper_file.write(
      'static const GpuControlList::MachineModelInfo %s = {\n' % var_name)
    if machine_model_name:
      data_helper_file.write('base::span(%s),  // machine model names\n' %
                             model_name_var_name)
    else:
      data_helper_file.write(
          'base::span<const char* const>(),  // machine model names\n')
    write_version(machine_model_version, 'machine model version',
                  data_helper_file)
    data_helper_file.write('};\n\n')
    # reference the machine model info
    data_file.write('&%s,  // machine model info\n' % var_name)
  else:
    data_file.write('nullptr,  // machine model info\n')


def write_os_type(os_type, data_file):
  assert os_type in _OS_TYPE_MAP
  data_file.write('GpuControlList::%s,  // os_type\n' % _OS_TYPE_MAP[os_type])


def write_multi_gpu_category(multi_gpu_category, data_file):
  suffix_for_category = {
    'primary': 'Primary',
    'secondary': 'Secondary',
    'npu': 'Npu',
    'active': 'Active',
    'any': 'Any',
    '': 'None',
  }
  assert multi_gpu_category in suffix_for_category
  data_file.write(
    'GpuControlList::kMultiGpuCategory%s,  // multi_gpu_category\n' %
    suffix_for_category[multi_gpu_category])


def write_multi_gpu_style(multi_gpu_style, data_file):
  suffix_for_style = {
    'optimus': 'Optimus',
    'amd_switchable': 'AMDSwitchable',
    'amd_switchable_discrete': 'AMDSwitchableDiscrete',
    'amd_switchable_integrated': 'AMDSwitchableIntegrated',
    '': 'None',
  }
  assert multi_gpu_style in suffix_for_style
  data_file.write(
    'GpuControlList::kMultiGpuStyle%s,  // multi_gpu_style\n' %
    suffix_for_style[multi_gpu_style])


def write_gl_type(gl_type, data_file):
  suffix_for_type = {
    'gl': 'GL',
    'gles': 'GLES',
    'angle': 'ANGLE',
    '': 'None',
  }
  assert gl_type in suffix_for_type
  data_file.write('GpuControlList::kGLType%s,  // gl_type\n' %
                  suffix_for_type[gl_type])


def write_supported_or_not(feature_value, feature_name, data_file):
  if feature_value is None:
    feature_value = 'dont_care'
  suffix_for_value = {
    'supported': 'Supported',
    'unsupported': 'Unsupported',
    'dont_care': 'DontCare',
  }
  assert feature_value in suffix_for_value
  data_file.write('GpuControlList::k%s,  // %s\n' %
                  (suffix_for_value[feature_value], feature_name))


def write_conditions(entry_id, is_exception, exception_id, entry,
                     unique_symbol_id, data_file, data_helper_file,
                     _data_exception_file):
  os_type = ''
  os_version = None
  vendor_id = 0
  device_id = None
  device_revision = None
  multi_gpu_category = ''
  multi_gpu_style = ''
  intel_gpu_series_list = None
  intel_gpu_generation = None
  driver_vendor = ''
  driver_version = None
  gl_renderer = ''
  gl_vendor = ''
  gl_extensions = ''
  gl_version_string = ''
  gl_type = ''
  gl_version = None
  pixel_shader_version = None
  in_process_gpu = False
  gl_reset_notification_strategy = None
  direct_rendering_version = None
  gpu_count = None
  hardware_overlay = None
  test_group = 0
  machine_model_name = None
  machine_model_version = None
  exception_count = 0
  subpixel_font_rendering = None
  # process the entry
  for key in entry:
    if key == 'id':
      assert not is_exception
      assert entry['id'] == entry_id
      continue
    if key == 'description':
      assert not is_exception
      continue
    if key == 'driver_update_url':
      assert not is_exception
      continue
    if key == 'features':
      assert not is_exception
      continue
    if key == 'disabled_extensions':
      assert not is_exception
      continue
    if key == 'disabled_webgl_extensions':
      assert not is_exception
      continue
    if key == 'comment':
      continue
    if key == 'webkit_bugs':
      assert not is_exception
      continue
    if key == 'cr_bugs':
      assert not is_exception
      continue
    if key == 'os':
      os_info = entry[key]
      os_type = os_info['type']
      if 'version' in os_info:
        os_version = os_info['version']
    elif key == 'vendor_id':
      vendor_id = int(entry[key], 0)
    elif key == 'device_id':
      device_id = entry[key]
    elif key == 'device_revision':
      device_revision = entry[key]
    elif key == 'multi_gpu_category':
      multi_gpu_category = entry[key]
    elif key == 'multi_gpu_style':
      multi_gpu_style = entry[key]
    elif key == 'intel_gpu_series':
      intel_gpu_series_list = entry[key]
    elif key == 'intel_gpu_generation':
      intel_gpu_generation = entry[key]
    elif key == 'driver_vendor':
      driver_vendor = entry[key]
    elif key == 'driver_version':
      driver_version = entry[key]
    elif key == 'gl_vendor':
      gl_vendor = entry[key]
    elif key == 'gl_renderer':
      gl_renderer = entry[key]
    elif key == 'gl_version_string':
      gl_version_string = entry[key]
    elif key == 'gl_type':
      gl_type = entry[key]
    elif key == 'gl_version':
      gl_version = entry[key]
    elif key == 'gl_extensions':
      gl_extensions = entry[key]
    elif key == 'pixel_shader_version':
      pixel_shader_version = entry[key]
    elif key == 'in_process_gpu':
      assert entry[key]
      in_process_gpu = True
    elif key == 'gl_reset_notification_strategy':
      gl_reset_notification_strategy = entry[key]
    elif key == 'direct_rendering_version':
      direct_rendering_version = entry[key]
    elif key == 'gpu_count':
      gpu_count = entry[key]
    elif key == 'hardware_overlay':
      hardware_overlay = entry[key]
    elif key == 'test_group':
      assert entry[key] > 0
      test_group = entry[key]
    elif key == 'machine_model_name':
      machine_model_name = entry[key]
    elif key == 'machine_model_version':
      machine_model_version = entry[key]
    elif key == 'subpixel_font_rendering':
      subpixel_font_rendering = entry[key]
    elif key == 'exceptions':
      assert not is_exception
      assert exception_count == 0
    else:
      raise ValueError('unknown key: ' + key + ' in entry ' + str(entry))
  # write out the entry
  write_os_type(os_type, data_file)
  write_version(os_version, 'os_version', data_file)
  data_file.write(format(vendor_id, '#04x'))
  data_file.write(',  // vendor_id\n')
  write_device_list(entry_id, device_id, device_revision, is_exception,
                    exception_id, unique_symbol_id, data_file,
                    data_helper_file)
  write_multi_gpu_category(multi_gpu_category, data_file)
  write_multi_gpu_style(multi_gpu_style, data_file)
  # group driver info
  if driver_vendor != '' or driver_version != None:
    if multi_gpu_category != '':
      assert vendor_id != 0, 'Need vendor_id in entry with id: '+ str(entry_id)
    if driver_version and driver_version.get('schema') == 'intel_driver':
      assert os_type == 'win', 'Intel driver schema is only for Windows'
      is_intel = (format(vendor_id, '#04x') == '0x8086' or
                  intel_gpu_series_list or
                  intel_gpu_generation or
                  'Intel' in driver_vendor)
      assert is_intel, 'Intel driver schema is only for Intel GPUs'
      valid_version = check_intel_driver_version(driver_version['value'])
      if 'value2' in driver_version:
        valid_version = (valid_version and
                         check_intel_driver_version(driver_version['value2']))
      assert valid_version, INTEL_DRIVER_VERSION_SCHEMA

    if driver_version and driver_version.get('schema') == 'nvidia_driver':
      assert os_type == 'win', 'Nvidia driver schema is only for Windows'
      is_nvidia = (format(vendor_id, '#04x') == '0x10de')
      assert is_nvidia, 'Nvidia driver schema is only for Nvidia GPUs'
      valid_version = check_nvidia_driver_version(driver_version['value'])
      if 'value2' in driver_version:
        valid_version = (valid_version and
                         check_nvidia_driver_version(driver_version['value2']))
      assert valid_version, NVIDIA_DRIVER_VERSION_SCHEMA

    write_driver_info(entry_id, is_exception, exception_id, driver_vendor,
                      driver_version, unique_symbol_id,
                      data_file, data_helper_file)
  else:
    data_file.write('nullptr,  // driver info\n')
  # group GL strings
  gl_strings = None
  if (gl_vendor != '' or gl_renderer != '' or gl_extensions != '' or
      gl_version_string != ''):
    gl_strings = [gl_vendor, gl_renderer, gl_extensions, gl_version_string]
  write_gl_strings(entry_id, is_exception, exception_id, gl_strings,
                   unique_symbol_id, data_file, data_helper_file)
  # group machine model info
  write_machine_model_info(entry_id, is_exception, exception_id,
                           machine_model_name, machine_model_version,
                           data_file, data_helper_file)
  write_intel_conditions(entry_id, is_exception, exception_id,
                         intel_gpu_series_list, intel_gpu_generation,
                         data_file, data_helper_file)
  # group a bunch of less used conditions
  if (gl_version != None or pixel_shader_version != None or in_process_gpu or
      gl_reset_notification_strategy != None or direct_rendering_version != None
      or gpu_count != None or hardware_overlay != None or test_group != 0 or
      subpixel_font_rendering != None):
    write_entry_more_data(entry_id, is_exception, exception_id, gl_type,
                          gl_version, pixel_shader_version, in_process_gpu,
                          gl_reset_notification_strategy,
                          direct_rendering_version, gpu_count, hardware_overlay,
                          test_group, subpixel_font_rendering,
                          data_file, data_helper_file)
  else:
    data_file.write('nullptr,  // more conditions\n')


def write_entry_more_data(entry_id, is_exception, exception_id, gl_type,
                          gl_version, pixel_shader_version, in_process_gpu,
                          gl_reset_notification_strategy,
                          direct_rendering_version, gpu_count, hardware_overlay,
                          test_group, subpixel_font_rendering, data_file,
                          data_helper_file):
  # write more data

  # Generate a unique name for jumbo build which concatenates multiple
  # translation units into one to speed compilation.
  basename = os.path.basename(data_helper_file.name)
  # & 0xffffffff converts to unsigned to keep consistent across Python versions
  # and platforms as per https://docs.python.org/3/library/zlib.html
  suffix = '_%s' % (zlib.crc32(basename.encode()) & 0xffffffff)
  var_name = 'kMoreForEntry' + str(entry_id) + suffix
  if is_exception:
    var_name += 'Exception' + str(exception_id)
  data_helper_file.write(
      'static const GpuControlList::More %s = {\n' % var_name)
  write_gl_type(gl_type, data_helper_file)
  write_version(gl_version, 'gl_version', data_helper_file)
  write_version(pixel_shader_version, 'pixel_shader_version', data_helper_file)
  write_boolean_value(in_process_gpu, 'in_process_gpu', data_helper_file)
  if not gl_reset_notification_strategy:
    gl_reset_notification_strategy = '0'
  data_helper_file.write('%s,  // gl_reset_notification_strategy\n' %
                         gl_reset_notification_strategy)
  write_version(direct_rendering_version, 'direct_rendering_version',
                data_helper_file)
  write_version(gpu_count, 'gpu_count', data_helper_file)
  write_supported_or_not(hardware_overlay, 'hardware_overlay', data_helper_file)
  write_integer_value(test_group, 'test_group', data_helper_file)
  write_supported_or_not(subpixel_font_rendering, 'subpixel_font_rendering',
                         data_helper_file)
  data_helper_file.write('};\n\n')
  # reference more data in entry
  data_file.write('&%s,  // more data\n' % var_name)


def write_intel_conditions(entry_id, is_exception, exception_id,
                           intel_gpu_series_list, intel_gpu_generation,
                           data_file, data_helper_file):
  # write intel conditions

  if not intel_gpu_series_list and not intel_gpu_generation:
    data_file.write('nullptr,  // Intel conditions\n')
    return

  var_name_series = None
  if intel_gpu_series_list:
    var_name_series = 'kIntelGpuSeriesForEntry' + str(entry_id)
    if is_exception:
      var_name_series += 'Exception' + str(exception_id)
    data_helper_file.write(
        'static const std::array<IntelGpuSeriesType, %d> %s = {{\n' %
        (len(intel_gpu_series_list), var_name_series))
    intel_gpu_series_map = {
      'broadwater': 'kBroadwater',
      'eaglelake': 'kEaglelake',
      'ironlake': 'kIronlake',
      'sandybridge': 'kSandybridge',
      'baytrail': 'kBaytrail',
      'ivybridge': 'kIvybridge',
      'haswell': 'kHaswell',
      'cherrytrail': 'kCherrytrail',
      'broadwell': 'kBroadwell',
      'apollolake': 'kApollolake',
      'skylake': 'kSkylake',
      'geminilake': 'kGeminilake',
      'amberlake': 'kAmberlake',
      'kabylake': 'kKabylake',
      'coffeelake': 'kCoffeelake',
      'whiskeylake': 'kWhiskeylake',
      'cometlake': 'kCometlake',
      'cannonlake': 'kCannonlake',
      'icelake': 'kIcelake',
      'elkhartlake': 'kElkhartlake',
      'jasperlake': 'kJasperlake',
      'tigerlake': 'kTigerlake',
      'rocketlake': 'kRocketlake',
      'dg1': 'kDG1',
      'alderlake': 'kAlderlake',
      'alchemist': 'kAlchemist',
      'raptorlake': 'kRaptorlake',
      'meteorlake': 'kMeteorlake',
      'arrowlake': 'kArrowlake',
      'lunarlake': 'kLunarlake',
      'battlemage': 'kBattlemage'
    }
    for series in intel_gpu_series_list:
      assert series in intel_gpu_series_map
      data_helper_file.write('IntelGpuSeriesType::%s,\n' %
                             intel_gpu_series_map[series])
    data_helper_file.write('}};\n\n')

  # Generate a unique name for jumbo build which concatenates multiple
  # translation units into one to speed compilation.
  basename = os.path.basename(data_helper_file.name)
  # & 0xffffffff converts to unsigned to keep consistent across Python versions
  # and platforms as per https://docs.python.org/3/library/zlib.html
  suffix = '_%s' % (zlib.crc32(basename.encode()) & 0xffffffff)
  var_name = 'kIntelConditionsForEntry' + str(entry_id) + suffix
  if is_exception:
    var_name += 'Exception' + str(exception_id)
  data_helper_file.write(
      'static const GpuControlList::IntelConditions %s = {\n' % var_name)
  if var_name_series:
    data_helper_file.write(
        'base::span(%s),  // intel_gpu_series_list\n' % var_name_series)
  else:
    data_helper_file.write(
        'base::span<const IntelGpuSeriesType>(),  // intel_gpu_series_list\n')
  write_version(intel_gpu_generation, 'intel_gpu_generation', data_helper_file)
  data_helper_file.write('};\n\n')

  data_file.write('&%s,  // Intel conditions\n' % var_name)


def write_entry(entry, total_feature_set, feature_name_prefix,
                unique_symbol_id, data_file, data_helper_file,
                data_exception_file):
  data_file.write('{\n')
  # ID
  entry_id = entry['id']
  data_file.write('%d,  // id\n' % entry_id)
  description = entry['description']
  if 'driver_update_url' in entry:
    description += (' Please update your graphics driver via this link: ' +
                    entry['driver_update_url'])
  data_file.write('"%s",\n' % description)
  # Features
  if 'features' in entry:
    var_name = 'kFeatureListFor%sEntry%d' % (unique_symbol_id, entry_id)
    features = entry['features']
    feature_set = get_feature_set(features, total_feature_set)
    data_file.write('base::span(%s),  // features\n' % var_name)
    write_features(feature_set, feature_name_prefix, var_name, data_helper_file)
  else:
    data_file.write('base::span<const int>(),  // features\n')
  # Disabled extensions
  write_disabled_extension_list('DisabledExtensions', entry_id,
                                entry.get('disabled_extensions', None),
                                data_file, data_helper_file)
  # Disabled WebGL extensions
  write_disabled_extension_list('DisabledWebGLExtensions', entry_id,
                                entry.get('disabled_webgl_extensions', None),
                                data_file, data_helper_file)
  # webkit_bugs are skipped because there is only one entry that has it.
  # cr_bugs
  cr_bugs = None
  if 'cr_bugs' in entry:
    cr_bugs = entry['cr_bugs']
  write_number_list(entry_id, 'uint32_t', 'CrBugs', cr_bugs, False, -1,
                    unique_symbol_id, data_file, data_helper_file)
  # Conditions
  data_file.write('{\n')
  write_conditions(entry_id, False, -1, entry, unique_symbol_id,
                   data_file, data_helper_file, data_exception_file)
  data_file.write('},\n')
  # Exceptions
  if 'exceptions' in entry:
    exceptions = entry['exceptions']
    exception_count = len(exceptions)
    exception_var = 'kExceptionsForEntry' + str(entry_id)
    data_exception_file.write(
        'static const std::array<GpuControlList::Conditions, %d> %s = {{\n' %
        (exception_count, exception_var))
    for index in range(exception_count):
      exception = exceptions[index]
      if 'device_id' in exception and 'vendor_id' not in exception:
        assert 'vendor_id' in entry
        exception['vendor_id'] = entry['vendor_id']
      data_exception_file.write('{\n')
      write_conditions(entry_id, True, index, exception, unique_symbol_id,
                       data_exception_file, data_helper_file, None)
      data_exception_file.write('},\n')
    data_exception_file.write('}};\n\n')
    data_file.write('base::span(%s),  // exceptions\n' % exception_var)
  else:
    data_file.write(
        'base::span<const GpuControlList::Conditions>(),  // exceptions\n')
  # END
  data_file.write('},\n')


def format_files(generated_files):
  formatter = "clang-format"
  if platform.system() == "Windows":
    formatter += ".bat"
  for filename in generated_files:
    call([formatter, "-i", "-style=chromium", filename])


def write_header_file_guard(out_file, filename, path, begin):
  token = (path.upper().replace('/', '_') + '_' +
           filename.upper().replace('.', '_') + '_')
  if begin:
    out_file.write('#ifndef %s\n#define %s\n\n' % (token, token))
  else:
    out_file.write('\n#endif  // %s\n' % token)


def process_json_file(json_filepath, list_tag,
                      feature_header_filename, total_features, feature_tag,
                      output_header_filepath, output_data_filepath,
                      output_helper_filepath, output_exception_filepath, path,
                      export_tag, git_format, os_filter, unique_symbol_id):
  output_header_filename = os.path.basename(output_header_filepath)
  output_helper_filename = os.path.basename(output_helper_filepath)
  output_exception_filename = os.path.basename(output_exception_filepath)
  json_file = open(json_filepath, 'rb')
  json_data = json.load(json_file)
  json_file.close()
  data_file = open(output_data_filepath, 'w')
  data_file.write(_LICENSE)
  data_file.write(_DO_NOT_EDIT_WARNING)
  data_file.write('#include "%s/%s"\n\n' % (path, output_header_filename))
  data_file.write('#include <array>\n')
  data_file.write('#include <iterator>\n\n')
  data_file.write('#include "gpu/config/%s"\n\n' % feature_header_filename)
  data_helper_file = open(output_helper_filepath, 'w')
  data_helper_file.write(_LICENSE)
  data_helper_file.write(_DO_NOT_EDIT_WARNING)
  write_header_file_guard(data_helper_file, output_helper_filename, path, True)
  data_exception_file = open(output_exception_filepath, 'w')
  data_exception_file.write(_LICENSE)
  data_exception_file.write(_DO_NOT_EDIT_WARNING)
  write_header_file_guard(data_exception_file, output_exception_filename, path,
                          True)
  data_file.write('namespace gpu {\n\n')
  entry_count = 0
  ids = []
  for index in range(len(json_data['entries'])):
    entry = json_data['entries'][index]
    entry_id = entry['id']
    assert entry_id not in ids
    ids.append(entry_id)
    if 'os' in entry:
      os_type = entry['os']['type']
      # Check for typos in the .json data
      if os_type not in _OS_TYPE_MAP:
        raise Exception('Unknown OS type "%s" for entry %d' %
                        (os_type, entry_id))
      if os_filter not in (None, os_type):
        continue
    entry_count += 1
  data_file.write(
      'const std::array<GpuControlList::Entry, %d>& Get%sEntries() {\n\n' %
      (entry_count, list_tag))
  data_file.write('#include "%s/%s"\n' % (path, output_helper_filename))
  data_file.write('#include "%s/%s"\n\n' % (path, output_exception_filename))
  data_file.write(
      'static const std::array<GpuControlList::Entry, %d> k%sEntries = {{\n' %
      (entry_count, list_tag))
  for index in range(len(json_data['entries'])):
    entry = json_data['entries'][index]
    entry_id = entry['id']
    if 'os' in entry:
      os_type = entry['os']['type']
      if os_filter not in (None, os_type):
        continue
    write_entry(entry, total_features, feature_tag, unique_symbol_id,
                data_file, data_helper_file, data_exception_file)
  data_file.write('}};\n')
  data_file.write('return k%sEntries;\n' % list_tag)
  data_file.write('}\n')
  data_file.write('}  // namespace gpu\n')
  data_file.close()
  write_header_file_guard(data_helper_file, output_helper_filename, path, False)
  data_helper_file.close()
  write_header_file_guard(data_exception_file, output_exception_filename, path,
                          False)
  data_exception_file.close()
  data_header_file = open(output_header_filepath, 'w')
  data_header_file.write(_LICENSE)
  data_header_file.write(_DO_NOT_EDIT_WARNING)
  write_header_file_guard(data_header_file, output_header_filename, path, True)
  data_header_file.write('#include <array>\n\n')
  data_header_file.write('#include "gpu/config/gpu_control_list.h"\n\n')
  data_header_file.write('namespace gpu {\n')
  func_dec = (
      '%sextern const std::array<GpuControlList::Entry, %d>& Get%sEntries();\n')
  data_header_file.write(func_dec % (export_tag, entry_count, list_tag))
  data_header_file.write('}  // namespace gpu\n')
  write_header_file_guard(data_header_file, output_header_filename, path, False)
  data_header_file.close()
  if git_format:
    format_files([output_header_filepath, output_data_filepath,
                  output_helper_filepath, output_exception_filepath])


def process_software_rendering_list(script_dir, output_dir, os_filter):
  total_features = load_software_rendering_list_features(
      os.path.join(script_dir, 'gpu_feature_type.h'))
  process_json_file(
      os.path.join(script_dir, 'software_rendering_list.json'),
      'SoftwareRenderingList',
      'gpu_feature_type.h',
      total_features,
      'GPU_FEATURE_TYPE_',
      os.path.join(output_dir, 'software_rendering_list_autogen.h'),
      os.path.join(output_dir, 'software_rendering_list_autogen.cc'),
      os.path.join(output_dir,
                   'software_rendering_list_arrays_and_structs_autogen.h'),
      os.path.join(output_dir, 'software_rendering_list_exceptions_autogen.h'),
      'gpu/config',
      'GPU_EXPORT ',
      False,
      os_filter,
      'Software')


def process_gpu_driver_bug_list(script_dir, output_dir, os_filter):
  total_features = load_gpu_driver_bug_workarounds(
      os.path.join(output_dir, 'gpu_driver_bug_workaround_autogen.h'))
  process_json_file(
      os.path.join(script_dir, 'gpu_driver_bug_list.json'),
      'GpuDriverBugList',
      'gpu_driver_bug_workaround_type.h',
      total_features,
      '',
      os.path.join(output_dir, 'gpu_driver_bug_list_autogen.h'),
      os.path.join(output_dir, 'gpu_driver_bug_list_autogen.cc'),
      os.path.join(output_dir,
                   'gpu_driver_bug_list_arrays_and_structs_autogen.h'),
      os.path.join(output_dir, 'gpu_driver_bug_list_exceptions_autogen.h'),
      'gpu/config',
      'GPU_EXPORT ',
      False,
      os_filter,
      'Workarounds')


def process_gpu_control_list_testing(script_dir, output_dir):
  total_features = ['test_feature_0', 'test_feature_1', 'test_feature_2']
  process_json_file(
      os.path.join(script_dir, 'gpu_control_list_testing.json'),
      'GpuControlListTesting',
      'gpu_control_list_testing_data.h',
      total_features,
      '',
      os.path.join(output_dir, 'gpu_control_list_testing_autogen.h'),
      os.path.join(output_dir, 'gpu_control_list_testing_autogen.cc'),
      os.path.join(output_dir,
                   'gpu_control_list_testing_arrays_and_structs_autogen.h'),
      os.path.join(output_dir, 'gpu_control_list_testing_exceptions_autogen.h'),
      'gpu/config',
      '',
      True,
      None,
      'GpuControlTesting')


def write_test_entry_enums(input_json_filepath, output_entry_enums_filepath,
                           path, list_tag):
  json_file = open(input_json_filepath, 'rb')
  json_data = json.load(json_file)
  json_file.close()

  output_entry_enums_filename = os.path.basename(output_entry_enums_filepath)
  enum_file = open(output_entry_enums_filepath, 'w')
  enum_file.write(_LICENSE)
  enum_file.write(_DO_NOT_EDIT_WARNING)
  write_header_file_guard(enum_file, output_entry_enums_filename, path, True)
  enum_file.write('namespace gpu {\n')
  enum_file.write('enum %sEntryEnum {\n' % list_tag)
  entry_count = len(json_data['entries'])
  for index in range(entry_count):
    entry = json_data['entries'][index]
    entry_id = entry['id']
    description = entry['description']
    assert(index + 1 == int(entry_id))
    description = 'k' + description
    description = description.replace('.', '_')
    enum_file.write('  %s = %d,\n' % (description, index))
  enum_file.write('};\n')
  enum_file.write('}  // namespace gpu\n')
  write_header_file_guard(enum_file, output_entry_enums_filename, path, False)
  enum_file.close()
  format_files([output_entry_enums_filepath])


def main(argv):
  parser = OptionParser()
  parser.add_option("--output-dir",
                    help="output directory for SoftwareRenderingList and "
                    "GpuDriverBugList data files. "
                    "If unspecified, these files are not generated.")
  parser.add_option("--skip-testing-data", action="store_false",
                    dest="generate_testing_data", default=True,
                    help="skip testing data generation.")
  parser.add_option("--os-filter",
                    help="only output entries applied to the specified os.")
  (options, _) = parser.parse_args(args=argv)

  script_dir = os.path.dirname(os.path.realpath(__file__))

  if options.output_dir != None:
    process_software_rendering_list(
        script_dir, options.output_dir, options.os_filter)
    process_gpu_driver_bug_list(
        script_dir, options.output_dir, options.os_filter)

  if options.generate_testing_data:
    # Testing data files are generated by calling the script manually.
    process_gpu_control_list_testing(script_dir, script_dir)
    write_test_entry_enums(
        os.path.join(script_dir, 'gpu_control_list_testing.json'),
        os.path.join(script_dir,
                     'gpu_control_list_testing_entry_enums_autogen.h'),
        'gpu/config',
        'GpuControlListTesting')


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

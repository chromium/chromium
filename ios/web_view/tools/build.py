#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Builds and packages ChromeWebView.framework.
"""

import argparse
import os
import shutil
import sys

def target_dir_name(build_config, target_device):
  """Returns a default output directory name string.

  Args:
    build_config: A string describing the build configuration. Ex: 'Debug'
    target_device: A string describing the target device. Ex: 'simulator'
  """
  return '%s-%s' % (build_config, target_device)

def build(build_config, target_device, extra_gn_options, extra_ninja_options):
  """Generates and builds ChromeWebView.framework.

  Args:
    build_config: A string describing the build configuration. Ex: 'Debug'
    target_device: A string describing the target device. Ex: 'simulator'
    extra_gn_options: A string of gn args (space separated key=value items) to
      be appended to the gn gen command.
    extra_ninja_options: A string of gn options to be appended to the ninja
      command.

  Returns:
    The return code of generating ninja if it is non-zero, else the return code
      of the ninja build command.
  """
  if target_device == 'iphoneos':
    target_cpu = 'arm64'
  else:
    target_cpu = 'x64'

  if build_config == 'Debug':
    build_config_gn_args = 'is_debug=true'
  else:
    build_config_gn_args = ('is_debug=false enable_stripping=true '
                            'is_official_build=true')

  build_dir = os.path.join("out", target_dir_name(build_config, target_device))
  gn_args = ('target_os="ios" enable_websockets=false '
            'is_component_build=false use_xcode_clang=false '
            'disable_file_support=true disable_ftp_support=true '
            'disable_brotli_filter=true ios_enable_code_signing=false '
            'enable_dsyms=true '
            'target_cpu="%s" %s %s' %
            (target_cpu, build_config_gn_args, extra_gn_options))

  gn_command = 'gn gen %s --args=\'%s\'' % (build_dir, gn_args)
  print gn_command
  gn_result = os.system(gn_command)
  if gn_result != 0:
    return gn_result

  ninja_options = '-C %s' % build_dir
  if extra_ninja_options:
    ninja_options += ' %s' % extra_ninja_options
  ninja_command = ('ninja %s ios/web_view:ios_web_view_package' %
                   ninja_options)
  print ninja_command
  return os.system(ninja_command)

def copy_build_products(build_config, target_device, out_dir, output_name):
  """Copies the resulting framework and symbols to out_dir.

  Args:
    build_config: A string describing the build configuration. Ex: 'Debug'
    target_device: A string describing the target device. Ex: 'simulator'
    out_dir: A string to the path which all build products will be copied.
  """
  target_dir = target_dir_name(build_config, target_device)
  build_dir = os.path.join("out", target_dir)
  package_dir = os.path.join(build_dir, 'ios_web_view')

  # # Copy framework.
  framework_name = '%s.framework' % output_name
  framework_source = os.path.join(build_dir, framework_name)
  framework_dest = os.path.join(out_dir, target_dir, framework_name)
  print 'Copying %s to %s' % (framework_source, framework_dest)
  shutil.copytree(framework_source, framework_dest)

  # Copy symbols.
  symbols_name = '%s.dSYM' % output_name
  symbols_source = os.path.join(build_dir, symbols_name)
  symbols_dest = os.path.join(out_dir, target_dir, symbols_name)
  print 'Copying %s to %s' % (symbols_source, symbols_dest)
  shutil.copytree(symbols_source, symbols_dest)

def package_framework(build_config,
                      target_device,
                      out_dir,
                      output_name,
                      extra_gn_options,
                      extra_ninja_options):
  """Builds ChromeWebView.framework and copies the result to out_dir.

  Args:
    build_config: A string describing the build configuration. Ex: 'Debug'
    target_device: A string describing the target device. Ex: 'simulator'
    out_dir: A string to the path which all build products will be copied.
    extra_gn_options: A string of gn args (space separated key=value items) to
      be appended to the gn gen command.
    extra_ninja_options: A string of gn options to be appended to the ninja
      command.

  Returns:
    The return code of the build if it fails or 0 if the build was successful.
  """
  print '\nBuilding for %s (%s)' % (target_device, build_config)

  build_result = build(build_config,
                       target_device,
                       extra_gn_options,
                       extra_ninja_options)
  if build_result != 0:
    error = 'Building %s/%s failed with code: ' % (build_config, target_device)
    print >>sys.stderr, error, build_result
    return build_result
  copy_build_products(build_config, target_device, out_dir, output_name)
  return 0

def package_all_frameworks(out_dir, output_name, extra_gn_options,
                           build_configs, target_devices, extra_ninja_options):
  """Builds ChromeWebView.framework.

  Builds Release and Debug versions of ChromeWebView.framework for both
    iOS devices and simulator and copies the resulting frameworks into out_dir.

  Args:
    out_dir: A string to the path which all build products will be copied.
    extra_gn_options: A string of gn args (space separated key=value items) to
      be appended to the gn gen command.
    build_configs: A list of configs to build.
    target_devices: A list of devices to target.
    extra_ninja_options: A string of gn options to be appended to the ninja
      command.

  Returns:
    0 if all builds are successful or 1 if any build fails.
  """
  print 'Building ChromeWebView.framework...'

  # Package all builds in the output directory
  os.makedirs(out_dir)

  configs_and_devices = [(a,b) for a in build_configs for b in target_devices]
  for build_config, target_device in configs_and_devices:
    if package_framework(build_config,
                         target_device,
                         out_dir,
                         output_name,
                         extra_gn_options,
                         extra_ninja_options) != 0:
      return 1

  # Copy common files from last built package to out_dir.
  build_dir = os.path.join('out', target_dir_name('Release', 'iphoneos'))
  package_dir = os.path.join(build_dir, 'ios_web_view')
  shutil.copy2(os.path.join(package_dir, 'AUTHORS'), out_dir)
  shutil.copy2(os.path.join(package_dir, 'LICENSE'), out_dir)
  shutil.copy2(os.path.join(package_dir, 'VERSION'), out_dir)

  print '\nSuccess! ChromeWebView.framework is packaged into %s' % out_dir

  return 0

def main():
  description = 'Build and package //ios/web_view.'
  parser = argparse.ArgumentParser(description=description)

  parser.add_argument('out_dir', nargs='?', default='out/IOSWebViewBuild',
                      help='path to output directory')
  parser.add_argument('--no_goma', action='store_true',
                      help='Prevents adding use_goma=true to the gn args.')
  parser.add_argument('--ninja_args',
                      help='Additional gn args to pass through to ninja.')
  parser.add_argument('--include_cronet', action='store_true',
                      help='Combines Cronet and ChromeWebView as 1 framework.')
  parser.add_argument('--enable_sync', action='store_true',
                      help='Enables public API for sync.')
  parser.add_argument('--enable_autofill', action='store_true',
                      help='Enables public API for autofill.')
  build_configs = ['Debug', 'Release']
  target_devices = ['iphonesimulator', 'iphoneos']
  parser.add_argument('--build_configs', nargs='+', default=build_configs,
                      choices=build_configs,
                      help='Specify which configs to build.')
  parser.add_argument('--target_devices', nargs='+', default=target_devices,
                      choices=target_devices,
                      help='Specify which devices to target.')

  options, extra_options = parser.parse_known_args()
  print 'Options:', options

  if len(extra_options):
    print >>sys.stderr, 'Unknown options: ', extra_options
    return 1

  out_dir = options.out_dir
  # Make sure that the output directory does not exist
  if os.path.exists(out_dir):
    print >>sys.stderr, 'The output directory already exists: ' + out_dir
    return 1

  output_name = 'ChromeWebView'
  extra_gn_options = ''
  if not options.no_goma:
    extra_gn_options += 'use_goma=true '
  if options.include_cronet:
    extra_gn_options += 'ios_web_view_include_cronet=true '
    output_name = 'CronetChromeWebView'
  else:
    extra_gn_options += 'ios_web_view_include_cronet=false '
  if options.enable_sync:
    extra_gn_options += 'ios_web_view_enable_sync=true '
  else:
    extra_gn_options += 'ios_web_view_enable_sync=false '
  if options.enable_autofill:
    extra_gn_options += 'ios_web_view_enable_autofill=true '
  else:
    extra_gn_options += 'ios_web_view_enable_autofill=false '
  extra_gn_options += 'ios_web_view_output_name="%s" ' % output_name
  # This prevents Breakpad from being included in the final binary to avoid
  # duplicate symbols with the client app.
  extra_gn_options += 'use_crash_key_stubs=true '

  return package_all_frameworks(out_dir, output_name, extra_gn_options,
                                set(options.build_configs),
                                set(options.target_devices),
                                options.ninja_args)

if __name__ == '__main__':
  sys.exit(main())

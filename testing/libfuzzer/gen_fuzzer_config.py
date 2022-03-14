#!/usr/bin/env python3
#
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate or update an existing config (.options file) for libfuzzer test.

Invoked by GN from fuzzer_test.gni.
"""

import argparse
import os
import sys

if sys.version_info.major == 2:
    from ConfigParser import ConfigParser
else:
    from configparser import ConfigParser


def AddSectionOptions(config, section_name, options):
  """Add |options| to the |section_name| section of |config|.

  Throws an
  assertion error if any option in |options| does not have exactly two
  elements.
  """
  if not options:
    return

  config.add_section(section_name)
  for option_and_value in options:
    assert len(option_and_value) == 2, (
        '%s is not an option, value pair' % option_and_value)

    config.set(section_name, *option_and_value)


def main():
  parser = argparse.ArgumentParser(description='Generate fuzzer config.')
  parser.add_argument('--config', required=True)
  parser.add_argument('--dict')
  parser.add_argument('--libfuzzer_options', nargs='+', default=[])
  parser.add_argument('--asan_options', nargs='+', default=[])
  parser.add_argument('--msan_options', nargs='+', default=[])
  parser.add_argument('--ubsan_options', nargs='+', default=[])
  parser.add_argument('--grammar_options', nargs='+', default=[])
  parser.add_argument(
      '--environment_variables',
      nargs='+',
      default=[],
      choices=['AFL_DRIVER_DONT_DEFER=1'])
  args = parser.parse_args()

  # Script shouldn't be invoked without any arguments, but just in case.
  if not (args.dict or args.libfuzzer_options or args.environment_variables or
          args.asan_options or args.msan_options or args.ubsan_options or
          args.grammar_options):
    return

  config = ConfigParser()
  libfuzzer_options = []
  if args.dict:
    libfuzzer_options.append(('dict', os.path.basename(args.dict)))
  libfuzzer_options.extend(
      option.split('=') for option in args.libfuzzer_options)

  AddSectionOptions(config, 'libfuzzer', libfuzzer_options)

  AddSectionOptions(config, 'asan',
                    [option.split('=') for option in args.asan_options])

  AddSectionOptions(config, 'msan',
                    [option.split('=') for option in args.msan_options])

  AddSectionOptions(config, 'ubsan',
                    [option.split('=') for option in args.ubsan_options])

  AddSectionOptions(config, 'grammar',
                    [option.split('=') for option in args.grammar_options])

  AddSectionOptions(
      config, 'env',
      [option.split('=') for option in args.environment_variables])

  # Generate .options file.
  config_path = args.config
  with open(config_path, 'w') as options_file:
    options_file.write(
        '# This is an automatically generated config for ClusterFuzz.\n')
    config.write(options_file)


if __name__ == '__main__':
  main()

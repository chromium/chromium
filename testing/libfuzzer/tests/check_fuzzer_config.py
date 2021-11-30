#!/usr/bin/env python2
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script that prints out "option=value" from the config file. Used for testing.

import ConfigParser
import os
import sys

OPTIONS_SECTION_LIBFUZZER = 'libfuzzer'

config_path = os.path.join(os.path.dirname(sys.argv[0]), sys.argv[1])
fuzzer_config = ConfigParser.ConfigParser()
fuzzer_config.read(config_path)

if not fuzzer_config.has_section(OPTIONS_SECTION_LIBFUZZER):
  sys.exit(-1)

for option_name, option_value in fuzzer_config.items(OPTIONS_SECTION_LIBFUZZER):
  sys.stdout.write('%s=%s\n' % (option_name, option_value))

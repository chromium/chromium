# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from gpu_tests import path_util

path_util.AddDirToPathIfNeeded(path_util.GetChromiumSrcDir(), 'tools', 'perf')

# TODO(crbug.com/1289421): Remove this disable.
# pylint: disable=wrong-import-position
from chrome_telemetry_build import chromium_config
# pylint: enable=wrong-import-position

CONFIG = chromium_config.ChromiumConfig(
    top_level_dir=path_util.GetGpuTestDir(),
    benchmark_dirs=[os.path.join(path_util.GetGpuTestDir(), 'gpu_tests')])

# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import gpu_path_util
# pylint: disable=unused-import
from gpu_path_util import setup_tools_perf_paths
# pylint: enable=unused-import

# This needs to come after setup_tools_perf_paths so we can actually import
# this.
# pylint: disable=wrong-import-order
from chrome_telemetry_build import chromium_config
# pylint: enable=wrong-import-order

CONFIG = chromium_config.ChromiumConfig(
    top_level_dir=gpu_path_util.GPU_DIR,
    benchmark_dirs=[os.path.join(gpu_path_util.GPU_DIR, 'gpu_tests')])

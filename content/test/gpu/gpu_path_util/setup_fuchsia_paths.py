# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gpu_path_util

gpu_path_util.AddDirToPathIfNeeded(gpu_path_util.CHROMIUM_SRC_DIR, 'build',
                                   'fuchsia', 'test')

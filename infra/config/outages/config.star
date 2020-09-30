# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _outages_config(*, disable_cq_experiments = False):
    return struct(
        disable_cq_experiments = disable_cq_experiments,
    )

# See README.md for documentation on allowable configuration values
config = _outages_config(
)

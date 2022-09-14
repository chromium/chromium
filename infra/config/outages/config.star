# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _outages_config(*, disable_cq_experiments = False):
    return struct(
        disable_cq_experiments = disable_cq_experiments,
    )

# The default value that is used by a generator to compare against the current
# config to output the effective outages configuration
DEFAULT_CONFIG = _outages_config()

# See README.md for documentation on allowable configuration values
config = _outages_config(
)

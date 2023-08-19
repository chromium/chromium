#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for determining GPU test shard times.

Reported data should be taken as estimates rather than concrete numbers since
data will not be precise when failed, timed out, or infra-failed tasks are
present.
"""

from machine_times import get_machine_times

if __name__ == '__main__':
  get_machine_times.main()

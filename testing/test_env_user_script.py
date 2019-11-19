#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for use in test_env unittests."""

import test_env


if __name__ == '__main__':
  test_env.run_command(['python', 'test_env_test_script.py'])

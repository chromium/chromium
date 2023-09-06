#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.web_tests.flake_suppressor import (suppress_flakes)

import sys

if __name__ == '__main__':
    rc = suppress_flakes.main()
    sys.exit(rc)

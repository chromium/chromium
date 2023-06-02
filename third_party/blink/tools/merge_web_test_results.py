#!/usr/bin/env vpython3
#
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

if __name__ == '__main__':
    from blinkpy.web_tests.merge_results import main
    main(sys.argv[1:])

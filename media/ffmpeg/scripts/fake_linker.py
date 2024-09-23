#!/usr/bin/env python
#
# Copyright 2018 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Find '-o' and create a file with that name if it doesn't already exist.
# Ignore everything else.

import os
import sys

for idx, arg in enumerate(sys.argv[1:]):
    filename = None
    if arg == '-o':
        # Hopefully there is an |idx+1|.
        filename = sys.argv[1:][idx + 1]
    if arg.startswith('-out:'):
        # also handle lld-link argument
        filename = arg.split(':')[-1]
    if filename != None:
        # If the file exists, then take no action.
        if not os.path.exists(filename):
            print("creating fake linker output file: %s" % filename)
            open(filename, "w+").close()
        sys.exit(0)

print("Please supply a '-o filename\' somewhere on the command line")
sys.exit(1)

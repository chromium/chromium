#!/usr/bin/env python
#
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Removes spaces from a given string."""

from __future__ import print_function

import sys

def main():
  if len(sys.argv) < 1:
    print('Usage: %s <string>' % sys.argv[0])
    sys.exit(1)

  # Takes all arguments passed in, joins as one string, and removes spaces.
  nospace = "".join(sys.argv[1:]).replace(" ", "")
  print(nospace)

if __name__ == '__main__':
  sys.exit(main())

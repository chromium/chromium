#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

def DoMain(args):
  if len(args) != 1:
    sys.exit(1)
  with open(args[0], "w") as f:
    f.writelines("module mojo.test.generated_test_mojom; interface Foo {};")

if __name__ == '__main__':
  DoMain(sys.argv[1:])

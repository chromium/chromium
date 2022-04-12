#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys

_TOOLS_DIR = os.path.dirname(__file__)
_MOJOM_DIR = os.path.join(_TOOLS_DIR, 'mojom')
_SRC_DIR = os.path.join(_TOOLS_DIR, os.path.pardir, os.path.pardir,
                        os.path.pardir)

# Ensure that the mojom library is discoverable.
sys.path.append(_MOJOM_DIR)

# Help Python find typ in //third_party/catapult/third_party/typ/
sys.path.append(
    os.path.join(_SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ'))
import typ


def Main():
  return typ.main(top_level_dir=_MOJOM_DIR)


if __name__ == '__main__':
  sys.exit(Main())

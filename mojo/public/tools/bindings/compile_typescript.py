# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import argparse

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..', '..'))

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
import node_modules

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--tsconfig_path', required=True)
  args = parser.parse_args(argv)

  result = node.RunNode([node_modules.PathToTypescript()] +
                        ['--project', args.tsconfig_path])
  if len(result) != 0:
    raise RuntimeError('Failed to compile Typescript: \n%s' % result)

if __name__ == '__main__':
  main(sys.argv[1:])

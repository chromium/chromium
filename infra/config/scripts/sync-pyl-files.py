#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sync generated *.pyl files to //testing/buildbot.

After modifying the starlark and running it to regenerate configs, if
mixins.pyl has been modified, this script should be run to sync it to
//testing/buildbot. The script can be run with the --check flag to
indicate whether a sync needs to be performed.

mixins.pyl needs to be present in //testing/buildbot because the
directory is exported to a separate repo that the angle repo includes as
a dep in order to reuse the mixin definitions.
"""

import argparse
import filecmp
import os.path
import shutil
import sys

INFRA_CONFIG_DIR = os.path.abspath(f'{__file__}/../..')
TESTING_BUILDBOT_DIR = os.path.normpath(
    f'{INFRA_CONFIG_DIR}/../../testing/buildbot')
GENERATED_TESTING_DIR = os.path.normpath(
    f'{INFRA_CONFIG_DIR}/generated/testing')


def copy_file(src, dst):
  shutil.copyfile(src, dst)
  return None


_DOC_LINK = ('https://chromium.googlesource.com/chromium/src'
             '/+/HEAD/infra/config/targets#tests-in-starlark')


def check_file(src, dst):
  if os.path.exists(dst) and filecmp.cmp(src, dst):
    return None
  return ('files in //testing/buildbot differ from those in'
          f' //infra/config/generated/testing, see {_DOC_LINK} for information'
          ' on the process for updating pyl files')


def parse_args(argv):
  parser = argparse.ArgumentParser()
  parser.set_defaults(func=copy_file)
  parser.add_argument('--check',
                      help='check that files are synced',
                      action='store_const',
                      dest='func',
                      const=check_file)
  return parser.parse_args(argv)


def main(args):
  error = args.func(os.path.normpath(f'{GENERATED_TESTING_DIR}/mixins.pyl'),
                    os.path.normpath(f'{TESTING_BUILDBOT_DIR}/mixins.pyl'))
  if error is not None:
    print(error, file=sys.stderr)
    return 1
  return 0


if __name__ == '__main__':
  args = parse_args(sys.argv[1:])
  sys.exit(main(args))

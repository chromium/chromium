#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sync generated *.pyl files to //testing/buildbot.

After modifying the starlark and running it to regenerate configs, if
any files in //infra/config/generated/testing are modified, this script
should be run to sync them to //testing/buildbot. The script can be run
with the --check flag to indicate whether any files are out-of-date.

The *.pyl files need to be present in //testing/buildbot for multiple
reasons:
* The angle repo uses the *.pyl files in //testing/buildbot via a repo
  that mirrors //testing so that the definitions do not need to be kept
  in sync.
* pinpoint builds revisions not at head, so until all revisions in scope
  for pinpoint contain
  //infra/config/generated/testing/gn_isolate_map.pyl, the config
  located in recipes cannot be updated to refer to that location.
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


def check_file(src, dst):
  if os.path.exists(dst) and filecmp.cmp(src, dst):
    return None
  return ('files in //testing/buildbot differ from those in'
          ' //infra/config/generated/testing,'
          ' please run //infra/config/scripts/sync-pyl-files.py')


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
  for f in ('gn_isolate_map.pyl', 'mixins.pyl', 'variants.pyl'):
    error = args.func(os.path.normpath(f'{GENERATED_TESTING_DIR}/{f}'),
                      os.path.normpath(f'{TESTING_BUILDBOT_DIR}/{f}'))
    if error is not None:
      print(error, file=sys.stderr)
      return 1
  return 0


if __name__ == '__main__':
  args = parse_args(sys.argv[1:])
  sys.exit(main(args))

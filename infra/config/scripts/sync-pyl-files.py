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


def copy_file(src, dst):
  shutil.copyfile(src, dst)
  return None


_DOC_LINK = (
  'https://chromium.googlesource.com/chromium/src'
  '/+/HEAD/infra/config/targets#tests-in-starlark'
)


def check_file(src, dst):
  if os.path.exists(dst) and filecmp.cmp(src, dst):
    return None
  return (
    'files in //testing/buildbot differ from those in'
    f' //infra/config/generated/testing, see {_DOC_LINK} for information'
    ' on the process for updating pyl files'
  )


def parse_args(argv):
  parser = argparse.ArgumentParser()
  parser.set_defaults(func=copy_file)
  parser.add_argument(
    '--check',
    help='check that files are synced',
    action='store_const',
    dest='func',
    const=check_file,
  )
  parser.add_argument(
    '--infra-config-dir',
    default=INFRA_CONFIG_DIR,
    help=(
      'infra/config directory whose generated/testing files are the source.'
      ' Defaults to the directory containing this script.'
    ),
  )
  parser.add_argument(
    '--testing-buildbot-dir',
    default=None,
    help=(
      'Directory to sync the files to. Defaults to ../../testing/buildbot'
      ' relative to --infra-config-dir.'
    ),
  )
  return parser.parse_args(argv)


def main(args):
  generated_testing_dir = f'{args.infra_config_dir}/generated/testing'
  testing_buildbot_dir = (
    args.testing_buildbot_dir
    or f'{args.infra_config_dir}/../../testing/buildbot'
  )
  error = args.func(
    os.path.normpath(f'{generated_testing_dir}/mixins.pyl'),
    os.path.normpath(f'{testing_buildbot_dir}/mixins.pyl'),
  )
  if error is not None:
    print(error, file=sys.stderr)
    return 1
  return 0


if __name__ == '__main__':
  args = parse_args(sys.argv[1:])
  sys.exit(main(args))

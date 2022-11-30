#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for updating the project settings for a chromium branch.

To initialize a new chromium branch, run the following from the root of
the repo (where MM is the milestone number and BBBB is the branch
number):
```
infra/config/scripts/branch.py initialize --milestone MM --branch BBBB
infra/config/main.star
infra/config/dev.star
```

Usage:
  branch.py initialize --milestone XX --branch YYYY
"""

import argparse
import json
import os

INFRA_CONFIG_DIR = os.path.abspath(os.path.join(__file__, '..', '..'))

def parse_args(args=None, *, parser_type=None):
  parser_type = parser_type or argparse.ArgumentParser
  parser = parser_type(
      description='Update the project settings for a chromium branch')
  parser.set_defaults(func=None)
  parser.add_argument('--settings-json',
                      help='Path to the settings.json file',
                      default=os.path.join(INFRA_CONFIG_DIR, 'settings.json'))

  subparsers = parser.add_subparsers()

  init_parser = subparsers.add_parser(
      'initialize', help='Initialize the settings for a branch')
  init_parser.set_defaults(func=initialize_cmd)
  init_parser.add_argument(
      '--milestone',
      required=True,
      help=('The milestone identifier '
            '(e.g. the milestone number for standard release channel)'))
  init_parser.add_argument(
      '--branch',
      required=True,
      help='The branch name, must correspond to a ref in refs/branch-heads')

  set_type_parser = subparsers.add_parser(
      'set-type', help='Change the branch type of the project')
  set_type_parser.set_defaults(func=set_type_cmd)
  set_type_parser.add_argument(
      '--type',
      required=True,
      choices=BRANCH_TYPES,
      action='append',
      help='The type of the branch to change the project config to')

  args = parser.parse_args(args)
  if args.func is None:
    parser.error('no sub-command specified')
  return args

def initial_settings(milestone, branch):
  settings = dict(
      project=f'chromium-m{milestone}',
      project_title=f'Chromium M{milestone}',
      ref=f'refs/branch-heads/{branch}',
      chrome_project=f'chrome-m{milestone}',
      branch_types=['standard'],
  )

  return json.dumps(settings, indent=4) + '\n'

def initialize_cmd(args):
  settings = initial_settings(args.milestone, args.branch)

  with open(args.settings_json, 'w') as f:
    f.write(settings)


BRANCH_TYPES = (
    'standard',
    'desktop-extended-stable',
    'cros-lts',
    'fuchsia-lts',
)


def set_type(settings_json, branch_types):
  for t in branch_types:
    assert t in BRANCH_TYPES, 'Unknown branch_type {!r}'.format(t)

  settings = json.loads(settings_json)
  settings.update(branch_types=branch_types)
  return json.dumps(settings, indent=4) + '\n'


def set_type_cmd(args):
  with open(args.settings_json) as f:
    settings = f.read()

  settings = set_type(settings, args.type)

  with open(args.settings_json, 'w') as f:
    f.write(settings)


def main():
  args = parse_args()
  args.func(args)

if __name__ == '__main__':
  main()

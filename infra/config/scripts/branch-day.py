#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for performing the branch day tasks.

This script will make all of the necessary binary invocations to update
input settings files based on flags and then re-generate the
configuration. No output will be produced unless one of the binary
invocations fails.

Config can be updated on a new branch with:
```
branch-day.py --on-branch --milestone MM --branch BBBB
```

Config on trunk for enabling the new branch can be updated with:
```
branch-day.py --milestone MM --branch BBBB
```
"""

import argparse
import os
import subprocess
import sys

INFRA_CONFIG_DIR = os.path.abspath(os.path.join(__file__, '..', '..'))


def parse_args(args=None, *, parser_type=None):
  parser_type = parser_type or argparse.ArgumentParser
  parser = parser_type(
      description='Update the project settings for a chromium branch')
  parser.set_defaults(func=_activate_milestone)
  parser.add_argument('--milestones-py',
                      help='Path to milestones.py script',
                      default=os.path.join(INFRA_CONFIG_DIR, 'scripts',
                                           'milestones.py'))
  parser.add_argument('--branch-py',
                      help='Path to branch.py script',
                      default=os.path.join(INFRA_CONFIG_DIR, 'scripts',
                                           'branch.py'))
  parser.add_argument('--main-star',
                      help='Path to main.star script',
                      default=os.path.join(INFRA_CONFIG_DIR, 'main.star'))
  parser.add_argument('--dev-star',
                      help='Path to dev.star script',
                      default=os.path.join(INFRA_CONFIG_DIR, 'dev.star'))

  parser.add_argument(
      '--milestone',
      required=True,
      help=('The milestone identifier '
            '(e.g. the milestone number for standard release channel)'))
  parser.add_argument(
      '--branch',
      required=True,
      help='The branch name, must correspond to a ref in refs/branch-heads')

  parser.add_argument(
      '--on-branch',
      action='store_const',
      dest='func',
      const=_initialize_branch,
      help='Switches to performing the branch day tasks on the new branch')

  return parser.parse_args(args)


def _execute(cmd):
  if os.name == 'nt':
    cmd = ['vpython3.bat'] + cmd
  try:
    subprocess.run(cmd,
                   check=True,
                   text=True,
                   stdout=subprocess.PIPE,
                   stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    print('Executing {} failed'.format(cmd), file=sys.stderr)
    end = '' if e.output[-1] == '\n' else '\n'
    print(e.output, file=sys.stderr, end=end)
    sys.exit(1)


def _activate_milestone(args):
  _execute([
      args.milestones_py, 'activate', '--milestone', args.milestone, '--branch',
      args.branch
  ])
  _execute([args.main_star])
  _execute([args.dev_star])


def _initialize_branch(args):
  _execute([
      args.branch_py, 'initialize', '--milestone', args.milestone, '--branch',
      args.branch
  ])
  _execute([args.main_star])
  _execute([args.dev_star])


def main():
  args = parse_args()
  args.func(args)


if __name__ == '__main__':
  main()

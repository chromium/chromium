#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for updating the active milestones for the chromium project.

To activate a new chromium branch, run the following from the root of
the repo (where MM is the milestone number and BBBB is the branch
number):
```
scripts/chromium/milestones.py activate --milestone MM --branch BBBB
./main.star
```

To deactivate a chromium branch, run the following from the root of the
repo (where MM is the milestone number):
```
scripts/chromium/milestones.py deactivate --milestone MM
./main.star
```

Usage:
  milestones.py activate --milestone XX --branch YYYY
  milestones.py deactivate --milestone XX
"""

import argparse
import itertools
import json
import os
import re
import sys

INFRA_CONFIG_DIR = os.path.abspath(os.path.join(__file__, '..', '..'))

def parse_args(args=None, *, parser_type=None):
  parser_type = parser_type or argparse.ArgumentParser
  parser = parser_type(
      description='Update the active milestones for the chromium project')
  parser.set_defaults(func=None)
  parser.add_argument('--milestones-json',
                      help='Path to the milestones.json file',
                      default=os.path.join(INFRA_CONFIG_DIR, 'milestones.json'))

  subparsers = parser.add_subparsers()

  activate_parser = subparsers.add_parser(
      'activate', help='Add an additional active milestone')
  activate_parser.set_defaults(func=activate_cmd)
  activate_parser.add_argument(
      '--milestone',
      required=True,
      help=('The milestone identifier '
            '(e.g. the milestone number for standard release channel)'))
  activate_parser.add_argument(
      '--branch',
      required=True,
      help='The branch name, must correspond to a ref in refs/branch-heads')

  deactivate_parser = subparsers.add_parser(
      'deactivate', help='Remove an active milestone')
  deactivate_parser.set_defaults(func=deactivate_cmd)
  deactivate_parser.add_argument(
      '--milestone',
      required=True,
      help=('The milestone identifier '
            '(e.g. the milestone number for standard release channel)'))

  args = parser.parse_args(args)
  if args.func is None:
    parser.error('no sub-command specified')
  return args

class MilestonesException(Exception):
  pass

_NUMBER_RE  = re.compile('([0-9]+)')

def numeric_sort_key(s):
  # The capture group in the regex means that the numeric portions are returned,
  # odd indices will be the numeric portions of the string (the 0th or last
  # element will be empty if the string starts or ends with a number,
  # respectively)
  pieces = _NUMBER_RE.split(s)
  return [
      (int(x), x) if is_numeric else x
      for x, is_numeric
      in zip(pieces, itertools.cycle([False, True]))
  ]

def add_milestone(milestones, milestone, branch):
  if milestone in milestones:
    raise MilestonesException(
        f'there is already an active milestone with id {milestone!r}: '
        f'{milestones[milestone]}')

  milestones[milestone] = {
      'name': f'm{milestone}',
      'project': f'chromium-m{milestone}',
      'ref': f'refs/branch-heads/{branch}',
  }

  milestones = {
      k: milestones[k] for k in sorted(milestones, key=numeric_sort_key)
  }

  return json.dumps(milestones, indent=4) + '\n'

def activate_cmd(args):
  with open(args.milestones_json) as f:
    milestones = json.load(f)

  milestones = add_milestone(milestones, args.milestone, args.branch)

  with open(args.milestones_json, 'w') as f:
    f.write(milestones)

def remove_milestone(milestones, milestone):
  if milestone not in milestones:
    raise MilestonesException(
        f'{milestone!r} does not refer to an active milestone: '
        f'{list(milestones.keys())}')

  del milestones[milestone]

  milestones = {
      k: milestones[k] for k in sorted(milestones, key=numeric_sort_key)
  }

  return json.dumps(milestones, indent=4) + '\n'

def deactivate_cmd(args):
  with open(args.milestones_json) as f:
    milestones = json.load(f)

  milestones = remove_milestone(milestones, args.milestone)

  with open(args.milestones_json, 'w') as f:
    f.write(milestones)

def main():
  args = parse_args()
  try:
    args.func(args)
  except MilestonesException as e:
    print(str(e), file=sys.stderr)
    sys.exit(1)

if __name__ == '__main__':
  main()
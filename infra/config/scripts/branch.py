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

from typing import Optional

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

  enable_platform_parser = subparsers.add_parser(
      'enable-platform', help='Enable builders for an additional platform')
  enable_platform_parser.set_defaults(func=enable_platform_cmd)
  enable_platform_parser.add_argument(
      'platform', help='The platform to enable builders for')
  enable_platform_parser.add_argument(
      '--description',
      required=True,
      help='A description of why the platform is enabled')
  enable_platform_parser.add_argument(
      '--sheriff-rotation',
      help=
      'A sheriff that builders associated with the platform should be added to')

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
      is_main=False,
      platforms={
          p: {
              "description": "beta/stable",
              "sheriff_rotation": "chrome_browser_release"
          }
          for p in (
              "android",
              "cros",
              "fuchsia",
              "ios",
              "linux",
              "mac",
              "windows",
          )
      },
  )

  return json.dumps(settings, indent=4) + '\n'

def initialize_cmd(args):
  settings = initial_settings(args.milestone, args.branch)

  with open(args.settings_json, 'w') as f:
    f.write(settings)


def enable_platform(
    settings_json: str,
    platform: str,
    description: str,
    sheriff_rotation: Optional[str],
) -> str:
  settings = json.loads(settings_json)
  settings['is_main'] = False
  platforms = settings.pop('platforms', {})
  platform_settings = {'description': description}
  if sheriff_rotation is not None:
    platform_settings['sheriff_rotation'] = sheriff_rotation
  platforms[platform] = platform_settings
  settings['platforms'] = dict(sorted(platforms.items()))
  return json.dumps(settings, indent=4) + '\n'


def enable_platform_cmd(args):
  with open(args.settings_json) as f:
    settings = f.read()

  settings = enable_platform(
      settings,
      args.platform,
      args.description,
      args.sheriff_rotation,
  )

  with open(args.settings_json, 'w') as f:
    f.write(settings)


def main():
  args = parse_args()
  args.func(args)

if __name__ == '__main__':
  main()

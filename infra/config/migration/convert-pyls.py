#! /usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections.abc
import pathlib
import subprocess
import typing

from lib import convert_pyls
from lib import pyl


def _parse_args(
    args: collections.abc.Sequence[str] | None = None) -> argparse.Namespace:
  parser = argparse.ArgumentParser()
  parser.add_argument('--pyls-dir', type=pathlib.Path)
  parser.add_argument('--infra-config-dir', type=pathlib.Path)
  return parser.parse_args(args)


def _get_literal(path: pathlib.Path) -> pyl.Value | None:
  try:
    with open(path, encoding='utf-8') as f:
      content = f.read()
  except FileNotFoundError:
    return None
  # If we're looking at a generated file, don't try and do any conversion
  if content.startswith('# THIS IS A GENERATED FILE DO NOT EDIT!!!'):
    return None
  nodes = pyl.parse(path, content)
  nodes = [n for n in nodes if isinstance(n, pyl.Value)]
  assert len(nodes) == 1
  return nodes[0]


def main():
  args = _parse_args()

  gn_isolate_map = _get_literal(args.pyls_dir / 'gn_isolate_map.pyl')
  if gn_isolate_map:
    new_files = convert_pyls.convert_gn_isolate_map_pyl(
        typing.cast(pyl.Dict[pyl.Str, pyl.Dict[pyl.Str, pyl.Value]],
                    gn_isolate_map))

  for file_name, content in new_files.items():
    file_path = args.infra_config_dir / file_name
    file_path.parent.mkdir(parents=True, exist_ok=True)
    with open(file_path, 'w') as f:
      f.write(content)

  subprocess.check_call(['lucicfg', 'fmt'], cwd=args.infra_config_dir)


if __name__ == '__main__':
  main()

#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Rebuilds baseline files by deleting the xml and re-building lint targets."""

import argparse
import logging
import os
import pathlib
import subprocess
from typing import List, Optional

_SRC_PATH = pathlib.Path(__file__).parents[3].resolve()
_OUTPUT_DIR_ROOT = _SRC_PATH / 'out'
_AUTONINJA_PATH = _SRC_PATH / 'third_party' / 'depot_tools' / 'autoninja'
_NINJA_PATH = _SRC_PATH / 'third_party' / 'depot_tools' / 'ninja'
_GN_PATH = _SRC_PATH / 'third_party' / 'depot_tools' / 'gn'
_CHROMECAST_EXTRA_ARGS = ['is_cast_android=true', 'enable_cast_receiver=true']


def gen_args_gn_content(use_goma: bool = False,
                        *,
                        extra_args: Optional[List[str]] = None) -> str:
    args = []
    if use_goma:
        args.append('use_goma=true')
    args.append('target_os="android"')
    # Lint prints out all errors when generating baseline files, so avoid
    # failing on output.
    args.append('treat_warnings_as_errors=false')
    # Both cronet and chromecast require this line to be built.
    args.append('is_component_build=false')
    if extra_args:
        args.extend(extra_args)
    return '\n'.join(args)


def build_all_lint_targets(out_dir: pathlib.Path,
                           args_gn_content: str,
                           prefix: Optional[str],
                           verbose: bool,
                           built_targets: Optional[List[str]] = None
                           ) -> List[str]:
    logging.info(f'Create output directory: {out_dir}')
    out_dir.mkdir(parents=True, exist_ok=True)

    args_gn_path = out_dir / 'args.gn'
    logging.info(f'Populating {args_gn_path}:\n{args_gn_content}')
    with open(args_gn_path, 'w') as f:
        f.write(args_gn_content)

    logging.info('Run `gn gen`.')
    subprocess.run([_GN_PATH, 'gen', out_dir], check=True)

    if built_targets is None:
        built_targets = []

    logging.info('Finding all lint targets.')
    target_names = []
    output = subprocess.run([_NINJA_PATH, '-C', out_dir, '-t', 'targets'],
                            check=True,
                            text=True,
                            capture_output=True).stdout
    for line in output.splitlines():
        ninja_target = line.rsplit(':', 1)[0]
        # Ensure only full-path targets are used (path:name) instead of the
        # shorthand (name).
        if ':' in ninja_target and ninja_target.endswith('__lint'):
            if ninja_target in built_targets:
                logging.info(
                    f'> Skipping {ninja_target} since it was already built.')
                continue
            if prefix and not ninja_target.startswith(prefix):
                logging.info(
                    f'> Skipping {ninja_target} due to --prefix={prefix}')
                continue
            logging.info(f'> Found {ninja_target}')
            target_names.append(ninja_target)

    if not target_names:
        logging.info('Did not find any targets to build.')
    else:
        logging.info(f'Re-building lint targets: {target_names}')
        subprocess.run([_AUTONINJA_PATH, '-C', out_dir] + target_names,
                       check=True,
                       capture_output=not verbose)

    return built_targets + target_names


def main():
    logging.basicConfig(
        level=logging.INFO,
        format='%(levelname).1s %(relativeCreated)7d %(message)s')

    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument(
        '--prefix',
        help='Only lint targets starting with this prefix will be re-built. '
        'For example, setting --prefix="clank" will only build lint targets '
        'under the //clank sub-directory.')
    parser.add_argument(
        '--git-root',
        default=_SRC_PATH,
        help='Defaults to //src, allows overriding which repo git runs in.')
    parser.add_argument('--use-goma',
                        action='store_true',
                        help='Turn on goma (useful for local runs).')
    parser.add_argument('-v',
                        '--verbose',
                        action='store_true',
                        help='Used to show ninja output.')
    parser.add_argument('-q',
                        '--quiet',
                        action='store_true',
                        help='Used to print only warnings and errors.')
    args = parser.parse_args()

    if args.quiet:
        level = logging.WARNING
    elif args.verbose:
        level = logging.DEBUG
    else:
        level = logging.INFO
    logging.basicConfig(
        level=level, format='%(levelname).1s %(relativeCreated)7d %(message)s')

    logging.info(f'Removing "lint-baseline.xml" files under {args.git_root}.')

    git_root = pathlib.Path(args.git_root)
    repo_file_paths = subprocess.run(
        ['git', '-C', str(git_root), 'ls-files'],
        check=True,
        capture_output=True,
        text=True).stdout.splitlines()

    for repo_path in repo_file_paths:
        path = git_root / repo_path
        if path.name == 'lint-baseline.xml':
            logging.info(f'> Deleting: {path}')
            path.unlink()

    out_dir = _OUTPUT_DIR_ROOT / 'Lint-Default'
    built_targets = build_all_lint_targets(out_dir,
                                           gen_args_gn_content(args.use_goma),
                                           args.prefix, args.verbose)

    out_dir = _OUTPUT_DIR_ROOT / 'Lint-Cast'
    built_targets = build_all_lint_targets(
        out_dir,
        gen_args_gn_content(args.use_goma, extra_args=_CHROMECAST_EXTRA_ARGS),
        args.prefix, args.verbose, built_targets)

    logging.info('Adding new lint-baseline.xml files to git.')
    for repo_path in repo_file_paths:
        path = git_root / repo_path
        if path.name == 'lint-baseline.xml':
            # Since we are passing -C to git, the relative path is needed
            logging.info(f'> Adding to git: {repo_path}')
            subprocess.run(['git', '-C', str(git_root), 'add', repo_path])


if __name__ == '__main__':
    main()

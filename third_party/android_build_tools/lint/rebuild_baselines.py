#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Rebuilds baseline files by deleting the xml and re-building lint targets."""

import argparse
import logging
import pathlib
import subprocess
import sys
from typing import List, Optional

_SRC_PATH = pathlib.Path(__file__).resolve().parents[3]
_CLANK_PATH = _SRC_PATH / 'clank'
_OUTPUT_DIR_ROOT = _SRC_PATH / 'out'
_NINJA_PATH = _SRC_PATH / 'third_party' / 'ninja' / 'ninja'
_GN_PATH = _SRC_PATH / 'buildtools' / 'linux64' / 'gn'

sys.path.insert(1, str(_SRC_PATH / 'build'))
import gn_helpers


def build_all_lint_targets(
        out_dir: pathlib.Path,
        args_list: List[str],
        *,
        verbose: bool,
        built_targets: Optional[List[str]] = None) -> List[str]:
    logging.info(f'Create output directory: {out_dir}')
    out_dir.mkdir(parents=True, exist_ok=True)

    args_gn_path = out_dir / 'args.gn'
    args_gn_content = '\n'.join(args_list)
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
            logging.info(f'> Found {ninja_target}')
            target_names.append(ninja_target)

    if not target_names:
        logging.info('Did not find any targets to build.')
    else:
        logging.info(f'Re-building lint targets: {target_names}')
        cmd = gn_helpers.CreateBuildCommand(str(out_dir)) + target_names
        # Do not show output by default since all lint warnings are printed.
        result = subprocess.run(cmd, check=False, capture_output=not verbose)
        if result.returncode:
            print('Build failed.')
            print(result.stdout)
            print(result.stderr)
            sys.exit(1)

    return built_targets + target_names


def _remove_redundant_lint_error(path: pathlib.Path):
    with open(path) as f:
        original_content = f.read()

    # Lint error is always the first issue.
    first_issue_idx = original_content.find('    <issue\n')
    if first_issue_idx == -1:
        return

    lint_error_idx = original_content.find('id="LintError"', first_issue_idx)
    if lint_error_idx == -1:
        return

    # If the lint error isn't the first issue, we should keep it.
    if lint_error_idx - first_issue_idx > 20:
        return

    issue_end_idx = original_content.find('</issue>\n', lint_error_idx)
    if issue_end_idx == -1:
        return

    # Replace 10 characters past the start of </issue> to remove extra newlines.
    replaced_content = original_content[:first_issue_idx] + original_content[
        issue_end_idx + 10:]

    with open(path, 'w') as f:
        f.write(replaced_content)


def main():
    logging.basicConfig(
        level=logging.INFO,
        format='%(levelname).1s %(relativeCreated)7d %(message)s')

    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
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

    include_clank = _CLANK_PATH.exists()

    git_roots = [_SRC_PATH]
    if include_clank:
        git_roots.append(_CLANK_PATH)

    logging.info(f'Removing "lint-baseline.xml" files under {git_roots}.')

    repo_file_paths = []
    for git_root in git_roots:
        git_file_paths = subprocess.run(
            ['git', '-C', str(git_root), 'ls-files'],
            check=True,
            capture_output=True,
            text=True).stdout.splitlines()
        repo_file_paths.extend((git_root, p) for p in git_file_paths)

    for git_root, repo_path in repo_file_paths:
        path = git_root / repo_path
        if path.name == 'lint-baseline.xml' and path.exists():
            logging.info(f'> Deleting: {path}')
            path.unlink()

    out_dir = _OUTPUT_DIR_ROOT / 'Lint-Default'
    gn_args = [
        'use_remoteexec=true',
        'target_os="android"',
        'treat_warnings_as_errors=false',
        'is_component_build=false',
        'enable_chrome_android_internal=false',
    ]
    built_targets = build_all_lint_targets(out_dir,
                                           gn_args,
                                           verbose=args.verbose)

    if include_clank:
        out_dir = _OUTPUT_DIR_ROOT / 'Lint-Clank'
        gn_args = [
            'use_remoteexec=true',
            'target_os="android"',
            'treat_warnings_as_errors=false',
            'is_component_build=false',
        ]
        built_targets = build_all_lint_targets(out_dir,
                                               gn_args,
                                               verbose=args.verbose,
                                               built_targets=built_targets)

    out_dir = _OUTPUT_DIR_ROOT / 'Lint-Cast'
    gn_args = [
        'use_remoteexec=true',
        'target_os="android"',
        'treat_warnings_as_errors=false',
        'is_component_build=false',
        'enable_chrome_android_internal=false',
        'is_cast_android=true',
        'enable_cast_receiver=true',
    ]
    built_targets = build_all_lint_targets(out_dir,
                                           gn_args,
                                           verbose=args.verbose,
                                           built_targets=built_targets)

    out_dir = _OUTPUT_DIR_ROOT / 'Lint-Cronet'
    gn_args = [
        'use_remoteexec=true',
        'target_os="android"',
        'treat_warnings_as_errors=false',
        'is_component_build=false',
        'is_cronet_build = true',
    ]
    built_targets = build_all_lint_targets(out_dir,
                                           gn_args,
                                           verbose=args.verbose,
                                           built_targets=built_targets)

    logging.info('Cleaning up new lint-baseline.xml files.')
    for git_root, repo_path in repo_file_paths:
        path = git_root / repo_path
        if path.name == 'lint-baseline.xml':
            logging.info(f'> Removing redundant LintError: {path}')
            _remove_redundant_lint_error(path)


if __name__ == '__main__':
    main()

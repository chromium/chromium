#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to generate build.gradle from template and run fetch_all.py

Extra args after -- are passed directly to fetch_all.py.
"""

import argparse
import logging
import os
import pathlib
import shutil
import subprocess
import sys

_SRC_PATH = pathlib.Path(__file__).resolve().parents[3]
_AUTOROLLED_PATH = _SRC_PATH / 'third_party/android_deps/autorolled'
_CIPD_PATH = _AUTOROLLED_PATH / 'cipd'
_BOM_NAME = 'bill_of_materials.json'
_CIPD_PACKAGE = 'chromium/third_party/android_deps/autorolled'
_EXTRACT_SCRIPT_PATH = _AUTOROLLED_PATH / 'extract_and_commit_extras.py'

sys.path.insert(1, str(_SRC_PATH / 'build/autoroll'))
import fetch_util


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-v',
                        '--verbose',
                        dest='verbose_count',
                        default=0,
                        action='count',
                        help='Verbose level (multiple times for more)')
    parser.add_argument('--local',
                        help='Run extract_and_commit_extras.py to update '
                        'committed files after rolling',
                        action='store_true')
    parser.add_argument(
        '--use-bom',
        action='store_true',
        help='If passed then we will use the existing bill_of_materials.json '
        'instead of resolving the latest versions.')
    args, extra_args = parser.parse_known_args()

    logging.basicConfig(
        level=logging.WARNING - 10 * args.verbose_count,
        format='%(levelname).1s %(relativeCreated)6d %(message)s')

    if 'SWARMING_TASK_ID' not in os.environ and not args.local:
        logging.warning(
            'Detected not running on a bot. You probably want to use --local')

    if args.use_bom:
        version_map_str = fetch_util.generate_version_map_str(
            _AUTOROLLED_PATH / _BOM_NAME)
    else:
        version_map_str = ''

    fetch_util.fill_template(_AUTOROLLED_PATH / 'build.gradle.template',
                             _AUTOROLLED_PATH / 'build.gradle',
                             version_overrides=version_map_str)

    os.makedirs(_CIPD_PATH, exist_ok=True)
    # cipd/gclient extract files as read only, allow writing before running
    # fetch_all.py
    subprocess.run(['chmod', '-R', '+w', _CIPD_PATH])

    fetch_util.run_fetch_all(android_deps_dir=_AUTOROLLED_PATH,
                             output_subdir='cipd',
                             extra_args=extra_args,
                             verbose_count=args.verbose_count)

    version_map_str, bom_hash = fetch_util.generate_version_map_str(
        _CIPD_PATH / _BOM_NAME, with_hash=True)

    # Regenerate the build.gradle file filling in the the version map so that
    # runs of the main project do not have to revalutate androidx versions.
    fetch_util.fill_template(_AUTOROLLED_PATH / 'build.gradle.template',
                             _CIPD_PATH / 'build.gradle',
                             version_overrides=version_map_str)

    generated_files = [
        'BUILD.gn',
        'bill_of_materials.json',
        'additional_readme_paths.json',
        'build.gradle',
    ]
    # TODO(mheikal): probably need to hash all text files, including licenses
    # and readmes.
    content_hash = fetch_util.hash_files(
        [_CIPD_PATH / filename for filename in generated_files])

    version_string = f'{bom_hash}.{content_hash}'
    version_txt_path = os.path.join(_CIPD_PATH, 'VERSION.txt')
    with open(version_txt_path, 'w') as f:
        f.write(version_string)
    generated_files.append('VERSION.txt')

    to_commit_zip_path = _CIPD_PATH / 'to_commit.zip'
    file_map = {
        f: f'third_party/android_deps/autorolled/{f}'
        for f in generated_files
    }
    fetch_util.create_to_commit_zip(output_path=to_commit_zip_path,
                                    package_root=_CIPD_PATH,
                                    dirnames=['libs'],
                                    absolute_file_map=file_map)

    if args.local:
        subprocess.run([
            _EXTRACT_SCRIPT_PATH, '--cipd-package-path', _CIPD_PATH,
            '--no-git-add'
        ],
                       check=True)

    fetch_util.write_cipd_yaml(package_root=_CIPD_PATH,
                               package_name=_CIPD_PACKAGE,
                               version=version_string,
                               output_path=_CIPD_PATH / 'cipd.yaml')


if __name__ == '__main__':
    main()

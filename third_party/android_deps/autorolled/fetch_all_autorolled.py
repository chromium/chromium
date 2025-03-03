#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to generate build.gradle from template and run fetch_all.py

Extra args after -- are passed directly to fetch_all.py.
"""

import argparse
import contextlib
import json
import logging
import os
import pathlib
import re
import hashlib
import shutil
import sys
import subprocess
import tempfile
from urllib import request
import zipfile

_AUTOROLLED_PATH = pathlib.Path(__file__).resolve().parents[0]
_CIPD_PATH = _AUTOROLLED_PATH / 'cipd'
_SRC_PATH = _AUTOROLLED_PATH.parents[2]
_BOM_PATH = _CIPD_PATH / 'bill_of_materials.json'
_HASH_LENGTH = 15

sys.path.insert(1, str(_SRC_PATH / 'third_party/depot_tools'))
import gclient_eval

_FETCH_ALL_PATH = _SRC_PATH / 'third_party/android_deps/fetch_all.py'

# Path to package listed in //DEPS
_DEPS_PACKAGE = 'src/third_party/android_deps/autorolled/cipd'
# CIPD package name
_CIPD_PACKAGE = 'chromium/third_party/android_deps/autorolled'


@contextlib.contextmanager
def _build_dir():
    dirname = tempfile.mkdtemp()
    try:
        yield dirname
    finally:
        shutil.rmtree(dirname)


def _generate_version_map_str(bom_path):
    bom = []
    version_map_lines = []
    bom_hash = hashlib.sha256()
    with open(bom_path) as f:
        bom = json.load(f)
    bom.sort(key=lambda x: (x['group'], x['name']))
    for dep in bom:
        group = dep['group']
        name = dep['name']
        version = dep['version']
        bom_hash.update(f'${group}:${name}:${version}'.encode())
        map_line = f"versionCache['{group}:{name}'] = '{version}'"
        version_map_lines.append(map_line)
    version_map_str = '\n'.join(sorted(version_map_lines))
    version_hash = bom_hash.hexdigest()[:_HASH_LENGTH]
    return version_map_str, version_hash


def _process_build_gradle(template_path, output_path, version_overrides_str):
    """Generates build.gradle from template.

    Args:
      template_path: Path to build.gradle.template.
      output_path: Path to build.gradle.
      androidx_repository_url: URL of the maven repository.
      version_override_str: An optional list of pinned versions.
    """
    content = pathlib.Path(template_path).read_text()
    content = content.replace('{{version_overrides}}', version_overrides_str)
    # build.gradle is not deleted after script has finished running. The file is in
    # .gitignore and thus will be excluded from uploaded CLs.
    pathlib.Path(output_path).write_text(content)


def _write_cipd_yaml(libs_dir, version, cipd_yaml_path, experimental=False):
    """Writes cipd.yaml file at the passed-in path."""

    lib_dirs = os.listdir(libs_dir)
    if not lib_dirs:
        raise Exception('No generated libraries in {}'.format(libs_dir))

    data_files = [
        'BUILD.gn',
        'VERSION.txt',
        'bill_of_materials.json',
        'additional_readme_paths.json',
        'build.gradle',
    ]
    for lib_dir in lib_dirs:
        abs_lib_dir = os.path.join(libs_dir, lib_dir)
        androidx_rel_lib_dir = os.path.relpath(abs_lib_dir, _CIPD_PATH)
        if not os.path.isdir(abs_lib_dir):
            continue
        lib_files = os.listdir(abs_lib_dir)
        if not 'cipd.yaml' in lib_files:
            continue

        for lib_file in lib_files:
            if lib_file == 'cipd.yaml' or lib_file == 'OWNERS':
                continue
            data_files.append(os.path.join(androidx_rel_lib_dir, lib_file))

    if experimental:
        package = ('experimental/google.com/' + os.getlogin() +
                   '/android_deps/autorolled')
    else:
        package = 'chromium/third_party/android_deps/autorolled'
    contents = [
        '# Copyright 2025 The Chromium Authors',
        '# Use of this source code is governed by a BSD-style license that can be',
        '# found in the LICENSE file.',
        '# version: ' + version,
        'package: ' + package,
        'description: Autorolled Android Deps',
        'data:',
    ]
    contents.extend('- file: ' + f for f in data_files)

    with open(cipd_yaml_path, 'w') as out:
        out.write('\n'.join(contents))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-v',
                        '--verbose',
                        dest='verbose_count',
                        default=0,
                        action='count',
                        help='Verbose level (multiple times for more)')
    parser.add_argument(
        '--use-bom',
        action='store_true',
        help='If passed then we will use the existing bill_of_materials.json '
        'instead of resolving the latest androidx.')
    args, extra_args = parser.parse_known_args()

    logging.basicConfig(
        level=logging.WARNING - 10 * args.verbose_count,
        format='%(levelname).1s %(relativeCreated)6d %(message)s')

    if args.use_bom:
        version_map_str, _ = _generate_version_map_str(_BOM_PATH)
    else:
        version_map_str = ''

    if os.path.exists(_CIPD_PATH):
        shutil.rmtree(_CIPD_PATH)
    os.mkdir(_CIPD_PATH)

    _process_build_gradle(_AUTOROLLED_PATH / 'build.gradle.template',
                          _CIPD_PATH / 'build.gradle', version_map_str)
    shutil.copyfile(_AUTOROLLED_PATH / 'BUILD.gn.template',
                    _CIPD_PATH / 'BUILD.gn')

    fetch_all_cmd = [
        _FETCH_ALL_PATH, '--android-deps-dir', _CIPD_PATH,
        '--ignore-vulnerabilities'
    ] + ['-v'] * args.verbose_count

    # Filter out -- from the args to pass to fetch_all.py.
    fetch_all_cmd += [a for a in extra_args if a != '--']

    env = os.environ.copy()
    # Silence the --local warning in fetch_all.py that is not applicable here.
    env['SWARMING_TASK_ID'] = '1'
    subprocess.run(fetch_all_cmd, check=True, env=env)

    version_map_str, version_hash = _generate_version_map_str(_BOM_PATH)

    # Regenerate the build.gradle file filling in the the version map so that
    # runs of the main project do not have to revalutate androidx versions.
    _process_build_gradle(_AUTOROLLED_PATH / 'build.gradle.template',
                          _CIPD_PATH / 'build.gradle', version_map_str)

    version_txt_path = os.path.join(_CIPD_PATH, 'VERSION.txt')
    with open(version_txt_path, 'w') as f:
        f.write(version_hash)

    libs_dir = os.path.join(_CIPD_PATH, 'libs')

    to_commit_paths = []
    for root, _, files in os.walk(libs_dir):
        for file in files:
            # Avoid committing actual artifacts.
            if file.endswith(('.aar', '.jar')):
                continue
            file_path = os.path.join(root, file)
            file_path_in_committed = os.path.relpath(file_path, _CIPD_PATH)
            to_commit_paths.append((file_path, file_path_in_committed))

    files_in_tree = [
        'additional_readme_paths.json',
        'bill_of_materials.json',
        'BUILD.gn',
        'build.gradle',
    ]
    for file in files_in_tree:
        file_path = os.path.join(_CIPD_PATH, file)
        to_commit_paths.append(
            (file_path,
             f'CHROMIUM_SRC/third_party/android_deps/autorolled/{file}'))

    to_commit_zip_path = os.path.join(_CIPD_PATH, 'to_commit.zip')
    with zipfile.ZipFile(to_commit_zip_path, 'w') as zip_file:
        for filename, arcname in to_commit_paths:
            zip_file.write(filename, arcname=arcname)

    yaml_path = os.path.join(_CIPD_PATH, 'cipd.yaml')
    _write_cipd_yaml(libs_dir, version_hash, yaml_path)


if __name__ == '__main__':
    main()

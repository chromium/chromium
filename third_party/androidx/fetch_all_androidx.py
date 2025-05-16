#!/usr/bin/env python3

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to generate build.gradle from template and run fetch_all.py

More specifically, to generate build.gradle:
  - It downloads the index page for the latest androidx snapshot from
    https://androidx.dev/snapshots/latest/artifacts
  - It replaces {{androidx_repository_url}} with the URL for the latest snapshot
  - For each dependency, it looks up the version in the index page's HTML and
    substitutes the version into {{androidx_dependency_version}}.

Extra args after -- are passed directly to fetch_all.py.
"""

import argparse
import contextlib
import json
import logging
import os
import pathlib
import re
import shutil
import sys
import subprocess
import tempfile
from urllib import request
import zipfile

_SRC_PATH = pathlib.Path(__file__).resolve().parents[2]
_ANDROIDX_PATH = _SRC_PATH / 'third_party/androidx'
_CIPD_PATH = _ANDROIDX_PATH / 'cipd'
_BOM_NAME = 'bill_of_materials.json'
_EXTRACT_SCRIPT_PATH = _ANDROIDX_PATH / 'extract_and_commit_extras.py'

sys.path.insert(1, str(_SRC_PATH / 'build/autoroll'))
import fetch_util

# URL to artifacts in latest androidx snapshot.
_ANDROIDX_LATEST_SNAPSHOT_ARTIFACTS_URL = 'https://androidx.dev/snapshots/latest/artifacts'

# When androidx roller is breaking, and a fix is not imminent, use this to pin a
# broken library to an old known-working version.
# * Find working versions from prior androidx roll commit descriptions.
# * The first element of each tuple is the path to the artifact of the latest
#   version of the library. It could change if the version is rev'ed in a new
#   snapshot.
# * The second element is a URL to replace the file with. Find the URL for older
#   versions of libraries by looking through the artifacts for the older version
#   (e.g.: https://androidx.dev/snapshots/builds/8545498/artifacts)
_OVERRIDES = [
    # Example:
    #('androidx_core_core/core-1.9.0-SNAPSHOT.aar',
    # 'https://androidx.dev/snapshots/builds/8545498/artifacts/repository/'
    # 'androidx/core/core/1.8.0-SNAPSHOT/core-1.8.0-20220505.122105-1.aar'),
]

# Set this to the build_id to pin all libraries to a given version.
# Useful when pinning a single library would cause issues, but you do not want
# to pause the auto-roller because other teams want to add / remove libraries.
# Example: '8545498'
_LATEST_VERSION_OVERRIDE = ''


_FILES_TO_COMMIT = [
    'additional_readme_paths.json',
    'bill_of_materials.json',
    'BUILD.gn',
    'build.gradle',
]


def _get_latest_androidx_version():
    androidx_artifacts_response = request.urlopen(
        _ANDROIDX_LATEST_SNAPSHOT_ARTIFACTS_URL)
    # Get the versioned url from the redirect destination.
    androidx_artifacts_url = androidx_artifacts_response.url
    androidx_artifacts_response.close()
    logging.info('URL for the latest build info: %s', androidx_artifacts_url)
    # Strip '/repository' from pattern.
    resolved_snapshot_repository_url_pattern = (
        fetch_util.make_androidx_maven_url('([0-9]*)').rsplit('/', 1)[0])
    match = re.match(resolved_snapshot_repository_url_pattern,
                     androidx_artifacts_url)
    assert match is not None
    version = match.group(1)
    logging.info('Resolved latest androidx version to %s', version)
    return version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-v',
                        '--verbose',
                        dest='verbose_count',
                        default=0,
                        action='count',
                        help='Verbose level (multiple times for more)')
    parser.add_argument('--local-repo',
                        help='Path to a locally androidx maven repo to use '
                        'instead of fetching the latest.')
    parser.add_argument(
        '--local',
        action='store_true',
        help='If passed then we will run the extract_and_commit_extras.py '
        'script and will not try rolling to the latest snapshot but reprocess '
        'the project at the current androidx.dev snapshot.')
    parser.add_argument(
        '--use-bom',
        action='store_true',
        help='If passed then we will use the existing bill_of_materials.json '
        'instead of resolving the latest androidx (faster but might resolve '
        'incorrect versions if deps are added/removed).')
    args, extra_args = parser.parse_known_args()

    logging.basicConfig(
        level=logging.WARNING - 10 * args.verbose_count,
        format='%(levelname).1s %(relativeCreated)6d %(message)s')

    if args.local_repo:
        version = 'local'
        androidx_snapshot_repository_url = ('file://' +
                                            os.path.abspath(args.local_repo))
    else:
        if _LATEST_VERSION_OVERRIDE:
            version = _LATEST_VERSION_OVERRIDE
        elif args.local:
            version = fetch_util.get_current_androidx_version()
        else:
            version = _get_latest_androidx_version()

        androidx_snapshot_repository_url = (
            fetch_util.make_androidx_maven_url(version))
        # Prepend '0' to version to avoid conflicts with previous version format.
        version = 'cr-0' + version

    if args.use_bom:
        version_map_str = fetch_util.generate_version_map_str(_ANDROIDX_PATH /
                                                              _BOM_NAME)
    else:
        version_map_str = ''

    fetch_util.fill_template(
        _ANDROIDX_PATH / 'build.gradle.template',
        _ANDROIDX_PATH / 'build.gradle',
        version_overrides=version_map_str,
        androidx_repository_url=androidx_snapshot_repository_url)

    # Overrides do not work with local snapshots since the repository_url is
    # different.
    if not args.local_repo:
        for subpath, url in _OVERRIDES:
            extra_args += ['--override-artifact', f'{subpath}:{url}']

    os.makedirs(_CIPD_PATH, exist_ok=True)
    # gclient/cipd extract files as read only.
    subprocess.run(['chmod', '-R', '+w', _CIPD_PATH])

    fetch_util.run_fetch_all(android_deps_dir=_ANDROIDX_PATH,
                             output_subdir='cipd',
                             extra_args=extra_args,
                             verbose_count=args.verbose_count)

    version_map_str = fetch_util.generate_version_map_str(_CIPD_PATH /
                                                          _BOM_NAME)

    # Regenerate the build.gradle file filling in the the version map so that
    # runs of the main project do not have to revalutate androidx versions.
    fetch_util.fill_template(
        _ANDROIDX_PATH / 'build.gradle.template',
        _CIPD_PATH / 'build.gradle',
        version_overrides=version_map_str,
        androidx_repository_url=androidx_snapshot_repository_url)

    version_txt_path = os.path.join(_CIPD_PATH, 'VERSION.txt')
    with open(version_txt_path, 'w') as f:
        f.write(version)

    to_commit_zip_path = _CIPD_PATH / 'to_commit.zip'
    file_map = {f: f'third_party/androidx/{f}' for f in _FILES_TO_COMMIT}
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
                               package_name=fetch_util.ANDROIDX_CIPD_PACKAGE,
                               version=version,
                               output_path=_CIPD_PATH / 'cipd.yaml')

if __name__ == '__main__':
    main()

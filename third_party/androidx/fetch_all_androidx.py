#!/usr/bin/env python3

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to generate build.gradle from template and run fetch_all.py

More specifically, to generate build.gradle:
  - It downloads the BUILD_INFO file for the latest androidx snapshot from
    https://androidx.dev/snapshots/
  - It replaces {{androidx_repository_url}} with the URL for the latest snapshot
  - For each dependency, it looks up the version in the BUILD_INFO file and
    substitutes the version into {{androidx_dependency_version}}.
"""

import contextlib
import json
import os
import re
import requests
import shutil
import subprocess
import tempfile

_ANDROIDX_PATH = os.path.normpath(os.path.join(__file__, '..'))

_FETCH_ALL_PATH = os.path.normpath(
    os.path.join(_ANDROIDX_PATH, '..', 'android_deps', 'fetch_all.py'))

# URL to BUILD_INFO in latest androidx snapshot.
_ANDROIDX_LATEST_SNAPSHOT_BUILD_INFO_URL = 'https://androidx.dev/snapshots/latest/artifacts/BUILD_INFO'


def _parse_dir_list(dir_list):
    """Computes 'library_group:library_name'->library_version mapping.

    Args:
      dir_list: List of androidx library directories.
    """
    dependency_version_map = dict()
    for dir_entry in dir_list:
        stripped_dir = dir_entry.strip()
        if not stripped_dir.startswith('repository/androidx/'):
            continue
        dir_components = stripped_dir.split('/')
        # Expected format:
        # "repository/androidx/library_group/library_name/library_version/pom_or_jar"
        if len(dir_components) < 6:
            continue
        dependency_module = 'androidx.{}:{}'.format(dir_components[2],
                                                    dir_components[3])
        if dependency_module not in dependency_version_map:
            dependency_version_map[dependency_module] = dir_components[4]
    return dependency_version_map


def _compute_replacement(dependency_version_map, androidx_repository_url,
                         line):
    """Computes output line for build.gradle from build.gradle.template line.

    Replaces {{android_repository_url}} and {{androidx_dependency_version}}.

    Args:
      dependency_version_map: An "dependency_group:dependency_name"->dependency_version mapping.
      androidx_repository_url: URL of the maven repository.
      line: Input line from the build.gradle.template.
    """
    line = line.replace('{{androidx_repository_url}}', androidx_repository_url)

    match = re.search(r'"(\S+):{{androidx_dependency_version}}"', line)
    if not match:
        return line

    version = dependency_version_map.get(match.group(1))
    if not version:
        return line

    return line.replace('{{androidx_dependency_version}}', version)


@contextlib.contextmanager
def _build_dir():
    dirname = tempfile.mkdtemp()
    try:
        yield dirname
    finally:
        shutil.rmtree(dirname)


def _download_and_parse_build_info():
    """Downloads and parses BUILD_INFO file."""
    with _build_dir() as build_dir:
        androidx_build_info_response = requests.get(
            _ANDROIDX_LATEST_SNAPSHOT_BUILD_INFO_URL)
        androidx_build_info_path = os.path.join(build_dir, 'BUILD_INFO')
        with open(androidx_build_info_path, 'w') as f:
            f.write(androidx_build_info_response.text)

        # Compute repository URL from resolved BUILD_INFO url in case 'latest' redirect changes.
        androidx_snapshot_repository_url = androidx_build_info_response.url.rsplit(
            '/', 1)[0] + '/repository'

        with open(androidx_build_info_path, 'r') as f:
            build_info_dict = json.loads(f.read())
        dir_list = build_info_dict['target']['dir_list']

        dependency_version_map = _parse_dir_list(dir_list)
        return (dependency_version_map, androidx_snapshot_repository_url)


def _process_build_gradle(dependency_version_map, androidx_repository_url):
    """Generates build.gradle from template.

    Args:
      dependency_version_map: An "dependency_group:dependency_name"->dependency_version mapping.
      androidx_repository_url: URL of the maven repository.
    """
    build_gradle_template_path = os.path.join(_ANDROIDX_PATH,
                                              'build.gradle.template')
    build_gradle_out_path = os.path.join(_ANDROIDX_PATH, 'build.gradle')
    # |build_gradle_out_path| is not deleted after script has finished running. The file is in
    # .gitignore and thus will be excluded from uploaded CLs.
    with open(build_gradle_template_path, 'r') as template_f, \
        open(build_gradle_out_path, 'w') as out:
        for template_line in template_f:
            replacement = _compute_replacement(dependency_version_map,
                                               androidx_repository_url,
                                               template_line)
            out.write(replacement)


def main():
    dependency_version_map, androidx_snapshot_repository_url = (
        _download_and_parse_build_info())
    _process_build_gradle(dependency_version_map,
                          androidx_snapshot_repository_url)

    fetch_all_cmd = [
        _FETCH_ALL_PATH, '--android-deps-dir',
        os.path.join('third_party', 'androidx'), '--ignore-vulnerabilities'
    ]
    subprocess.run(fetch_all_cmd)


if __name__ == '__main__':
    main()

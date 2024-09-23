#!/usr/bin/env python3

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to generate build.gradle from template and run fetch_all.py

More specifically, to generate build.gradle:
  - It downloads the BUILD_INFO file for the latest androidx snapshot from
    https://androidx.dev/snapshots/builds
  - It replaces {{androidx_repository_url}} with the URL for the latest snapshot
  - For each dependency, it looks up the version in the BUILD_INFO file and
    substitutes the version into {{androidx_dependency_version}}.
"""

import argparse
import contextlib
import json
import logging
import os
import pathlib
import re
import shutil
import stat
import subprocess
import tempfile
import urllib
from urllib import request

_ANDROIDX_PATH = os.path.normpath(os.path.join(__file__, '..'))
_CIPD_PATH = os.path.join(_ANDROIDX_PATH, 'cipd')

_FETCH_ALL_PATH = os.path.normpath(
    os.path.join(_ANDROIDX_PATH, '..', 'android_deps', 'fetch_all.py'))

# URL to BUILD_INFO in latest androidx snapshot.
_ANDROIDX_LATEST_SNAPSHOT_BUILD_INFO_URL = 'https://androidx.dev/snapshots/latest/artifacts/BUILD_INFO'

# Snapshot repository URL with {{version}} placeholder.
_SNAPSHOT_REPOSITORY_URL = 'https://androidx.dev/snapshots/builds/{{version}}/artifacts/repository'

# When androidx roller is breaking, and a fix is not immenent, use this to pin a
# broken library to an old known-working version.
# * The first element of each tuple is the path to the artifact of the latest
#   version of the library. It could change if the version is rev'ed in a new
#   snapshot.
# * The second element is a URL to replace the file with. Find the URL for older
#   versions of libraries by looking in the BUILD_INFO for the older version
#   (e.g.: https://androidx.dev/snapshots/builds/8545498/artifacts/BUILD_INFO)
_OVERRIDES = [
    # Example:
    #('androidx_core_core/core-1.9.0-SNAPSHOT.aar',
    # 'https://androidx.dev/snapshots/builds/8545498/artifacts/repository/'
    # 'androidx/core/core/1.8.0-SNAPSHOT/core-1.8.0-20220505.122105-1.aar'),
]


def _build_snapshot_repository_url(version):
    return _SNAPSHOT_REPOSITORY_URL.replace('{{version}}', version)


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
        dependency_package = 'androidx.' + '.'.join(dir_components[2:-3])
        dependency_module = '{}:{}'.format(dependency_package,
                                           dir_components[-3])
        if dependency_module not in dependency_version_map:
            dependency_version_map[dependency_module] = dir_components[-2]
    return dependency_version_map


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
        androidx_build_info_response = request.urlopen(
            _ANDROIDX_LATEST_SNAPSHOT_BUILD_INFO_URL)
        androidx_build_info_url = androidx_build_info_response.geturl()
        logging.info('URL for the latest build info: %s',
                     androidx_build_info_url)
        androidx_build_info_path = os.path.join(build_dir, 'BUILD_INFO')
        with open(androidx_build_info_path, 'w') as f:
            f.write(androidx_build_info_response.read().decode('utf-8'))

        # Strip '/repository' from pattern.
        resolved_snapshot_repository_url_pattern = (
            _build_snapshot_repository_url('([0-9]*)').rsplit('/', 1)[0])

        version = re.match(resolved_snapshot_repository_url_pattern,
                           androidx_build_info_url).group(1)

        with open(androidx_build_info_path, 'r') as f:
            build_info_dict = json.loads(f.read())
        dir_list = build_info_dict['target']['dir_list']

        return dir_list, version


def _create_local_dir_list(repo_path):
    repo_path = repo_path.rstrip('/')
    prefix_len = len(repo_path) + 1
    ret = []
    for dirpath, _, filenames in os.walk(repo_path):
        for name in filenames:
            ret.append(os.path.join('repository', dirpath[prefix_len:], name))
    return ret


def _process_build_gradle(template_path, output_path, dependency_version_map,
                          androidx_repository_url):
    """Generates build.gradle from template.

    Args:
      template_path: Path to build.gradle.template.
      output_path: Path to build.gradle.
      dependency_version_map: An "dependency_group:dependency_name"->dependency_version mapping.
      androidx_repository_url: URL of the maven repository.
    """
    version_re = re.compile(r'\s*\w+ompile\s+[\'"]([^:]+:[^:]+):(.+?)[\'"]')
    template_text = pathlib.Path(template_path).read_text()
    deps_with_custom_versions = set()
    sb = []
    for line in template_text.splitlines(keepends=True):
        line = line.replace('{{androidx_repository_url}}',
                            androidx_repository_url)
        if m := version_re.search(line):
            name, version = m.groups()
            if version == '{{androidx_dependency_version}}':
                new_version = dependency_version_map.get(name)
                if new_version is None:
                    raise Exception(f'Version for {name} not found.')
                line = line.replace(version, new_version)
            else:
                deps_with_custom_versions.add(name)
        elif line.strip() == '{{version_overrides}}':
            sb.append('versionOverrideMap = [:]\n')
            for name, version in sorted(dependency_version_map.items()):
                if name not in deps_with_custom_versions:
                    sb.append(f"versionOverrideMap['{name}'] = '{version}'\n")
            deps_with_custom_versions = None
            continue

        sb.append(line)

    # build.gradle is not deleted after script has finished running. The file is in
    # .gitignore and thus will be excluded from uploaded CLs.
    pathlib.Path(output_path).write_text(''.join(sb))


def _write_cipd_yaml(libs_dir, version, cipd_yaml_path, experimental=False):
    """Writes cipd.yaml file at the passed-in path."""

    lib_dirs = os.listdir(libs_dir)
    if not lib_dirs:
        raise Exception('No generated libraries in {}'.format(libs_dir))

    data_files = [
        'BUILD.gn', 'VERSION.txt', 'additional_readme_paths.json',
        'build.gradle'
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
        package = 'experimental/google.com/' + os.getlogin() + '/androidx'
    else:
        package = 'chromium/third_party/androidx'
    contents = [
        '# Copyright 2021 The Chromium Authors',
        '# Use of this source code is governed by a BSD-style license that can be',
        '# found in the LICENSE file.',
        '# version: ' + version,
        'package: ' + package,
        'description: androidx',
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
    parser.add_argument('--local-repo',
                        help='Path to a locally androidx maven repo to use '
                        'instead of fetching the latest.')
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.WARNING - 10 * args.verbose_count,
        format='%(levelname).1s %(relativeCreated)6d %(message)s')

    if os.path.exists(_CIPD_PATH):
        shutil.rmtree(_CIPD_PATH)
    os.mkdir(_CIPD_PATH)

    if args.local_repo:
        version = 'local'
        dir_list = _create_local_dir_list(args.local_repo)
        androidx_snapshot_repository_url = ('file://' +
                                            os.path.abspath(args.local_repo))
    else:
        dir_list, version = _download_and_parse_build_info()
        androidx_snapshot_repository_url = _build_snapshot_repository_url(
            version)
        # Prepend '0' to version to avoid conflicts with previous version format.
        version = 'cr-0' + version

    dependency_version_map = _parse_dir_list(dir_list)
    _process_build_gradle(
        os.path.join(_ANDROIDX_PATH, 'build.gradle.template'),
        os.path.join(_CIPD_PATH, 'build.gradle'), dependency_version_map,
        androidx_snapshot_repository_url)
    shutil.copyfile(os.path.join(_ANDROIDX_PATH, 'BUILD.gn.template'),
                    os.path.join(_CIPD_PATH, 'BUILD.gn'))

    fetch_all_cmd = [
        _FETCH_ALL_PATH, '--android-deps-dir', _CIPD_PATH,
        '--ignore-vulnerabilities'
    ] + ['-v'] * args.verbose_count
    # Overrides do not work with local snapshots since the repository_url is
    # different.
    if not args.local_repo:
        for subpath, url in _OVERRIDES:
            fetch_all_cmd += ['--override-artifact', f'{subpath}:{url}']
    subprocess.run(fetch_all_cmd, check=True)

    version_txt_path = os.path.join(_CIPD_PATH, 'VERSION.txt')
    with open(version_txt_path, 'w') as f:
        f.write(version)

    libs_dir = os.path.join(_CIPD_PATH, 'libs')
    yaml_path = os.path.join(_CIPD_PATH, 'cipd.yaml')
    _write_cipd_yaml(libs_dir,
                     version,
                     yaml_path,
                     experimental=bool(args.local_repo))


if __name__ == '__main__':
    main()

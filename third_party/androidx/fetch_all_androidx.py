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

_ANDROIDX_PATH = os.path.normpath(os.path.join(__file__, '..'))
_CIPD_PATH = os.path.join(_ANDROIDX_PATH, 'cipd')
_SRC_PATH = os.path.normpath(os.path.join(_ANDROIDX_PATH, '..', '..'))
_BOM_PATH = os.path.join(_CIPD_PATH, 'bill_of_materials.json')

sys.path.insert(1, os.path.join(_SRC_PATH, 'third_party', 'depot_tools'))
import gclient_eval

_FETCH_ALL_PATH = os.path.normpath(
    os.path.join(_ANDROIDX_PATH, '..', 'android_deps', 'fetch_all.py'))

# URL to artifacts in latest androidx snapshot.
_ANDROIDX_LATEST_SNAPSHOT_ARTIFACTS_URL = 'https://androidx.dev/snapshots/latest/artifacts'

# Path to package listed in //DEPS
_DEPS_PACKAGE = 'src/third_party/androidx/cipd'
# CIPD package name
_CIPD_PACKAGE = 'chromium/third_party/androidx'

# Snapshot repository URL with {{version}} placeholder.
_SNAPSHOT_REPOSITORY_URL = 'https://androidx.dev/snapshots/builds/{{version}}/artifacts/repository'

# When androidx roller is breaking, and a fix is not imminent, use this to pin a
# broken library to an old known-working version.
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

_FILES_TO_COMMIT = [
    'additional_readme_paths.json',
    'bill_of_materials.json',
    'BUILD.gn',
    'build.gradle',
]
_CIPD_DATA_FILES = _FILES_TO_COMMIT + [
    'VERSION.txt',
    'to_commit.zip',
]

_GENERATED_DISCLAIMER = '''\
// **IMPORTANT**: build.gradle is generated and any changes would be overridden
//                by the autoroller. Please update build.gradle.template
//                instead.
'''


def _build_snapshot_repository_url(version):
    return _SNAPSHOT_REPOSITORY_URL.replace('{{version}}', version)


def _get_latest_androidx_version():
    androidx_artifacts_response = request.urlopen(
        _ANDROIDX_LATEST_SNAPSHOT_ARTIFACTS_URL)
    # Get the versioned url from the redirect destination.
    androidx_artifacts_url = androidx_artifacts_response.url
    androidx_artifacts_response.close()
    logging.info('URL for the latest build info: %s', androidx_artifacts_url)
    # Strip '/repository' from pattern.
    resolved_snapshot_repository_url_pattern = (
        _build_snapshot_repository_url('([0-9]*)').rsplit('/', 1)[0])
    match = re.match(resolved_snapshot_repository_url_pattern,
                     androidx_artifacts_url)
    assert match is not None
    version = match.group(1)
    return version


def _query_cipd_tags(version):
    cipd_output = subprocess.check_output(
        ['cipd', 'describe', _CIPD_PACKAGE, '-version', version],
        encoding='utf-8')
    # Output looks like:
    # Package:       chromium/third_party/androidx
    # Instance ID:   gUjEawxv5mQO8yfbuC8W-rx4V3zYE-4LTWggXpZHI4sC
    # Registered by: user:chromium-cipd-builder@chops-service-accounts.iam.gserviceaccount.com
    # Registered at: 2025-01-06 17:54:48.034135 +0000 UTC
    # Refs:
    #   latest
    # Tags:
    #   details0:version-cr-012873390
    #   version:cr-012873390
    lines = cipd_output.split('\n')
    tags = {}
    parsing_tags = False
    for line in lines:
        if not line.strip():
            continue
        if line.startswith('Tags:'):
            parsing_tags = True
            continue
        if parsing_tags:
            tag, value = line.strip().split(':', 1)
            tags[tag] = value
    return tags


def _get_current_cipd_instance():
    with open(os.path.join(_SRC_PATH, 'DEPS'), 'rt') as f:
        gclient_dict = gclient_eval.Exec(f.read())
        return gclient_eval.GetCIPD(gclient_dict, _DEPS_PACKAGE, _CIPD_PACKAGE)


def _get_current_androidx_version():
    cipd_instance = _get_current_cipd_instance()
    cipd_tags = _query_cipd_tags(cipd_instance)
    version_string = cipd_tags['version']
    version = version_string[len('cr-0'):]
    return version


def _generate_version_map_str(bom_path):
    bom = []
    version_lines = []
    with open(bom_path) as f:
        bom = json.load(f)
    for dep in bom:
        line = f"versionCache['{dep['group']}:{dep['name']}'] = '{dep['version']}'"
        version_lines.append(line)
    return '\n'.join(sorted(version_lines))


def _process_build_gradle(template_path, output_path, androidx_repository_url,
                          version_overrides_str):
    """Generates build.gradle from template.

    Args:
      template_path: Path to build.gradle.template.
      output_path: Path to build.gradle.
      androidx_repository_url: URL of the maven repository.
      version_override_str: An optional list of pinned versions.
    """
    content = pathlib.Path(template_path).read_text()
    content = content.replace('{{androidx_repository_url}}',
                              androidx_repository_url)
    content = content.replace('{{version_overrides}}', version_overrides_str)
    content = content.replace('{{generated_disclaimer}}',
                              _GENERATED_DISCLAIMER)
    # build.gradle is not deleted after script has finished running. The file is in
    # .gitignore and thus will be excluded from uploaded CLs.
    pathlib.Path(output_path).write_text(content)


def _write_cipd_yaml(libs_dir,
                     version,
                     cipd_yaml_path,
                     experimental=False):
    """Writes cipd.yaml file at the passed-in path."""

    lib_dirs = os.listdir(libs_dir)
    if not lib_dirs:
        raise Exception('No generated libraries in {}'.format(libs_dir))

    cipd_lib_files = []
    for lib_dir in lib_dirs:
        abs_lib_dir = os.path.join(libs_dir, lib_dir)
        androidx_rel_lib_dir = os.path.relpath(abs_lib_dir, _CIPD_PATH)
        if not os.path.isdir(abs_lib_dir):
            continue
        lib_files = os.listdir(abs_lib_dir)
        if not 'cipd.yaml' in lib_files:
            continue

        for lib_file in lib_files:
            if lib_file == 'cipd.yaml':
                continue
            cipd_lib_files.append(os.path.join(androidx_rel_lib_dir, lib_file))

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
    contents.extend('- file: ' + f for f in cipd_lib_files + _CIPD_DATA_FILES)

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
    parser.add_argument(
        '--no-roll',
        action='store_true',
        help='If passed then we will not try rolling the '
        'latest androidx but use the currently rolled version.')
    parser.add_argument(
        '--use-bom',
        action='store_true',
        help='If passed then we will use the existing bill_of_materials.json '
        'instead of resolving the latest androidx.')
    args, extra_args = parser.parse_known_args()

    logging.basicConfig(
        level=logging.WARNING - 10 * args.verbose_count,
        format='%(levelname).1s %(relativeCreated)6d %(message)s')

    if args.local_repo:
        version = 'local'
        androidx_snapshot_repository_url = ('file://' +
                                            os.path.abspath(args.local_repo))
    else:
        if args.no_roll:
            version = _get_current_androidx_version()
            logging.info('Resolved current androidx version to %s', version)
        else:
            version = _get_latest_androidx_version()
            logging.info('Resolved latest androidx version to %s', version)

        androidx_snapshot_repository_url = _build_snapshot_repository_url(
            version)
        # Prepend '0' to version to avoid conflicts with previous version format.
        version = 'cr-0' + version

    if args.use_bom:
        version_map_str = _generate_version_map_str(_BOM_PATH)
    else:
        version_map_str = ''

    if os.path.exists(_CIPD_PATH):
        shutil.rmtree(_CIPD_PATH)
    os.mkdir(_CIPD_PATH)

    _process_build_gradle(
        os.path.join(_ANDROIDX_PATH, 'build.gradle.template'),
        os.path.join(_CIPD_PATH, 'build.gradle'),
        androidx_snapshot_repository_url, version_map_str)
    shutil.copyfile(os.path.join(_ANDROIDX_PATH, 'BUILD.gn'),
                    os.path.join(_CIPD_PATH, 'BUILD.gn'))

    fetch_all_cmd = [
        _FETCH_ALL_PATH, '--android-deps-dir', _CIPD_PATH,
        '--ignore-vulnerabilities'
    ] + ['-v'] * args.verbose_count

    # Filter out -- from the args to pass to fetch_all.py.
    fetch_all_cmd += [a for a in extra_args if a != '--']

    # Overrides do not work with local snapshots since the repository_url is
    # different.
    if not args.local_repo:
        for subpath, url in _OVERRIDES:
            fetch_all_cmd += ['--override-artifact', f'{subpath}:{url}']
    env = os.environ.copy()
    # Silence the --local warning in fetch_all.py that is not applicable here.
    env['SWARMING_TASK_ID'] = '1'
    subprocess.run(fetch_all_cmd, check=True, env=env)

    version_map_str = _generate_version_map_str(_BOM_PATH)

    # Regenerate the build.gradle file filling in the the version map so that
    # runs of the main project do not have to revalutate androidx versions.
    _process_build_gradle(
        os.path.join(_ANDROIDX_PATH, 'build.gradle.template'),
        os.path.join(_CIPD_PATH, 'build.gradle'),
        androidx_snapshot_repository_url, version_map_str)

    version_txt_path = os.path.join(_CIPD_PATH, 'VERSION.txt')
    with open(version_txt_path, 'w') as f:
        f.write(version)

    libs_dir = os.path.join(_CIPD_PATH, 'libs')

    to_commit_paths = []
    for root, _, files in os.walk(libs_dir):
        for file in files:
            # Avoid committing actual artifacts.
            if file.endswith(('.aar', '.jar', 'cipd.yaml')):
                continue
            file_path = os.path.join(root, file)
            file_path_in_committed = os.path.relpath(file_path, _CIPD_PATH)
            to_commit_paths.append((file_path, file_path_in_committed))

    for file in _FILES_TO_COMMIT:
        file_path = os.path.join(_CIPD_PATH, file)
        to_commit_paths.append(
            (file_path, f'CHROMIUM_SRC/third_party/androidx/{file}'))

    to_commit_zip_path = os.path.join(_CIPD_PATH, 'to_commit.zip')
    with zipfile.ZipFile(to_commit_zip_path, 'w') as zip_file:
        for filename, arcname in to_commit_paths:
            zip_file.write(filename, arcname=arcname)

    yaml_path = os.path.join(_CIPD_PATH, 'cipd.yaml')
    _write_cipd_yaml(libs_dir,
                     version,
                     yaml_path,
                     experimental=bool(args.local_repo))

if __name__ == '__main__':
    main()

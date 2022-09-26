#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to generate build.gradle from template and run fetch_all.py

More specifically, to generate build.gradle:
  - It downloads the gradle metadata for guava and parses the file to determine
    the latest guava version.
  - It replaces {{guava_version}} in build.gralde.template with the latest guava
    version.
"""

import os
import re
import shutil
import subprocess
import tempfile
import urllib
from urllib import request
from xml.etree import ElementTree

_AUTOROLLED_PATH = os.path.normpath(os.path.join(__file__, '..'))

_GUAVA_MAVEN_METADATA_XML_URL = 'https://repo.maven.apache.org/maven2/com/google/guava/guava/maven-metadata.xml'

_FETCH_ALL_PATH = os.path.normpath(
    os.path.join(_AUTOROLLED_PATH, '..', 'android_deps', 'fetch_all.py'))


def _download_and_compute_latest_guava_version():
    """Downloads gradle metadata for guava. Returns latest guava version."""
    metadata_xml_response = request.urlopen(_GUAVA_MAVEN_METADATA_XML_URL)
    xml_tree = ElementTree.fromstring(metadata_xml_response.read())
    latest_version = xml_tree.find('versioning/latest')

    match = re.match(r'([0-9]*\.[0-9]*).*', latest_version.text)
    version = match.group(1)

    jre_version = version + '-jre'
    android_version = version + '-android'
    version_jre_path = 'versioning/versions/version[.=\'' + jre_version + '\']'
    version_android_path = 'versioning/versions/version[.=\'' + android_version + '\']'
    if xml_tree.find(version_jre_path) == None or xml_tree.find(version_android_path) == None:
      raise Exception('{} or {} version not found in {}.'.format(jre_version, android_version, _GUAVA_MAVEN_METADATA_XML_URL))

    return version


def _process_build_gradle(guava_version):
    """Generates build.gradle from template.

    Args:
      guava_version: Latest guava version.
    """
    template_path = os.path.join(_AUTOROLLED_PATH, 'build.gradle.template')
    with open(template_path) as f:
      template_content = f.read()

    out_path = os.path.join(_AUTOROLLED_PATH, 'build.gradle')
    with open(out_path, 'w') as f:
      out = template_content.replace('{{guava_version}}', guava_version)
      f.write(out)


def _extract_files_from_yaml(yaml_path):
    """Extracts '- file' file listings from yaml file."""
    out = None
    with open(yaml_path, 'r') as f:
        for line in f.readlines():
            line = line.rstrip('\n')
            if line == 'data:':
                out = []
                continue
            if out is not None:
                if not line.startswith('- file:'):
                    raise Exception(
                        '{} has unsupported attributes. Only \'- file\' is supported'
                        .format(yaml_path))
                out.append(line.rsplit(' ', 1)[1])

    if not out:
        raise Exception('{} does not have \'data\' section.'.format(yaml_path))
    return out


def _write_cipd_yaml(libs_dir, cipd_yaml_path):
    """Writes cipd.yaml file at the passed-in path."""

    lib_dirs = os.listdir(libs_dir)
    if not lib_dirs:
        raise Exception('No generated libraries in {}'.format(libs_dir))

    data_files = ['BUILD.gn', 'additional_readme_paths.json']
    for lib_dir in lib_dirs:
        abs_lib_dir = os.path.join(libs_dir, lib_dir)
        autorolled_rel_lib_dir = os.path.relpath(abs_lib_dir, _AUTOROLLED_PATH)
        if not os.path.isdir(abs_lib_dir):
            continue
        lib_files = os.listdir(abs_lib_dir)
        if not 'cipd.yaml' in lib_files:
            continue

        if not 'README.chromium' in lib_files:
            raise Exception('README.chromium not in {}'.format(abs_lib_dir))
        if not 'LICENSE' in lib_files:
            raise Exception('LICENSE not in {}'.format(abs_lib_dir))
        data_files.append(os.path.join(autorolled_rel_lib_dir,
                                       'README.chromium'))
        data_files.append(os.path.join(autorolled_rel_lib_dir, 'LICENSE'))

        _rel_extracted_files = _extract_files_from_yaml(
            os.path.join(abs_lib_dir, 'cipd.yaml'))
        data_files.extend(
            os.path.join(autorolled_rel_lib_dir, f)
            for f in _rel_extracted_files)

    contents = [
        '# Copyright 2021 The Chromium Authors. All rights reserved.',
        '# Use of this source code is governed by a BSD-style license that can be',
        '# found in the LICENSE file.',
        'package: chromium/third_party/android_deps_autorolled', 'description: android_deps_autorolled',
        'data:'
    ]
    contents.extend('- file: ' + f for f in data_files)

    with open(cipd_yaml_path, 'w') as out:
        out.write('\n'.join(contents))


def main():
    libs_dir = os.path.join(_AUTOROLLED_PATH, 'libs')

    # Let recipe delete contents of lib directory because it has API to retry
    # directory deletion if the first deletion attempt does not work.
    if os.path.exists(libs_dir) and os.listdir(libs_dir):
        raise Exception('Recipe did not empty \'libs\' directory.')

    guava_version = _download_and_compute_latest_guava_version()
    _process_build_gradle(guava_version)

    fetch_all_cmd = [
        _FETCH_ALL_PATH, '--android-deps-dir', _AUTOROLLED_PATH,
        '--ignore-vulnerabilities'
    ]
    subprocess.run(fetch_all_cmd, check=True)

    _write_cipd_yaml(libs_dir, os.path.join(_AUTOROLLED_PATH, 'cipd.yaml'))


if __name__ == '__main__':
    main()

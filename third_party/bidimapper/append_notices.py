#!/usr/bin/env python3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import io
import json
import pathlib
import re
import sys
import urllib.request

from typing import Optional

_DEPENDENCY_DIVIDER = '-------------------- DEPENDENCY DIVIDER --------------------'
_HEADER_PATTERN = re.compile('^\\w+:.*$')


class NoticeParsingException(Exception):
    pass


class ThirdPartyNoticeParser:

    def __init__(self, third_party_notices_path: pathlib.Path,
                 readme_path: pathlib.Path, license_dir: pathlib.Path):
        self._third_party_notices_path = third_party_notices_path
        self._readme_path = readme_path
        self._license_dir = license_dir
        self._seen_names = set()
        self._headers = {}
        self._readme_file: Optional[io.TextIOWrapper] = None
        self._license_begin = 0
        self._license_end = 0
        pass

    def parse_and_append_notices(self):
        # Remove the license files for each depenency
        for license_path in self._license_dir.glob('LICENSE.*'):
            license_path.unlink()

        lines: list[str] = []
        with self._third_party_notices_path.open('r') as notices_file:
            lines = notices_file.readlines()
        if len(lines) == 0:
            return

        lines = [line.rstrip() for line in lines]

        state = self._before_headers_state
        with self._readme_path.open('a') as readme_file:
            self._readme_file = readme_file
            line_index = 0
            # Some value not less than the total state count
            state_count = 10
            # A guard intended to detect an infinite loop
            loop_variant = len(lines) * state_count
            while line_index < len(lines):
                if loop_variant <= 0:
                    raise NoticeParsingException(
                        'Internal error: no progress during the iteration')
                state, line_index = state(lines, line_index)
                loop_variant -= 1

            if state == self._read_license_state:
                state = self._print_license_state
                state(lines, line_index)

    def _before_headers_state(self, lines: list[str], line_index: int):
        line = lines[line_index]
        if line == '':
            return self._before_headers_state, line_index + 1

        self._headers = {}
        return self._read_headers_state, line_index

    def _read_headers_state(self, lines: list[str], line_index: int):
        line = lines[line_index]
        if line == '':
            self._license_begin = line_index + 1
            self._license_end = line_index + 1
            return self._read_license_state, line_index + 1

        if not _HEADER_PATTERN.match(line):
            raise NoticeParsingException('Expected a header field, got "%s"' %
                                         line.strip())
        header_key = line.split(':')[0]
        if header_key in self._headers:
            raise NoticeParsingException('Dumplicated header: "%s"' %
                                         line.strip())
        self._headers[header_key] = line_index
        return self._read_headers_state, line_index + 1

    def _read_license_state(self, lines: list[str], line_index: int):
        self._license_end = line_index
        if lines[line_index] == _DEPENDENCY_DIVIDER:
            return self._print_license_state, line_index + 1
        return self._read_license_state, line_index + 1

    def _print_license_state(self, lines: list[str], line_index: int):
        expected_headers = ['Name', 'URL', 'Version', 'License']
        for header in expected_headers:
            if header not in self._headers:
                raise NoticeParsingException(
                    'Expected header is missing: "%s"' % header)

        original_name_value = self._get_header_value('Name', lines)
        original_version_value = self._get_header_value('Version', lines)

        print('\n' + _DEPENDENCY_DIVIDER + '\n', file=self._readme_file)
        for header in expected_headers:
            print(lines[self._headers[header]], file=self._readme_file)
        print('Revision: '+self._get_revision(original_name_value, original_version_value), file=self._readme_file)
        print('Security Critical: no', file=self._readme_file)
        print('Shipped: yes', file=self._readme_file)

        path_name_value = re.sub(r'[^\w_]', '_', original_name_value)
        name_value = path_name_value
        index = 0
        while name_value in self._seen_names:
            index += 1
            name_value = f'{path_name_value}_{index}'
        self._seen_names.add(name_value)
        license_path = self._license_dir.joinpath(f'LICENSE.{name_value}')
        relative_license_path = license_path.relative_to(
            self._readme_path.parent)
        print(f'License File: {relative_license_path}', file=self._readme_file)

        while self._license_begin < self._license_end and lines[
            self._license_begin] == '':
            self._license_begin += 1
        while self._license_begin < self._license_end and lines[
            self._license_end - 1] == '':
            self._license_end -= 1
        if self._license_begin == self._license_end:
            raise NoticeParsingException(
                'License text is missing for dependency: "%s"' %
                original_name_value)

        with license_path.open('w') as license_file:
            for k in range(self._license_begin, self._license_end):
                print(lines[k], file=license_file)

        return self._before_headers_state, line_index

    def _get_header_value(self, name, lines):
        header_line = lines[self._headers[name]]
        return header_line[len(name+':'):].strip()

    @staticmethod
    def _get_revision(name, version):
        """
        Get the revision of the package from the npm registry. Required, as the
        specific dependency revisions are required for the build. As long as the
        information about the specific revision is not available on the local
        npm package, this function fetches the revision from the npm registry.
        Falls back to `N/A` if the revision cannot be fetched for any reason.
        """
        try:
            registry_url = f"https://registry.npmjs.org/{name}/{version}"
            with urllib.request.urlopen(registry_url) as registry_response:
                registry_data = json.load(registry_response)
                return registry_data["gitHead"] or 'N/A'
        except:
            return 'N/A'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--third-party-notices',
        type=pathlib.Path,
        help='Path to chromium-bidi third party notices (REQUIRED!)')
    parser.add_argument('--readme',
                        type=pathlib.Path,
                        help='Path to README.chromium')
    parser.add_argument('--license-dir',
                        type=pathlib.Path,
                        help='Directory for third party license files')
    options = parser.parse_args()
    if options.third_party_notices is None:
        parser.error('--third-party-notices is required.\n' +
                     'Please run "%s --help" for help' % __file__)
    if not options.third_party_notices.exists():
        parser.error('File not found: %s' % (options.third_party_notices))

    readme_path = options.readme
    if readme_path is None:
        readme_path = pathlib.Path('.', 'README.chromium')
    if not readme_path.exists():
        parser.error('File not found: %s' % (readme_path))

    license_dir = options.license_dir
    if license_dir is None:
        license_dir = pathlib.Path('.')
    if not license_dir.exists():
        try:
            license_dir.mkdir(parents=True)
        except Exception:
            parser.error('Unable to create directory: %s' % (license_dir))
    if not license_dir.is_dir():
        parser.error('The path is not a directory: %s' % (license_dir))

    try:
        notice_parser = ThirdPartyNoticeParser(options.third_party_notices,
                                               readme_path=readme_path,
                                               license_dir=license_dir)
        notice_parser.parse_and_append_notices()
    except Exception as ex:
        parser.error('Failed to append notices: %s' % (ex))

    return 0


if __name__ == '__main__':
    sys.exit(main())

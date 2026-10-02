#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Host string generation utilities for iOS builds on non-macOS hosts.

Provides pure-Python equivalents of the Objective-C++ host tools
`generate_localizable_strings` and `substitute_strings_identifier` using
Chromium's `grit.format.data_pack` and standard library `plistlib`.
"""

from __future__ import annotations

import getopt
import os
import plistlib
import struct
import sys
from typing import Any

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_SRC_ROOT = os.path.abspath(os.path.join(_THIS_DIR, '..', '..', '..', '..'))
_GRIT_DIR = os.path.join(_SRC_ROOT, 'tools', 'grit')
if _GRIT_DIR not in sys.path:
    sys.path.insert(0, _GRIT_DIR)

from grit.format import data_pack  # noqa: E402


def load_resources_from_headers(headers: list[str]) -> dict[str, int] | None:
    """Parses `#define <KEY> <INT>` resource ID mappings from GRIT headers."""
    resource_map: dict[str, int] = {}
    for header in headers:
        try:
            with open(header, 'r', encoding='utf-8') as f:
                content = f.read()
        except OSError as e:
            sys.stderr.write(f'ERROR: error loading header {header}: {e}\n')
            return None
        for line in content.split('\n'):
            if not line.startswith('#define '):
                continue
            items = [item.strip() for item in line.split(' ') if item.strip()]
            if len(items) != 3:
                sys.stderr.write(
                    f'ERROR: header {header} contains invalid entry: {line}\n'
                )
                return None
            key = items[1]
            if key in resource_map:
                sys.stderr.write(
                    f'ERROR: entry duplicated in parsed headers: {key}\n'
                )
                return None
            try:
                value = int(items[2])
            except ValueError:
                sys.stderr.write(
                    f'ERROR: header {header} contains invalid entry: {line}\n'
                )
                return None
            resource_map[key] = value
    return resource_map


def load_data_pack(pak_path: str) -> data_pack.DataPackContents | None:
    """Loads a GRIT `.pak` file."""
    if not os.path.exists(pak_path):
        return None
    try:
        return data_pack.ReadDataPack(pak_path)
    except (
        OSError,
        struct.error,
        data_pack.WrongFileVersion,
        data_pack.CorruptDataPack,
    ):
        return None


def get_string_from_data_pack(
    pack: data_pack.DataPackContents, resource_id: int
) -> str | None:
    """Decodes the string for `resource_id` from `pack`, or returns None."""
    raw = pack.resources.get(resource_id)
    if raw is None:
        return None
    try:
        if pack.encoding == data_pack.UTF8:
            return raw.decode('utf-8')
        if pack.encoding == data_pack.UTF16:
            return raw.decode('utf-16-le')
    except UnicodeDecodeError:
        return None
    return None


def generate_localizable_strings(argv: list[str]) -> int:
    """Generates localized `.strings` binary plists from GRIT `.pak` files."""
    output_dir = None
    data_pack_dir = None
    root_header_dir = None
    config_file = None
    try:
        opts, args = getopt.getopt(argv, 'c:I:p:o:h')
    except getopt.GetoptError as e:
        sys.stderr.write(f'ERROR: {e}\n')
        return 1

    for opt, arg in opts:
        if opt == '-c':
            config_file = arg
        elif opt == '-I':
            root_header_dir = arg
        elif opt == '-p':
            data_pack_dir = arg
        elif opt == '-o':
            output_dir = arg
        elif opt == '-h':
            return 0

    if not config_file:
        sys.stderr.write('ERROR: missing config file.\n')
        return 1
    if not root_header_dir:
        sys.stderr.write('ERROR: missing root header dir.\n')
        return 1
    if not output_dir:
        sys.stderr.write('ERROR: missing output directory.\n')
        return 1
    if not data_pack_dir:
        sys.stderr.write('ERROR: missing data pack directory.\n')
        return 1
    if not args:
        sys.stderr.write('ERROR: missing locale list.\n')
        return 1

    with open(config_file, 'rb') as f:
        config = plistlib.load(f)

    header_list = config.get('headers', [])
    if not header_list:
        sys.stderr.write('ERROR: No header file in the config.\n')
        return 1

    headers = [os.path.join(root_header_dir, h) for h in header_list]
    resources_ids = load_resources_from_headers(headers)
    if resources_ids is None:
        return 1

    locales = [
        'en' if arg == 'en-US' else arg.replace('-', '_') for arg in args
    ]
    outputs = config.get('outputs', [])
    if not outputs:
        sys.stderr.write('ERROR: No output in config file.\n')
        return 1

    for locale in locales:
        pak_path = os.path.join(data_pack_dir, f'{locale}.lproj', 'locale.pak')
        loaded_pack = load_data_pack(pak_path)
        if loaded_pack is None:
            sys.stderr.write(
                f'ERROR: Failed to load branded pak for language:{locale}\n'
            )
            return 1

        for output in outputs:
            output_name = output.get('name')
            if not output_name:
                sys.stderr.write('ERROR: Output without name.\n')
                return 1
            output_strings = output.get('strings', [])
            if not output_strings:
                sys.stderr.write(
                    f'ERROR: Output without strings: {output_name}\n'
                )
                return 1

            dictionary: dict[str, str] = {}
            for resource in output_strings:
                if isinstance(resource, str):
                    resource_name = resource
                    resource_output_name = resource
                elif isinstance(resource, dict):
                    resource_name = resource.get('input')
                    resource_output_name = resource.get('output')
                    if not resource_name or not resource_output_name:
                        sys.stderr.write(
                            'ERROR: resources must be given in <string> or '
                            '<dict> format.\n'
                        )
                        return 1
                else:
                    sys.stderr.write(
                        'ERROR: resources must be given in <string> or <dict> '
                        'format.\n'
                    )
                    return 1

                if resource_name not in resources_ids:
                    sys.stderr.write(
                        f"ERROR: fail to load string '{resource_name}' for "
                        f"locale '{locale}'\n"
                    )
                    return 1
                resource_id = resources_ids[resource_name] & 0xFFFF
                string_val = get_string_from_data_pack(loaded_pack, resource_id)
                if string_val is None:
                    sys.stderr.write(
                        f"ERROR: fail to load string '{resource_name}' for "
                        f"locale '{locale}'\n"
                    )
                    return 1
                dictionary[resource_output_name] = string_val

            locale_out_dir = os.path.join(output_dir, f'{locale}.lproj')
            os.makedirs(locale_out_dir, exist_ok=True)
            out_file = os.path.join(locale_out_dir, output_name)
            with open(out_file, 'wb') as f:
                plistlib.dump(dictionary, f, fmt=plistlib.FMT_BINARY)

    return 0


def convert_plist_value(value: Any, resource_map: dict[str, int]) -> Any:
    """Recursively converts `IDS_`/`IDR_` string identifiers to integer IDs."""
    if isinstance(value, str):
        if value in resource_map:
            return resource_map[value]
        if value.startswith(('IDS_', 'IDR_')):
            sys.stderr.write(f'ERROR: no value found for string: {value}\n')
            return None
        return value
    if isinstance(value, list):
        converted_list = []
        for item in value:
            obj = convert_plist_value(item, resource_map)
            if obj is None:
                return None
            converted_list.append(obj)
        return converted_list
    if isinstance(value, dict):
        converted_dict = {}
        for k, v in value.items():
            obj = convert_plist_value(v, resource_map)
            if obj is None:
                return None
            converted_dict[k] = obj
        return converted_dict
    return value


def substitute_strings_identifier(argv: list[str]) -> int:
    """Replaces string identifiers in a plist with numeric IDs from headers."""
    headers: list[str] = []
    source_path = ''
    output_path = ''
    try:
        opts, _ = getopt.getopt(argv, 'I:i:o:h')
    except getopt.GetoptError as e:
        sys.stderr.write(f'ERROR: {e}\n')
        return 1

    for opt, arg in opts:
        if opt == '-I':
            headers.append(arg)
        elif opt == '-i':
            if source_path:
                sys.stderr.write('ERROR: cannot pass -i multiple times\n')
                return 1
            source_path = arg
        elif opt == '-o':
            if output_path:
                sys.stderr.write('ERROR: cannot pass -o multiple times\n')
                return 1
            output_path = arg
        elif opt == '-h':
            return 0

    if not headers:
        sys.stderr.write('ERROR: header_path is required.\n')
        return 1
    if not source_path:
        sys.stderr.write('ERROR: source_path is required.\n')
        return 1
    if not output_path:
        sys.stderr.write('ERROR: output_path is required.\n')
        return 1

    resource_map = load_resources_from_headers(headers)
    if resource_map is None:
        return 1

    try:
        with open(source_path, 'rb') as f:
            source_plist = plistlib.load(f)
    except Exception as e:
        sys.stderr.write(f"ERROR: loading '{source_path}' failed: {e}\n")
        return 1

    output_plist = convert_plist_value(source_plist, resource_map)
    if output_plist is None:
        return 1

    output_dir = os.path.dirname(output_path)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    try:
        with open(output_path, 'wb') as f:
            plistlib.dump(
                output_plist, f, fmt=plistlib.FMT_XML, sort_keys=False
            )
    except Exception as e:
        sys.stderr.write(f"ERROR: writing '{output_path}' failed: {e}\n")
        return 1

    return 0


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    if not args:
        sys.stderr.write(
            'Usage: ios_strings_tool.py '
            '<generate_localizable_strings|substitute_strings_identifier> '
            '[args...]\n'
        )
        return 1

    subcommand = args[0]
    sub_args = args[1:]
    if subcommand == 'generate_localizable_strings':
        return generate_localizable_strings(sub_args)
    if subcommand == 'substitute_strings_identifier':
        return substitute_strings_identifier(sub_args)

    sys.stderr.write(f'ERROR: unknown subcommand: {subcommand}\n')
    return 1


if __name__ == '__main__':
    sys.exit(main())

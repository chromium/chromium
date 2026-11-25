#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Compiles a Java source file via javac + D8 and packages classes.dex in a ZIP."""

import argparse
import io
import os
import struct
import subprocess
import tempfile
import zipfile

_DEX_HEADER_MIN_SIZE = 0x70
_DEX_MAP_OFF_HEADER_OFFSET = 52
_FIXED_ZIP_DATE_TIME = (1980, 1, 1, 0, 0, 0)

_REQUIRED_DEX_SECTIONS = {
    0x0000: 'HEADER_ITEM',
    0x0001: 'STRING_ID_ITEM',
    0x0002: 'TYPE_ID_ITEM',
    0x0003: 'PROTO_ID_ITEM',
    0x0004: 'FIELD_ID_ITEM',
    0x0005: 'METHOD_ID_ITEM',
    0x0006: 'CLASS_DEF_ITEM',
    0x1000: 'MAP_LIST',
    0x1001: 'TYPE_LIST',
    0x1002: 'ANNOTATION_SET_REF_LIST',
    0x1003: 'ANNOTATION_SET_ITEM',
    0x2000: 'CLASS_DATA_ITEM',
    0x2001: 'CODE_ITEM',
    0x2002: 'STRING_DATA_ITEM',
    0x2003: 'DEBUG_INFO_ITEM',
    0x2004: 'ANNOTATION_ITEM',
    0x2005: 'ENCODED_ARRAY_ITEM',
    0x2006: 'ANNOTATIONS_DIRECTORY_ITEM',
}


def verify_dex(dex_bytes, expected_dex_version):
    """Validates DEX magic and asserts all required map_list sections exist."""
    if len(dex_bytes) < _DEX_HEADER_MIN_SIZE:
        raise ValueError(
            f'DEX file too small: {len(dex_bytes)} < {_DEX_HEADER_MIN_SIZE}'
        )

    expected_magic = f'dex\n{expected_dex_version}\0'.encode('ascii')
    actual_magic = dex_bytes[:8]
    if actual_magic != expected_magic:
        raise ValueError(
            f'Unexpected DEX magic {actual_magic!r}, expected'
            f' {expected_magic!r}'
        )

    (map_off,) = struct.unpack_from('<I', dex_bytes, _DEX_MAP_OFF_HEADER_OFFSET)
    if map_off == 0 or map_off + 4 > len(dex_bytes):
        raise ValueError(
            f'Invalid map_off {map_off} for DEX size {len(dex_bytes)}'
        )

    (map_size,) = struct.unpack_from('<I', dex_bytes, map_off)
    if map_off + 4 + map_size * 12 > len(dex_bytes):
        raise ValueError(
            f'Truncated map_list: map_off={map_off}, map_size={map_size},'
            f' file_size={len(dex_bytes)}'
        )

    present_types = set()
    for i in range(map_size):
        item_type, _unused, _size, _offset = struct.unpack_from(
            '<HHII', dex_bytes, map_off + 4 + i * 12
        )
        present_types.add(item_type)

    missing = [
        f'{name} (0x{code:04x})'
        for code, name in sorted(_REQUIRED_DEX_SECTIONS.items())
        if code not in present_types
    ]
    if missing:
        raise ValueError(
            'Generated DEX is missing required map_list sections: '
            + ', '.join(missing)
        )


def build_deterministic_zip(dex_bytes):
    """Packages classes.dex uncompressed into a deterministic ZIP buffer."""
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, 'w', compression=zipfile.ZIP_STORED) as zf:
        info = zipfile.ZipInfo(
            filename='classes.dex', date_time=_FIXED_ZIP_DATE_TIME
        )
        info.compress_type = zipfile.ZIP_STORED
        info.create_system = 3
        info.external_attr = 0o644 << 16
        zf.writestr(info, dex_bytes)
    return buf.getvalue()


def write_if_changed(output_path, data):
    """Writes data atomically via os.replace."""
    abs_out = os.path.abspath(output_path)
    out_dir = os.path.dirname(abs_out)
    os.makedirs(out_dir, exist_ok=True)
    fd, tmp_path = tempfile.mkstemp(dir=out_dir, prefix='.tmp_dex_zip_')
    try:
        with os.fdopen(fd, 'wb') as f:
            f.write(data)
        os.replace(tmp_path, abs_out)
    except BaseException:
        if os.path.exists(tmp_path):
            os.unlink(tmp_path)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--javac', required=True)
    parser.add_argument('--java', required=True)
    parser.add_argument('--r8-jar', required=True)
    parser.add_argument('--android-jar', required=True)
    parser.add_argument('--min-api', type=int, required=True)
    parser.add_argument('--expected-dex-version', required=True)
    parser.add_argument('--java-src', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmp_dir:
        cls_dir = os.path.join(tmp_dir, 'classes')
        dex_dir = os.path.join(tmp_dir, 'dex')
        os.makedirs(cls_dir)
        os.makedirs(dex_dir)

        subprocess.run(
            [
                args.javac,
                '--release',
                '11',
                '-g',
                '-cp',
                args.android_jar,
                '-d',
                cls_dir,
                args.java_src,
            ],
            check=True,
        )

        class_files = []
        for root, _, files in os.walk(cls_dir):
            for name in files:
                if name.endswith('.class'):
                    class_files.append(os.path.join(root, name))
        class_files.sort()
        if not class_files:
            raise RuntimeError(f'javac produced no .class files for {args.java_src}')

        subprocess.run(
            [
                args.java,
                '-cp',
                args.r8_jar,
                'com.android.tools.r8.D8',
                '--min-api',
                str(args.min_api),
                '--lib',
                args.android_jar,
                '--output',
                dex_dir,
                *class_files,
            ],
            check=True,
        )

        dex_path = os.path.join(dex_dir, 'classes.dex')
        with open(dex_path, 'rb') as f:
            dex_bytes = f.read()

        verify_dex(dex_bytes, args.expected_dex_version)
        zip_bytes = build_deterministic_zip(dex_bytes)
        write_if_changed(args.output, zip_bytes)


if __name__ == '__main__':
    main()

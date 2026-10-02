#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for ios_strings_tool.py."""

import os
import plistlib
import sys
import tempfile
import unittest

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_SRC_ROOT = os.path.abspath(os.path.join(_THIS_DIR, '..', '..', '..', '..'))
sys.path.insert(0, os.path.join(_SRC_ROOT, 'tools', 'grit'))

from grit.format import data_pack
import ios_strings_tool


class IosStringsToolTest(unittest.TestCase):

    def test_generate_localizable_strings_utf8_and_utf16(self):
        """Test generating binary `.strings` from UTF-8 and UTF-16 paks."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            gen_dir = os.path.join(tmp_dir, 'gen')
            pak_dir = os.path.join(tmp_dir, 'paks')
            out_dir = os.path.join(tmp_dir, 'out')
            os.makedirs(os.path.join(gen_dir, 'ios'), exist_ok=True)

            header_path = os.path.join(gen_dir, 'ios', 'test_strings.h')
            with open(header_path, 'w', encoding='utf-8') as f:
                f.write('#pragma once\n')
                f.write('#define IDS_HELLO 1001\n')
                f.write('#define IDS_WORLD 1002\n')

            config_path = os.path.join(tmp_dir, 'config.plist')
            config_data = {
                'headers': ['ios/test_strings.h'],
                'outputs': [
                    {
                        'name': 'Localizable.strings',
                        'strings': [
                            'IDS_HELLO',
                            {'input': 'IDS_WORLD', 'output': 'CustomWorldKey'},
                        ],
                    }
                ],
            }
            with open(config_path, 'wb') as f:
                plistlib.dump(config_data, f)

            en_pak_dir = os.path.join(pak_dir, 'en.lproj')
            pt_pak_dir = os.path.join(pak_dir, 'pt_PT.lproj')
            os.makedirs(en_pak_dir, exist_ok=True)
            os.makedirs(pt_pak_dir, exist_ok=True)

            data_pack.WriteDataPack(
                {1001: 'Hello', 1002: 'World', 9999: b'\xff\xfe\xfd'},
                os.path.join(en_pak_dir, 'locale.pak'),
                data_pack.UTF8,
            )
            data_pack.WriteDataPack(
                {
                    1001: 'Olá'.encode('utf-16-le'),
                    1002: 'Mundo'.encode('utf-16-le'),
                },
                os.path.join(pt_pak_dir, 'locale.pak'),
                data_pack.UTF16,
            )

            rc = ios_strings_tool.main([
                'generate_localizable_strings',
                '-c',
                config_path,
                '-I',
                gen_dir,
                '-p',
                pak_dir,
                '-o',
                out_dir,
                'en-US',
                'pt-PT',
            ])
            self.assertEqual(rc, 0)

            with open(
                os.path.join(out_dir, 'en.lproj', 'Localizable.strings'), 'rb'
            ) as f:
                en_plist = plistlib.load(f)
            self.assertEqual(
                en_plist, {'IDS_HELLO': 'Hello', 'CustomWorldKey': 'World'}
            )

            with open(
                os.path.join(out_dir, 'pt_PT.lproj', 'Localizable.strings'),
                'rb',
            ) as f:
                pt_plist = plistlib.load(f)
            self.assertEqual(
                pt_plist, {'IDS_HELLO': 'Olá', 'CustomWorldKey': 'Mundo'}
            )

    def test_substitute_strings_identifier_success_and_missing_id(self):
        """Test substituting `IDS_`/`IDR_` identifiers in nested plists."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            hdr1 = os.path.join(tmp_dir, 'hdr1.h')
            hdr2 = os.path.join(tmp_dir, 'hdr2.h')
            with open(hdr1, 'w', encoding='utf-8') as f:
                f.write('#define IDS_TITLE 2001\n')
            with open(hdr2, 'w', encoding='utf-8') as f:
                f.write('#define IDR_ICON 3005\n')

            src_plist_path = os.path.join(tmp_dir, 'source.plist')
            out_plist_path = os.path.join(tmp_dir, 'sub', 'output.plist')
            with open(src_plist_path, 'wb') as f:
                plistlib.dump(
                    {
                        'Title': 'IDS_TITLE',
                        'Items': ['IDR_ICON', 'LiteralString', 42],
                    },
                    f,
                )

            rc = ios_strings_tool.main([
                'substitute_strings_identifier',
                '-I',
                hdr1,
                '-I',
                hdr2,
                '-i',
                src_plist_path,
                '-o',
                out_plist_path,
            ])
            self.assertEqual(rc, 0)

            with open(out_plist_path, 'rb') as f:
                out_plist = plistlib.load(f)
            self.assertEqual(
                out_plist,
                {
                    'Title': 2001,
                    'Items': [3005, 'LiteralString', 42],
                },
            )

            # Unmapped IDS_ identifier must fail.
            bad_plist_path = os.path.join(tmp_dir, 'bad.plist')
            with open(bad_plist_path, 'wb') as f:
                plistlib.dump({'Missing': 'IDS_UNKNOWN'}, f)
            rc_bad = ios_strings_tool.main([
                'substitute_strings_identifier',
                '-I',
                hdr1,
                '-i',
                bad_plist_path,
                '-o',
                out_plist_path,
            ])
            self.assertEqual(rc_bad, 1)

    def test_duplicate_header_entry_fails(self):
        """Test that duplicate `#define` keys across headers fail."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            hdr = os.path.join(tmp_dir, 'dup.h')
            with open(hdr, 'w', encoding='utf-8') as f:
                f.write('#define IDS_DUP 10\n#define IDS_DUP 11\n')
            self.assertIsNone(
                ios_strings_tool.load_resources_from_headers([hdr])
            )


if __name__ == '__main__':
    unittest.main()

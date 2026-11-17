#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for generate_character_emoji_data.py."""

import os
from pathlib import Path
import tempfile
import unittest

import generate_character_emoji_data as gen


class GenerateCharacterEmojiDataTest(unittest.TestCase):
    def test_resolve_source_default(self):
        location, is_url = gen.resolve_source(None)
        self.assertTrue(is_url)
        self.assertEqual(
            location,
            'https://www.unicode.org/Public/UNIDATA/emoji/emoji-data.txt',
        )

    def test_resolve_source_version(self):
        for arg, expected_ver in (
            ('17.0.0', '17.0.0'),
            ('17.0', '17.0.0'),
            ('17', '17.0.0'),
            ('15.1', '15.1.0'),
        ):
            location, is_url = gen.resolve_source(arg)
            self.assertTrue(is_url)
            self.assertEqual(
                location,
                f'https://unicode.org/Public/{expected_ver}/ucd/emoji/emoji-data.txt',
            )

    def test_resolve_source_url_and_path(self):
        url = 'https://example.com/emoji-data.txt'
        location, is_url = gen.resolve_source(url)
        self.assertTrue(is_url)
        self.assertEqual(location, url)

        with tempfile.NamedTemporaryFile() as tmp:
            location, is_url = gen.resolve_source(tmp.name)
            self.assertFalse(is_url)
            self.assertEqual(location, tmp.name)

        # Existing file named like a version should be treated as a path.
        with tempfile.TemporaryDirectory() as tmpdir:
            ver_file = Path(tmpdir) / '17.0.0'
            ver_file.write_text('', encoding='utf-8')
            old_cwd = Path.cwd()
            try:
                os.chdir(tmpdir)
                location, is_url = gen.resolve_source('17.0.0')
                self.assertFalse(is_url)
                self.assertEqual(location, '17.0.0')
            finally:
                os.chdir(old_cwd)

    def test_generate_header_metadata_and_merging(self):
        sample = """\
# emoji-data.txt
# Date: 2025-07-25, 17:54:31 GMT
# Version: 17.0

0023          ; Emoji                # E0.0   [1] (#)
0030..0039    ; Emoji                # E0.0  [10] (0..9)
23E9..23EC    ; Emoji                # E0.6   [4]
23ED..23EE    ; Emoji                # E0.7   [2]
23EF          ; Emoji                # E1.0   [1]
231A..231B    ; Emoji_Presentation   # E0.6   [2]
261D          ; Emoji_Modifier_Base  # E0.6   [1]
"""
        with self.assertLogs(gen.__name__, level='INFO') as log_ctx:
            header = gen.generate_header(
                sample,
                source_comment=gen.DEFAULT_URL,
            )
        self.assertTrue(
            any('version 17.0' in msg for msg in log_ctx.output),
            log_ctx.output,
        )
        self.assertIn(
            '// Source: https://unicode.org/Public/17.0.0/ucd/emoji/emoji-data.txt',
            header,
        )
        self.assertIn('// Date: 2025-07-25, 17:54:31 GMT', header)
        self.assertIn('// Version: 17.0', header)
        self.assertIn(
            'inline constexpr std::array<uint8_t, 4> kEmojiDataVersion = {17, 0, 0, 0};',
            header,
        )
        # 23E9..23EC, 23ED..23EE, 23EF should merge into 0x23E9, 0x23EF.
        self.assertIn('0x0030, 0x0039, 0x23E9, 0x23EF,', header)
        self.assertIn('0x0023,', header)
        self.assertIn(
            'inline constexpr std::array<UChar32, 0> kIsEmojiPresentationArray{};',
            header,
        )
        self.assertIn(
            'inline constexpr std::array<UChar32, 0> kIsEmojiModifierBaseRanges{};',
            header,
        )


if __name__ == '__main__':
    unittest.main()

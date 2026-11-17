#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates character_emoji_data.h from Unicode emoji-data.txt."""

import argparse
import logging
from pathlib import Path
import re
import sys
import urllib.parse
import urllib.request

DEFAULT_URL = 'https://www.unicode.org/Public/UNIDATA/emoji/emoji-data.txt'
VERSIONED_URL_TEMPLATE = (
    'https://unicode.org/Public/{version}/ucd/emoji/emoji-data.txt'
)
DEFAULT_OUTPUT_PATH = Path(__file__).with_name('character_emoji_data.h')

PROPERTIES = (
    ('Emoji', 'kIsEmoji'),
    ('Emoji_Presentation', 'kIsEmojiPresentation'),
    ('Emoji_Modifier_Base', 'kIsEmojiModifierBase'),
)

_VERSION_RE = re.compile(r'^\d+(?:\.\d+){0,2}$')
_logger = logging.getLogger(__name__)


def normalize_ucd_version(version: str) -> str:
    parts = version.split('.')
    while len(parts) < 3:
        parts.append('0')
    return '.'.join(parts)


def resolve_source(source: str | None) -> tuple[str, bool]:
    """Resolves `source` to `(location, is_url)`."""
    if not source:
        return DEFAULT_URL, True
    parsed = urllib.parse.urlparse(source)
    if parsed.scheme in ('http', 'https', 'file'):
        return source, True
    if (
        source != '-'
        and not Path(source).exists()
        and _VERSION_RE.match(source)
    ):
        version = normalize_ucd_version(source)
        return VERSIONED_URL_TEMPLATE.format(version=version), True
    return source, False


def read_source_text(location: str, is_url: bool) -> str:
    _logger.info('Reading emoji data from %s', location)
    if is_url:
        request = urllib.request.Request(
            location, headers={'User-Agent': 'Chromium-Blink-Emoji-Generator'}
        )
        with urllib.request.urlopen(request) as response:
            return response.read().decode('utf-8')
    if location == '-':
        return sys.stdin.read()
    return Path(location).read_text(encoding='utf-8')


def parse_emoji_data(
    text: str,
) -> tuple[str | None, str | None, dict[str, list[tuple[int, int]]]]:
    date_value = None
    version_value = None
    intervals_by_prop: dict[str, list[tuple[int, int]]] = {
        prop: [] for prop, _ in PROPERTIES
    }

    for raw_line in text.splitlines():
        stripped = raw_line.strip()
        if stripped.startswith('#'):
            comment = stripped[1:].strip()
            if date_value is None and comment.startswith('Date:'):
                date_value = comment[len('Date:') :].strip()
            elif version_value is None and comment.startswith('Version:'):
                version_value = comment[len('Version:') :].strip()
            continue

        data_part = raw_line.split('#', 1)[0].strip()
        if not data_part:
            continue

        fields = [field.strip() for field in data_part.split(';')]
        if len(fields) < 2:
            continue
        code_points_field, prop_name = fields[0], fields[1]
        if prop_name not in intervals_by_prop:
            continue

        if '..' in code_points_field:
            start_hex, end_hex = code_points_field.split('..', 1)
            start, end = int(start_hex, 16), int(end_hex, 16)
        else:
            start = end = int(code_points_field, 16)
        intervals_by_prop[prop_name].append((start, end))

    return date_value, version_value, intervals_by_prop


def merge_intervals(
    intervals: list[tuple[int, int]],
) -> tuple[list[int], list[tuple[int, int]]]:
    if not intervals:
        return [], []
    sorted_intervals = sorted(intervals)
    merged = [sorted_intervals[0]]
    for start, end in sorted_intervals[1:]:
        prev_start, prev_end = merged[-1]
        if start <= prev_end + 1:
            merged[-1] = (prev_start, max(prev_end, end))
        else:
            merged.append((start, end))

    singles = [start for start, end in merged if start == end]
    ranges = [(start, end) for start, end in merged if start < end]
    return singles, ranges


def parse_version_array(version_value: str | None) -> list[int]:
    if not version_value:
        raise ValueError('Missing "# Version:" header in emoji-data.txt')
    parts = [int(p) for p in re.findall(r'\d+', version_value)[:4]]
    if not parts:
        raise ValueError(f'Invalid "# Version:" value: {version_value!r}')
    while len(parts) < 4:
        parts.append(0)
    return parts


def format_uchar32_array(name: str, values: list[int]) -> list[str]:
    if not values:
        return [f'inline constexpr std::array<UChar32, 0> {name}{{}};']

    lines = [f'inline constexpr auto {name} = std::to_array<UChar32>({{']
    current = '   '
    for value in values:
        token = f' 0x{value:04X},'
        if len(current) + len(token) > 80:
            lines.append(current)
            current = '   ' + token
        else:
            current += token
    if current.strip():
        lines.append(current)
    lines.append('});')
    return lines


def generate_header(text: str, source_comment: str | None = None) -> str:
    date_value, version_value, intervals_by_prop = parse_emoji_data(text)
    version_array = parse_version_array(version_value)
    _logger.info('Parsed emoji-data.txt version %s', version_value)

    lines = [
        '// Copyright 2026 The Chromium Authors',
        '// Use of this source code is governed by a BSD-style license that can be',
        '// found in the LICENSE file.',
        '',
        '// Generated by `generate_character_emoji_data.py`. DO NOT EDIT.',
    ]
    # Convert the default version-less URL to the versioned URL.
    if source_comment == DEFAULT_URL:
        ucd_version = '.'.join(str(v) for v in version_array[:3])
        source_comment = VERSIONED_URL_TEMPLATE.format(version=ucd_version)
    if source_comment:
        lines.append(f'// Source: {source_comment}')
    if date_value:
        lines.append(f'// Date: {date_value}')
    if version_value:
        lines.append(f'// Version: {version_value}')

    version_initializer = ', '.join(str(v) for v in version_array)
    lines.extend(
        [
            '',
            '#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_EMOJI_DATA_H_',
            '#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_EMOJI_DATA_H_',
            '',
            '#include <stddef.h>',
            '#include <stdint.h>',
            '#include <unicode/uchar.h>',
            '',
            '#include <array>',
            '#include <string_view>',
            '',
            '#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"',
            '',
            'namespace blink {',
            '',
            'consteval std::array<uint8_t, 4> ParseUnicodeVersion(std::string_view version) {',
            '  std::array<uint8_t, 4> result = {0, 0, 0, 0};',
            '  size_t index = 0;',
            '  for (char ch : version) {',
            "    if (ch == '.') {",
            '      ++index;',
            '      if (index >= result.size()) {',
            '        break;',
            '      }',
            "    } else if (ch >= '0' && ch <= '9') {",
            "      result[index] = static_cast<uint8_t>(result[index] * 10 + (ch - '0'));",
            '    } else {',
            '      break;',
            '    }',
            '  }',
            '  return result;',
            '}',
            '',
            f'inline constexpr std::array<uint8_t, 4> kEmojiDataVersion = {{{version_initializer}}};',
            '',
            '// True when `character_emoji_data.h` has a newer Unicode version than ICU,',
            '// in which case `character_property_data_generator` serializes an 8-bit',
            '// UCPTrie for Emoji properties instead of querying ICU at runtime.',
            'inline constexpr bool kUseBlinkEmojiData =',
            '    kEmojiDataVersion > ParseUnicodeVersion(U_UNICODE_VERSION);',
            '',
            'enum EmojiPropertyBits : uint8_t {',
            '  kIsEmojiBit = 1 << 0,',
            '  kIsEmojiPresentationBit = 1 << 1,',
            '  kIsEmojiModifierBaseBit = 1 << 2,',
            '};',
            '',
            '// clang-format off',
        ]
    )

    for prop_name, cpp_prefix in PROPERTIES:
        singles, ranges = merge_intervals(intervals_by_prop[prop_name])
        flat_ranges = [cp for pair in ranges for cp in pair]
        lines.append('')
        lines.extend(format_uchar32_array(f'{cpp_prefix}Array', singles))
        lines.append('')
        lines.extend(format_uchar32_array(f'{cpp_prefix}Ranges', flat_ranges))

    lines.extend(
        [
            '// clang-format on',
            '',
            '// Serialized 8-bit UCPTrie generated when `kUseBlinkEmojiData` is true.',
            'extern const int32_t kSerializedEmojiDataSize;',
            'extern const uint8_t kSerializedEmojiData[];',
            '',
            '}  // namespace blink',
            '',
            '#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_EMOJI_DATA_H_',
            '',
        ]
    )
    return '\n'.join(lines)


def main(argv: list[str] | None = None) -> int:
    logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
    parser = argparse.ArgumentParser(
        description='Generate character_emoji_data.h from Unicode emoji-data.txt.'
    )
    parser.add_argument(
        'source',
        nargs='?',
        help=(
            'Optional Unicode version (e.g., 17.0.0), URL, or file path. '
            f'Defaults to {DEFAULT_URL}'
        ),
    )
    parser.add_argument(
        '-o',
        '--output',
        default=DEFAULT_OUTPUT_PATH,
        help='Output file path, or "-" for stdout (default: %(default)s).',
    )
    args = parser.parse_args(argv)

    location, is_url = resolve_source(args.source)
    text = read_source_text(location, is_url)
    source_comment = (
        location
        if is_url
        and urllib.parse.urlparse(location).scheme in ('http', 'https')
        else None
    )
    header = generate_header(text, source_comment=source_comment)

    if args.output == '-':
        sys.stdout.write(header)
    else:
        output_path = Path(args.output)
        output_path.write_text(header, encoding='utf-8')
        _logger.info('Generated at %s', output_path)
    return 0


if __name__ == '__main__':
    sys.exit(main())

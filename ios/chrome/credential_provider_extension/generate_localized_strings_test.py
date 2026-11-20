#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for generate_localized_strings.py."""

import contextlib
import io
import os
import plistlib
import sys
import tempfile
import unittest

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

import generate_localized_strings  # noqa: E402


class GenerateLocalizedStringsTest(unittest.TestCase):
    def setUp(self):
        tmp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(tmp_dir.cleanup)
        self.tmp_dir = tmp_dir.name

    def _write_grd(self, filename, content):
        grd_path = os.path.join(self.tmp_dir, filename)
        with open(grd_path, 'w', encoding='utf-8', newline='') as f:
            f.write(content)
        return grd_path

    def test_snake_to_camel(self):
        self.assertEqual(
            generate_localized_strings.snake_to_camel(
                'IDS_IOS_CREDENTIAL_PROVIDER_OK'
            ),
            'CredentialProviderOkString',
        )
        self.assertEqual(
            generate_localized_strings.snake_to_camel(
                'IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_MORE_INFO_STRING'
            ),
            'CredentialProviderConsentMoreInfoString',
        )
        self.assertEqual(
            generate_localized_strings.snake_to_camel(
                'IDS_IOS_CREDENTIAL_PROVIDER_STEP_1'
            ),
            'CredentialProviderStep1String',
        )
        self.assertEqual(
            generate_localized_strings.snake_to_camel(
                'IDS_CREDENTIAL_PROVIDER_OK'
            ),
            'IdsCredentialProviderOkString',
        )

    def test_main_generates_plist_header_and_source(self):
        grd_path = self._write_grd(
            'strings.grd',
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<grit><release seq="1"><messages>\n'
            '  <message>Nameless</message>\n'
            '  <message name="NON_IDS_ENTRY">Ignored</message>\n'
            '  <message name="IDS_IOS_CREDENTIAL_PROVIDER_OK">'
            'OK</message>\n'
            '  <message name="IDS_IOS_CREDENTIAL_PROVIDER_DONE">'
            'Done</message>\n'
            '</messages></release></grit>\n',
        )
        plist_out = os.path.join(self.tmp_dir, 'out', 'config.plist')
        header_out = os.path.join(self.tmp_dir, 'out', 'strings.h')
        source_out = os.path.join(self.tmp_dir, 'out', 'strings.mm')

        rc = generate_localized_strings.main(
            [
                '--grd',
                grd_path,
                '--plist-out',
                plist_out,
                '--header-out',
                header_out,
                '--source-out',
                source_out,
            ]
        )
        self.assertEqual(rc, 0)

        with open(plist_out, 'rb') as f:
            plist_data = plistlib.load(f)
        self.assertEqual(
            plist_data,
            {
                'headers': [
                    'ios/credential_provider_extension/grit/'
                    'ios_credential_provider_extension_strings.h'
                ],
                'outputs': [
                    {
                        'name': 'Localizable.strings',
                        'strings': [
                            'IDS_IOS_CREDENTIAL_PROVIDER_OK',
                            'IDS_IOS_CREDENTIAL_PROVIDER_DONE',
                        ],
                    }
                ],
            },
        )

        with open(header_out, 'r', encoding='utf-8') as f:
            header_text = f.read()
        guard = (
            'IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_'
            'GENERATED_LOCALIZED_STRINGS_H_'
        )
        self.assertIn(f'#ifndef {guard}\n#define {guard}\n', header_text)
        self.assertIn('#import <Foundation/Foundation.h>\n', header_text)
        self.assertIn('NSString* CredentialProviderOkString();\n', header_text)
        self.assertIn(
            'NSString* CredentialProviderDoneString();\n', header_text
        )
        self.assertIn(f'#endif  // {guard}\n', header_text)
        self.assertNotIn('NonIdsEntryString', header_text)

        with open(source_out, 'r', encoding='utf-8') as f:
            source_text = f.read()
        self.assertNotIn('NON_IDS_ENTRY', source_text)
        self.assertIn(
            '#import "ios/chrome/credential_provider_extension/'
            'generated_localized_strings.h"\n',
            source_text,
        )
        self.assertIn(
            'NSString* CredentialProviderOkString() {\n'
            '  return NSLocalizedString('
            '@"IDS_IOS_CREDENTIAL_PROVIDER_OK", @"");\n'
            '}\n',
            source_text,
        )
        self.assertIn(
            'NSString* CredentialProviderDoneString() {\n'
            '  return NSLocalizedString('
            '@"IDS_IOS_CREDENTIAL_PROVIDER_DONE", @"");\n'
            '}\n',
            source_text,
        )

    def test_main_fails_gracefully_on_corrupt_grd(self):
        grd_path = self._write_grd('corrupt.grd', '<invalid><xml>')
        plist_out = os.path.join(self.tmp_dir, 'out.plist')
        header_out = os.path.join(self.tmp_dir, 'out.h')
        source_out = os.path.join(self.tmp_dir, 'out.mm')
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            rc = generate_localized_strings.main(
                [
                    '--grd',
                    grd_path,
                    '--plist-out',
                    plist_out,
                    '--header-out',
                    header_out,
                    '--source-out',
                    source_out,
                ]
            )
        self.assertNotEqual(rc, 0)
        self.assertIn('Error:', stderr.getvalue())
        self.assertFalse(os.path.exists(plist_out))
        self.assertFalse(os.path.exists(header_out))
        self.assertFalse(os.path.exists(source_out))

    def test_main_preserves_mtimes_when_outputs_unchanged(self):
        grd_path = self._write_grd(
            'strings.grd',
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<grit><release seq="1"><messages>\n'
            '  <message name="IDS_IOS_CREDENTIAL_PROVIDER_OK">OK</message>\n'
            '</messages></release></grit>\n',
        )
        out_dir = os.path.join(self.tmp_dir, 'out')
        plist_out = os.path.join(out_dir, 'config.plist')
        header_out = os.path.join(out_dir, 'strings.h')
        source_out = os.path.join(out_dir, 'strings.mm')
        argv = [
            '--grd',
            grd_path,
            '--plist-out',
            plist_out,
            '--header-out',
            header_out,
            '--source-out',
            source_out,
        ]
        self.assertEqual(generate_localized_strings.main(argv), 0)

        sentinel_mtime = 1000000
        for path in (plist_out, header_out, source_out):
            os.utime(path, (sentinel_mtime, sentinel_mtime))

        # Re-running with identical IDs should skip rewriting outputs.
        self.assertEqual(generate_localized_strings.main(argv), 0)
        for path in (plist_out, header_out, source_out):
            self.assertEqual(os.path.getmtime(path), sentinel_mtime)
        self.assertEqual(
            sorted(os.listdir(out_dir)),
            ['config.plist', 'strings.h', 'strings.mm'],
        )

        # Adding a new message ID should atomically replace all three outputs.
        self._write_grd(
            'strings.grd',
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<grit><release seq="1"><messages>\n'
            '  <message name="IDS_IOS_CREDENTIAL_PROVIDER_OK">OK</message>\n'
            '  <message name="IDS_IOS_CREDENTIAL_PROVIDER_DONE">Done</message>'
            '\n</messages></release></grit>\n',
        )
        self.assertEqual(generate_localized_strings.main(argv), 0)
        for path in (plist_out, header_out, source_out):
            self.assertNotEqual(os.path.getmtime(path), sentinel_mtime)
        self.assertEqual(
            sorted(os.listdir(out_dir)),
            ['config.plist', 'strings.h', 'strings.mm'],
        )

    def test_fails_on_part_element(self):
        grd_path = self._write_grd(
            'part.grd',
            '<grit><release seq="1"><messages>'
            '<part file="extra.grdp" />'
            '<message name="IDS_IOS_FOO">One</message>'
            '</messages></release></grit>',
        )
        with self.assertRaisesRegex(ValueError, 'Unsupported <part>'):
            generate_localized_strings.extract_strings_from_grd(grd_path)

    def test_fails_on_if_element(self):
        grd_path = self._write_grd(
            'if.grd',
            '<grit><release seq="1"><messages>'
            '<if expr="is_ios">'
            '<message name="IDS_IOS_FOO">One</message>'
            '</if>'
            '</messages></release></grit>',
        )
        with self.assertRaisesRegex(ValueError, 'Unsupported <if>'):
            generate_localized_strings.extract_strings_from_grd(grd_path)

    def test_fails_on_invalid_accessor_identifier(self):
        grd_path = self._write_grd(
            'invalid_id.grd',
            '<grit><release seq="1"><messages>'
            '<message name="IDS_IOS_123_INVALID">One</message>'
            '</messages></release></grit>',
        )
        with self.assertRaisesRegex(ValueError, 'Invalid accessor name'):
            generate_localized_strings.extract_strings_from_grd(grd_path)

    def test_fails_on_empty_grd(self):
        grd_path = self._write_grd(
            'empty.grd',
            '<grit><release seq="1"><messages/></release></grit>',
        )
        with self.assertRaisesRegex(ValueError, 'No IDS_'):
            generate_localized_strings.extract_strings_from_grd(grd_path)

    def test_fails_when_only_non_ids_or_nameless_messages(self):
        grd_path = self._write_grd(
            'non_ids.grd',
            '<grit><release seq="1"><messages>'
            '<message>Nameless</message>'
            '<message name="OTHER_NAME">Other</message>'
            '</messages></release></grit>',
        )
        with self.assertRaisesRegex(ValueError, 'No IDS_'):
            generate_localized_strings.extract_strings_from_grd(grd_path)

    def test_fails_on_duplicate_message_id(self):
        grd_path = self._write_grd(
            'dup.grd',
            '<grit><release seq="1"><messages>'
            '<message name="IDS_IOS_FOO">One</message>'
            '<message name="IDS_IOS_FOO">Two</message>'
            '</messages></release></grit>',
        )
        with self.assertRaisesRegex(ValueError, 'Duplicate message ID'):
            generate_localized_strings.extract_strings_from_grd(grd_path)

    def test_fails_on_accessor_name_collision(self):
        grd_path = self._write_grd(
            'collision.grd',
            '<grit><release seq="1"><messages>'
            '<message name="IDS_IOS_FOO">One</message>'
            '<message name="IDS_IOS_FOO_STRING">Two</message>'
            '</messages></release></grit>',
        )
        with self.assertRaisesRegex(ValueError, 'Accessor name collision'):
            generate_localized_strings.extract_strings_from_grd(grd_path)


if __name__ == '__main__':
    unittest.main()

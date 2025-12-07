# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sorts the flags in ios_chrome_flag_descriptions.h and .cc files."""

import argparse
import os
import re
import subprocess
import sys


def sort_flags_in_file(file_path, check_only=False):
    """Sorts the flags in the .h file.

    Args:
        file_path: Path to the .h file.
        check_only: If True, doesn't modify the file, only returns status.

    Returns:
        True if the file was unsorted, False otherwise.
    """
    with open(file_path, 'r') as f:
        content = f.read()

    header = content.split('namespace flag_descriptions {')[0]
    footer = content.split('}  // namespace flag_descriptions')[1]
    body = content.split('namespace flag_descriptions {')[1].split(
        '}  // namespace flag_descriptions')[0]

    # Find all flag definitions, allowing for newlines between keywords.
    flags = re.findall(r'(extern\s+const\s+char\s+k.*?Description\[\];)',
                       body.strip(), re.DOTALL)

    # Create a list of dictionaries, each containing the name and the
    # full text of the flag
    flag_list = []
    for flag in flags:
        # Allow for newlines between keywords when finding the name.
        name_match = re.search(r'extern\s+const\s+char\s+(k.*?Name)\[\];',
                               flag, re.DOTALL)
        if name_match:
            flag_list.append({'name': name_match.group(1), 'text': flag})
        else:
            print("Error: \"Name\" is missing for: \n" + flag)
            return False

    # Sort the list of flags alphabetically by name
    sorted_flags = sorted(flag_list, key=lambda x: x['name'])

    # Reconstruct the body with the sorted flags
    sorted_body = '\n\n'.join(flag['text'] for flag in sorted_flags)

    if body.strip() == sorted_body:
        return False

    if not check_only:
        # Reconstruct the full content
        new_content = (header + 'namespace flag_descriptions {\n\n' +
                       sorted_body + '\n\n}  // namespace flag_descriptions' +
                       footer)

        with open(file_path, 'w') as f:
            f.write(new_content)
    return True


def sort_flags_in_cc_file(file_path, check_only=False):
    """Sorts the flags in the .cc file.

    Args:
        file_path: Path to the .cc file.
        check_only: If True, doesn't modify the file, only returns status.

    Returns:
        True if the file was unsorted, False otherwise.
    """
    with open(file_path, 'r') as f:
        content = f.read()

    header = content.split('namespace flag_descriptions {')[0]
    footer = content.split(
        '// Please insert your name/description above in alphabetical order.'
    )[1]
    body = content.split('namespace flag_descriptions {')[1].split(
        '// Please insert your name/description above in alphabetical order.'
    )[0]

    # Find all flag definitions, specifically matching quoted strings to handle
    # semicolons within the description.
    flags = re.findall(r'(const\s+char\s+k.*?Description\[\]\s*=\s*".*?";)',
                       body.strip(), re.DOTALL)

    # Create a list of dictionaries, each containing the name
    # and the full text of the flag
    flag_list = []
    for flag in flags:
        # Allow for newlines between keywords when finding the name.
        name_match = re.search(r'const\s+char\s+(k.*?Name)\[\]', flag,
                               re.DOTALL)
        if name_match:
            flag_list.append({'name': name_match.group(1), 'text': flag})

    # Sort the list of flags alphabetically by name
    sorted_flags = sorted(flag_list, key=lambda x: x['name'])

    # Reconstruct the body with the sorted flags
    sorted_body = '\n\n'.join(flag['text'] for flag in sorted_flags)

    if body.strip() == sorted_body:
        return False

    if not check_only:
        # Reconstruct the full content
        new_content = (
            header + 'namespace flag_descriptions {\n\n' + sorted_body +
            '\n\n// Please insert your name/description above in alphabetical '
            'order.' + footer)

        with open(file_path, 'w') as f:
            f.write(new_content)
    return True


def main():
    # Block execution on Windows. Do not return error to avoid presubmit error.
    if sys.platform == 'win32':
        print("Error: This script is not supported on Windows.")
        return 0

    parser = argparse.ArgumentParser(
        description="Alphabetically sorts variables in"
        "ios_chrome_flag_descriptions files.")
    parser.add_argument(
        '--h-file',
        default='ios/chrome/browser/flags/ios_chrome_flag_descriptions.h')
    parser.add_argument(
        '--cc-file',
        default='ios/chrome/browser/flags/ios_chrome_flag_descriptions.cc')
    parser.add_argument(
        '--check',
        action='store_true',
        help="Don't modify files, just check if they are sorted and exit.")
    args = parser.parse_args()

    gclient_root = subprocess.check_output(['gclient',
                                            'root']).decode('utf-8').strip()
    src_root = os.path.join(gclient_root, 'src')

    h_file_path = os.path.join(src_root, args.h_file)
    cc_file_path = os.path.join(src_root, args.cc_file)

    h_changed = sort_flags_in_file(h_file_path, args.check)
    cc_changed = sort_flags_in_cc_file(cc_file_path, args.check)

    if h_changed or cc_changed:
        if args.check:
            print(
                "Error: ios_chrome_flag_descriptions.h or .cc is not sorted.")
        else:
            print("ios_chrome_flag_descriptions.h or .cc was not sorted. "
                  "Files have been automatically sorted.")
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

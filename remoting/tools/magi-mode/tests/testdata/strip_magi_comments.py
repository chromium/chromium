#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import re
import sys

def main():
    parser = argparse.ArgumentParser(
        description="Strip MAGI comments from files"
    )
    parser.add_argument("--input", required=True, help="Input file")
    parser.add_argument("--output", required=True, help="Output file")
    args = parser.parse_args()

    try:
        with open(args.input, 'r', encoding='utf-8') as f:
            content = f.read()

        # Strip lines containing // MAGI: (must be on their own line)
        cleaned_content = re.sub(r'(?m)^[ \t]*// MAGI:.*(?:\n|$)', '', content)

        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(cleaned_content)

        print(
            f"Successfully stripped comments from {args.input} to {args.output}"
        )
    except Exception as e:
        print(f"Error processing file: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()

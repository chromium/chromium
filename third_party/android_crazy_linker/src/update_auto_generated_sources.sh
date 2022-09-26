#!/bin/sh
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A simple script used to regenerate all sources that are normally
# auto-generated from scripts located under tests/

set -e
export LANG=C
export LC_ALL=C

PROGDIR=$(dirname "$0")
cd "$PROGDIR"

# Do not run this every time for now, because the script hard-codes the current
# date in each generated archive, making the content change on every call.
# TODO(digit): Fix this.
# tests/generate_zip_test_tables.sh > src/crazy_linker_zip_test_data.cpp

tests/generate_test_elf_hash_tables.py > \
    src/crazy_linker_elf_hash_table_test_data.h

tests/generate_test_gnu_hash_tables.py > \
    src/crazy_linker_gnu_hash_table_test_data.h

echo "Done"

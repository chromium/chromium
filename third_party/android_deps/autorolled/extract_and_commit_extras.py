#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import pathlib
import sys

_SRC_ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(_SRC_ROOT / 'build/autoroll'))
import extract_and_commit_util as ecu

_AUTOROLLED_PATH = _SRC_ROOT / 'third_party/android_deps/autorolled'
_COMMITED_DIR_NAME = 'committed'

ecu.main(committed_dir_path=_AUTOROLLED_PATH / _COMMITED_DIR_NAME)

#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import sys

# Outside docker: //path/to/project/3pp/fetch.py
# Inside docker: //path/to/project/install.py
_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _THIS_DIR.parents[3 if _THIS_DIR.name == '3pp' else 2]

sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))
import maven


maven.main(package='com.google.errorprone:javac', maven_url=maven.APACHE_MAVEN_URL)

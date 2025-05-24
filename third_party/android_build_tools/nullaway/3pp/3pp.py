#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import shutil
import sys
import zipfile

# Outside docker: //path/to/project/3pp/fetch.py
# Inside docker: //path/to/project/install.py
_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _THIS_DIR.parents[3 if _THIS_DIR.name == '3pp' else 2]

sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))
import maven


def _post_process(src_jar_path, dst_jar_path):
    with zipfile.ZipFile(src_jar_path) as in_z:
        with zipfile.ZipFile(dst_jar_path, 'w') as out_z:
            for info in in_z.infolist():
                # Not necessary, but why not.
                if info.is_dir():
                    continue
                # Remove classes that already exist within errorprone.jar.
                if info.filename.startswith('com/google'):
                    continue
                out_z.writestr(info, in_z.read(info))


maven.main(package='com.uber.nullaway:nullaway',
           maven_url=maven.APACHE_MAVEN_URL,
           post_process_func=_post_process)

#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Packages signflinger (plus zipflinger and apksig) along with ZipBuilder.

ZipBuilder is a small Chromium-authored main class (see ../java) that
apkbuilder.py and dex.py use to create zips and sign APKs in a single pass.
"""

import os
import pathlib
import shutil
import sys
import zipfile

# Outside docker: //path/to/project/3pp/fetch.py
# Inside docker: //path/to/project/install.py
_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _THIS_DIR.parents[3 if _THIS_DIR.name == '3pp' else 2]

sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))
import common
import maven

_JAVA_SRC = 'third_party/android_build_tools/signflinger/java'
# Path within the checkout. Cannot be under third_party/android_build_tools/
# signflinger, since the 3pp recipe copies the package definition (this
# directory) there after "checkout" runs, and fails if it already exists.
_JAVA_CHECKOUT_SUBPATH = 'signflinger_java'
_MAIN_CLASS_SRC = 'src/org/chromium/build/ZipBuilder.java'

# A GraalVM native-image build (see //build/3pp_common/graalvm.py) was
# evaluated for this tool and did not improve runtime: zipping was ~10% faster
# (no JIT warm-up), but signing was slower because native-image's portable CPU
# targets lack SHA-NI intrinsics, whereas the JVM selects them at startup.
# Overall the JVM version was faster, so this package is a plain .jar.


def _post_process(src_jar_path, dst_jar_path):
    """Compiles ZipBuilder.java against the fat jar and adds it to the jar."""
    javac = common.path_within_checkout('third_party/jdk/current/bin/javac')
    java_dir = common.path_within_checkout(_JAVA_CHECKOUT_SUBPATH)
    classes_dir = pathlib.Path('zipbuilder_classes').resolve()
    common.run_cmd([
        javac,
        '--release',
        '17',
        '-Werror',
        '-Xlint:unchecked,deprecation',
        '-cp',
        src_jar_path,
        '-d',
        str(classes_dir),
        os.path.join(java_dir, _MAIN_CLASS_SRC),
    ])

    shutil.move(src_jar_path, dst_jar_path)
    with zipfile.ZipFile(dst_jar_path, 'a', zipfile.ZIP_DEFLATED) as z:
        for path in sorted(classes_dir.rglob('*')):
            if path.is_file():
                z.write(str(path), str(path.relative_to(classes_dir)))


maven.main(
    package='com.android:signflinger',
    post_process_func=_post_process,
    runtime_deps=[('//' + _JAVA_SRC, _JAVA_CHECKOUT_SUBPATH)],
    version_deps=['//' + _JAVA_SRC],
)

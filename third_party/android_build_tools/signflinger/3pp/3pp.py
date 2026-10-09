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
import graalvm
import maven

_JAVA_SRC = 'third_party/android_build_tools/signflinger/java'
# Path within the checkout. Cannot be under third_party/android_build_tools/
# signflinger, since the 3pp recipe copies the package definition (this
# directory) there after "checkout" runs, and fails if it already exists.
_JAVA_CHECKOUT_SUBPATH = 'signflinger_java'
_MAIN_CLASS_SRC = 'src/org/chromium/build/ZipBuilder.java'
_MAIN_CLASS = 'org.chromium.build.ZipBuilder'


def _gen_keystore(keytool, path, storetype):
    common.run_cmd([
        keytool,
        '-genkeypair',
        '-keystore',
        path,
        '-storetype',
        storetype,
        '-storepass',
        'password',
        '-keypass',
        'password',
        '-alias',
        'key',
        '-keyalg',
        'RSA',
        '-keysize',
        '2048',
        '-sigalg',
        'SHA256withRSA',
        '-dname',
        'CN=Chromium',
        '-validity',
        '1',
    ])


def _post_process(src_jar_path, dst_jar_path):
    """Compiles ZipBuilder.java into the jar and builds a GraalVM binary."""
    javac = common.path_within_checkout('third_party/jdk/current/bin/javac')
    keytool = common.path_within_checkout('third_party/jdk/current/bin/keytool')
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

    # Create sample inputs covering all ZipBuilder entry types (BytesSource,
    # StoredFileSource >= 64 KiB, single-entry ZipSource, add-all ZipSource)
    # and both JKS and PKCS12 keystores for the GraalVM tracing agent.
    trace_dir = pathlib.Path('trace_inputs').resolve()
    trace_dir.mkdir()
    small_file = trace_dir / 'small.txt'
    small_file.write_bytes(b'hello\n' * 100)
    large_file = trace_dir / 'large.bin'
    large_file.write_bytes(b'x' * (128 * 1024))
    src_zip = trace_dir / 'input.zip'
    with zipfile.ZipFile(src_zip, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('AndroidManifest.xml', b'<manifest/>')
        z.writestr('res/raw/foo.txt', b'foo')
        z.writestr('res/raw/bar.txt', b'bar')

    spec_file = trace_dir / 'spec.txt'
    spec_file.write_text(
        '\n'.join([
            f'AndroidManifest.xml\t-1\t0\t{src_zip}\tAndroidManifest.xml',
            f'assets/small.txt\t0\t4\t{small_file}',
            f'assets/compressed.txt\t1\t0\t{small_file}',
            f'lib/x86_64/libtest.so\t0\t16384\t{large_file}',
            f'\t-1\t4\t{src_zip}\t',
        ])
        + '\n',
        encoding='utf-8',
    )

    jks_keystore = str(trace_dir / 'test.jks')
    p12_keystore = str(trace_dir / 'test.p12')
    _gen_keystore(keytool, jks_keystore, 'JKS')
    _gen_keystore(keytool, p12_keystore, 'PKCS12')

    def make_tracing_args(output_name, keystore, min_sdk, *, v1, v3, v4):
        return [
            '--output',
            str(trace_dir / output_name),
            '--spec',
            str(spec_file),
            '--keystore',
            keystore,
            '--key-alias',
            'key',
            '--key-password',
            'password',
            '--min-sdk-version',
            str(min_sdk),
            '--v1-signing-enabled',
            str(v1).lower(),
            '--v2-signing-enabled',
            'true',
            '--v3-signing-enabled',
            str(v3).lower(),
            '--v4-signing-enabled',
            str(v4).lower(),
        ]

    tracing_args_list = [
        # Fast-sign path (v2 + v3 + v4, JKS keystore).
        make_tracing_args(
            'out_fast.apk', jks_keystore, 24, v1=False, v3=True, v4=True
        ),
        # Legacy v1 signing path (SignedApk + DefaultApkSignerEngine, PKCS12).
        make_tracing_args(
            'out_v1.apk', p12_keystore, 1, v1=True, v3=False, v4=False
        ),
    ]

    output_bin = os.path.join(os.path.dirname(dst_jar_path), 'signflinger')
    graalvm.build_native_image(
        dst_jar_path,
        output_bin,
        main_class=_MAIN_CLASS,
        tracing_args_list=tracing_args_list,
        # Enable SHA-NI and CLMUL intrinsics on top of x86-64-v3 (AVX2) so
        # SHA-256 signing uses hardware sha256rnds2 instructions. Callers fall
        # back to signflinger.jar on hosts lacking these CPU features.
        extra_native_image_args=[
            '-march=x86-64-v3',
            '-H:CPUFeatures=SHA,CLMUL,AES',
        ],
    )


maven.main(
    package='com.android:signflinger',
    post_process_func=_post_process,
    runtime_deps=[('//' + _JAVA_SRC, _JAVA_CHECKOUT_SUBPATH)],
    version_deps=['//' + _JAVA_SRC],
)

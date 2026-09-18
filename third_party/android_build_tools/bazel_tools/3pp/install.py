#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds Bazel's singlejar, ijar and zipper binaries for the bazel_tools 3pp.

Invoked by the 3pp recipe as: install.py <output_prefix> <deps_prefix>

Keep this file compatible with Python 3.6. On linux the 3pp recipe runs it as a
bare "python3" inside the manylinux docker image, which is considerably older
than the "vpython3" used on the mac-arm64 bot.
"""

import os
import pathlib
import shutil
import subprocess
import sys

# Bazel targets to build. Each label "//pkg:name" produces "bazel-bin/pkg/name".
#
# Note: Bazel's own java_tools release ships "singlejar_local" rather than
# "singlejar". The only behavioural difference is that "singlejar_local"
# implements --check_desugar_deps, whereas plain "singlejar" errors out on it.
# Chromium desugars with D8 and never passes that flag, so the smaller
# "singlejar" is used here.
_TARGETS = [
    '//src/tools/singlejar:singlejar',
    '//third_party/ijar:ijar',
    '//third_party/ijar:zipper',
]

# Devtoolset provides a newer libstdc++ than the one on the 3pp docker image.
# Linking "stdc++_nonshared" statically pulls in only the newer symbols so that
# the resulting binaries still run against the image's system libstdc++.
_DEVTOOLSET_LIB_DIR = (
    '/opt/rh/devtoolset-10/root/usr/lib/gcc/x86_64-redhat-linux/10')


def _bazel_bin_path(label):
    """Converts "//pkg/sub:name" into "bazel-bin/pkg/sub/name"."""
    assert label.startswith('//'), f'Not an absolute label: {label}'
    # Using a slice rather than str.removeprefix(), which needs Python 3.9.
    package, _, name = label[2:].partition(':')
    return pathlib.Path('bazel-bin', package, name)


def install(output_prefix, _deps_prefix):
    # Prevent subprocess output from being out-of-order when stdout is piped.
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', buffering=1)

    cmd = ['bazelisk', 'build', '-c', 'opt', '--strip=always']

    platform = os.environ.get('_3PP_PLATFORM', '')
    if platform.startswith('linux'):
        cmd += [
            f'--linkopt=-L{_DEVTOOLSET_LIB_DIR}',
            '--linkopt=-lstdc++_nonshared',
            f'--host_linkopt=-L{_DEVTOOLSET_LIB_DIR}',
            '--host_linkopt=-lstdc++_nonshared',
        ]
    elif platform.startswith('mac'):
        # Bazel 7.4.1 vendors zlib 1.3, whose zutil.h contains:
        #     #if defined(MACOS) || defined(TARGET_OS_MAC)
        #     ...
        #     #  ifndef fdopen
        #     #    define fdopen(fd,mode) NULL /* No fdopen() */
        # Modern macOS SDKs define TARGET_OS_MAC, so that fires and mangles
        # the SDK's own "FILE *fdopen(int, const char *)" declaration, failing
        # the compile of zutil.c. Defining fdopen to itself satisfies the
        # "#ifndef fdopen" guard so zlib skips the hack, and is inert
        # everywhere else: C11 6.10.3.4p2 forbids rescanning a macro that
        # expands to its own name, so the SDK declaration and every fdopen()
        # call site are left exactly as written.
        #
        # Keep this scoped to mac. The same zutil.h has two more fdopen
        # defines that are NOT "#ifndef"-guarded (the _BEOS_/RISCOS one, and
        # the MSVC "_fdopen" one). Neither is reachable on macOS, but applying
        # this flag globally would risk a macro redefinition against them.
        #
        # Upstream zlib deleted the block in 1.3.1 and Bazel picked that up in
        # 8.3 (8.1 and 8.2 still vendor zlib 1.3), but no 8.x is usable here
        # for unrelated reasons -- see 3pp.pb.
        cmd += [
            '--copt=-Dfdopen=fdopen',
            '--host_copt=-Dfdopen=fdopen',
        ]
    else:
        # platform_re admits only linux-amd64 and mac-arm64. Fail loudly
        # rather than silently building a linux binary without the devtoolset
        # link flags, which would work on the builder but not everywhere.
        raise ValueError(f'Unsupported _3PP_PLATFORM: {platform!r}')

    cmd += _TARGETS

    print('Environment variables:')
    for k, v in sorted(os.environ.items()):
        print(f'  {k}={v}')

    print(f'Running: {" ".join(cmd)}')
    try:
        subprocess.run(cmd, check=True)
    finally:
        subprocess.run(['bazelisk', 'shutdown'], check=False)
  
    output_prefix = pathlib.Path(output_prefix)
    output_prefix.mkdir(parents=True, exist_ok=True)
    for label in _TARGETS:
        shutil.copy(_bazel_bin_path(label), output_prefix)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(f'Usage: {sys.argv[0]} <output_prefix> <deps_prefix>')
    install(sys.argv[1], sys.argv[2])

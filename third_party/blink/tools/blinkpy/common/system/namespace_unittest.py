# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from blinkpy.common.system import namespace
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.common.system.platform_info_mock import MockPlatformInfo


class NamespaceUnitTest(unittest.TestCase):
    def test_is_supported_platforms(self):
        fs = MockFileSystem()
        self.assertTrue(
            namespace.is_supported(MockPlatformInfo(os_name='linux'), fs)
        )
        self.assertFalse(
            namespace.is_supported(MockPlatformInfo(os_name='mac'), fs)
        )
        self.assertFalse(
            namespace.is_supported(MockPlatformInfo(os_name='win'), fs)
        )

    def test_is_supported_checks_userns_sysctls(self):
        linux = MockPlatformInfo(os_name='linux')

        fs = MockFileSystem(
            {'/proc/sys/kernel/unprivileged_userns_clone': b'0\n'}
        )
        self.assertFalse(namespace.is_supported(linux, fs))

        fs = MockFileSystem({'/proc/sys/user/max_user_namespaces': b'0\n'})
        self.assertFalse(namespace.is_supported(linux, fs))

        fs = MockFileSystem(
            {
                '/proc/sys/kernel/unprivileged_userns_clone': b'1\n',
                '/proc/sys/user/max_user_namespaces': b'1024\n',
            }
        )
        self.assertTrue(namespace.is_supported(linux, fs))

    def test_enter_namespace_rejects_non_linux(self):
        with mock.patch.object(namespace, '_in_namespace', False):
            with mock.patch.object(sys, 'platform', 'darwin'):
                with self.assertRaisesRegex(
                    namespace.NamespaceError, 'only supported on Linux'
                ):
                    namespace.enter_namespace('/tmp/WebKit')

    @unittest.skipUnless(
        sys.platform.startswith('linux'), 'Linux-only namespace syscall test'
    )
    def test_enter_namespace_isolates_network_and_mount_in_subprocess(self):
        runtime_dir = tempfile.mkdtemp(prefix='blinkpy_ns_test_')
        self.addCleanup(shutil.rmtree, runtime_dir, ignore_errors=True)
        host_sentinel = os.path.join(runtime_dir, 'host_sentinel.txt')
        with open(host_sentinel, 'w', encoding='utf-8') as f:
            f.write('host')

        blink_tools_dir = os.path.abspath(
            os.path.join(os.path.dirname(__file__), '..', '..', '..')
        )

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as host_sock:
            host_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            host_sock.bind(('127.0.0.1', 0))
            host_sock.listen(1)
            port = host_sock.getsockname()[1]

            script = """
import errno, json, os, socket, sys
sys.path.insert(0, sys.argv[1])
from blinkpy.common.system import namespace

runtime_dir = sys.argv[2]
port = int(sys.argv[3])
try:
    namespace.enter_namespace(runtime_dir)
except namespace.NamespaceError as exc:
    if exc.errno in (errno.EPERM, errno.EACCES, errno.ENOSPC):
        print(json.dumps({"skipped": str(exc)}))
        sys.exit(0)
    raise

ns_file = os.path.join(runtime_dir, "ns_only.txt")
with open(ns_file, "w", encoding="utf-8") as f:
    f.write("isolated")

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s4:
    s4.bind(("127.0.0.1", port))
with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as s6:
    s6.bind(("::1", port))

caps = {}
with open("/proc/self/status", encoding="utf-8") as f:
    for line in f:
        if line.startswith(("CapInh:", "CapPrm:", "CapEff:", "CapAmb:")):
            k, v = line.split(":", 1)
            caps[k] = int(v.strip(), 16)

uid_in_ns = os.getuid()
gid_in_ns = os.getgid()

# Verify Chrome's nested user-namespace sandbox still works after capset.
libc = namespace._load_libc()
nested_rc = libc.unshare(namespace.CloneFlag.NEWUSER)

print(json.dumps({
    "uid": uid_in_ns,
    "gid": gid_in_ns,
    "runtime_files": os.listdir(runtime_dir),
    "caps": caps,
    "nested_userns_rc": nested_rc,
}))
"""
            proc = subprocess.run(
                [
                    sys.executable,
                    '-c',
                    script,
                    blink_tools_dir,
                    runtime_dir,
                    str(port),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(proc.returncode, 0, msg=proc.stderr)
            result = json.loads(proc.stdout.strip())
            if 'skipped' in result:
                self.skipTest(result['skipped'])

            self.assertEqual(result['uid'], os.getuid())
            self.assertEqual(result['gid'], os.getgid())
            self.assertEqual(result['runtime_files'], ['ns_only.txt'])
            self.assertEqual(
                result['caps'],
                {'CapInh': 0, 'CapPrm': 0, 'CapEff': 0, 'CapAmb': 0},
            )
            self.assertEqual(result['nested_userns_rc'], 0)
            # Host directory must still only contain host_sentinel.txt.
            self.assertEqual(os.listdir(runtime_dir), ['host_sentinel.txt'])

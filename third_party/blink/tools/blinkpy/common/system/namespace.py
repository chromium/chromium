# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""In-process Linux namespace isolation for simultaneous web test runs.

Isolates shared host resources (loopback ports and `/tmp/WebKit`) so separate
checkouts on the same machine can run tests concurrently. Does not isolate
checkout or build-directory files for simultaneous runs within one checkout.
"""

import ctypes
import ctypes.util
import enum
import errno
import os
import socket
import struct
import sys

if sys.platform != 'win32':
    import fcntl


class CloneFlag(enum.IntFlag):
    """Linux sched.h clone flags."""

    NEWNS = 0x00020000
    NEWUSER = 0x10000000
    NEWNET = 0x40000000


class MountFlag(enum.IntFlag):
    """Linux sys/mount.h flags."""

    NOSUID = 0x2
    NODEV = 0x4
    REC = 0x4000
    PRIVATE = 0x40000


# Linux sockios.h / net/if.h constants for bringing up loopback (`struct ifreq`
# is 40 bytes on 64-bit Linux and 32 bytes on 32-bit Linux).
SIOCGIFFLAGS = 0x8913
SIOCSIFFLAGS = 0x8914
IFF_UP = 0x1
_IFREQ_FORMAT = '16sH22s'

# Linux capability.h v3 header magic (`_LINUX_CAPABILITY_U32S_3 == 2`, so the
# payload is two 3-uint32 structs = 24 bytes).
_LINUX_CAPABILITY_VERSION_3 = 0x20080522
_CAP_DATA_BYTES = 24

_USERNS_SYSCTLS = (
    '/proc/sys/kernel/unprivileged_userns_clone',
    '/proc/sys/user/max_user_namespaces',
)

_in_namespace = False


class NamespaceError(OSError):
    """Raised when Linux namespace isolation cannot be initialized."""


def is_supported(platform, filesystem=None) -> bool:
    """Returns whether `--namespace` isolation is supported on `platform`."""
    if not platform.is_linux():
        return False
    if filesystem is not None:
        for sysctl_path in _USERNS_SYSCTLS:
            if not filesystem.exists(sysctl_path):
                continue
            try:
                if filesystem.read_text_file(sysctl_path).strip() == '0':
                    return False
            except OSError:
                pass
    return True


def _wrap_oserror(prefix: str, error: OSError) -> NamespaceError:
    if error.errno is None:
        return NamespaceError(f'{prefix}: {error}')
    detail = error.strerror or str(error)
    if error.filename:
        detail = f'{detail} ({error.filename})'
    return NamespaceError(error.errno, f'{prefix}: {detail}')


def _load_libc():
    libc_name = ctypes.util.find_library('c') or 'libc.so.6'
    try:
        libc = ctypes.CDLL(libc_name, use_errno=True)
    except OSError as error:
        raise _wrap_oserror(
            f'Failed to load libc ({libc_name})', error
        ) from error
    libc.unshare.argtypes = [ctypes.c_int]
    libc.unshare.restype = ctypes.c_int
    libc.mount.argtypes = [
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.c_ulong,
        ctypes.c_char_p,
    ]
    libc.mount.restype = ctypes.c_int
    libc.capset.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    libc.capset.restype = ctypes.c_int
    return libc


def _bring_up_loopback():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        ifreq = struct.pack(_IFREQ_FORMAT, b'lo', 0, b'')
        try:
            res = fcntl.ioctl(sock.fileno(), SIOCGIFFLAGS, ifreq)
            flags = struct.unpack(_IFREQ_FORMAT, res)[1]
            ifreq = struct.pack(_IFREQ_FORMAT, b'lo', flags | IFF_UP, b'')
            fcntl.ioctl(sock.fileno(), SIOCSIFFLAGS, ifreq)
        except OSError as error:
            raise _wrap_oserror(
                'Failed to bring up loopback interface in namespace', error
            ) from error


def _drop_capabilities(libc):
    header = ctypes.create_string_buffer(
        struct.pack('=Ii', _LINUX_CAPABILITY_VERSION_3, 0)
    )
    data = ctypes.create_string_buffer(_CAP_DATA_BYTES)
    if libc.capset(header, data) != 0:
        err = ctypes.get_errno()
        raise NamespaceError(
            err, f'Failed to drop capabilities via capset: {os.strerror(err)}'
        )


def enter_namespace(runtime_dir: str = '/tmp/WebKit') -> None:
    """Isolates the current process in user, mount, and network namespaces.

    Must be called while the process is still single-threaded. Maps the current
    UID/GID so Chromium's sandbox does not see UID 0, brings up loopback
    (`127.0.0.1` / `::1`), mounts a private `tmpfs` at `runtime_dir` while
    leaving `/tmp/.X*-lock` shared for Xvfb, and drops all capabilities so
    Chromium's sandbox initializes normally.
    """
    global _in_namespace
    if _in_namespace:
        return

    if not sys.platform.startswith('linux'):
        raise NamespaceError('--namespace is only supported on Linux.')

    try:
        os.makedirs(runtime_dir, exist_ok=True)
    except OSError as error:
        raise _wrap_oserror(
            f'Failed to create runtime directory {runtime_dir}', error
        ) from error
    try:
        os.chmod(runtime_dir, 0o1777)
    except OSError:
        pass

    uid = os.getuid()
    gid = os.getgid()
    libc = _load_libc()

    if (
        libc.unshare(CloneFlag.NEWUSER | CloneFlag.NEWNS | CloneFlag.NEWNET)
        != 0
    ):
        err = ctypes.get_errno()
        hint = ''
        if err in (errno.EPERM, errno.EACCES):
            hint = ' (ensure unprivileged user namespaces are enabled)'
        elif err == errno.ENOSPC:
            hint = (
                ' (namespace limit reached; check '
                '/proc/sys/user/max_*_namespaces)'
            )
        elif err == errno.EINVAL:
            hint = ' (process must be single-threaded when unsharing)'
        raise NamespaceError(err, f'unshare() failed{hint}: {os.strerror(err)}')

    try:
        try:
            with open('/proc/self/setgroups', 'wb') as f:
                f.write(b'deny\n')
        except FileNotFoundError:
            pass
        with open('/proc/self/uid_map', 'wb') as f:
            f.write(f'{uid} {uid} 1\n'.encode('ascii'))
        with open('/proc/self/gid_map', 'wb') as f:
            f.write(f'{gid} {gid} 1\n'.encode('ascii'))
    except OSError as error:
        raise _wrap_oserror(
            'Failed to write UID/GID mapping in user namespace', error
        ) from error

    _bring_up_loopback()

    if (
        libc.mount(None, b'/', None, MountFlag.REC | MountFlag.PRIVATE, None)
        != 0
    ):
        err = ctypes.get_errno()
        raise NamespaceError(
            err, f'Failed to mark mounts private: {os.strerror(err)}'
        )

    runtime_bytes = os.fsencode(runtime_dir)
    if (
        libc.mount(
            b'tmpfs',
            runtime_bytes,
            b'tmpfs',
            MountFlag.NOSUID | MountFlag.NODEV,
            b'mode=1777',
        )
        != 0
    ):
        err = ctypes.get_errno()
        raise NamespaceError(
            err,
            f'Failed to mount tmpfs on {runtime_dir}: {os.strerror(err)}',
        )

    _drop_capabilities(libc)
    _in_namespace = True

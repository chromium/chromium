# Copyright 2024 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Utilities for installing os-level packages.'''

import collections
import enum


class Library(collections.namedtuple('Library', ['name', 'pkg_map'])):

    def Install(self, configuration):
        flavor = configuration.os_flavor()
        install_cmd = ['sudo', *flavor.value, self.pkg_map[flavor]]
        return configuration.Call(install_cmd, stderr=None, stdout=None)


class OsFlavor(enum.Enum):
    Debian = ('apt-get', 'install')
    Arch = ('pacman', '-S')


Nasm = Library('nasm', {
    OsFlavor.Debian: 'nasm',
    OsFlavor.Arch: 'nasm',
})

GccAarch64LinuxGNU = Library(
    'gcc-aarch64-linux-gnu', {
        OsFlavor.Debian: 'gcc-aarch64-linux-gnu',
        OsFlavor.Arch: 'aarch64-linux-gnu-binutils',
    })

GccMultilib = Library('gcc-multilib', {
    OsFlavor.Debian: 'gcc-multilib',
    OsFlavor.Arch: 'gcc',
})

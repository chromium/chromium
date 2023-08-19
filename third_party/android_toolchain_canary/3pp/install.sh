#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# THIS MUST BE KEPT IN SYNC WITH ../../android_toolchain/3pp/install.sh.

set -e
set -x
set -o pipefail

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# The commands below should output the built product to this directory.
PREFIX="$1"

# The simpleperf binaries are required for some tracing utilities.
mv simpleperf "$PREFIX"
# The sysroot is required for building each platform. Retain the path used by
# the NDK for ease of use by build files that expect NDK-shaped directories.
mkdir -p "$PREFIX/toolchains/llvm/prebuilt/linux-x86_64/"
mv toolchains/llvm/prebuilt/linux-x86_64/sysroot \
  "$PREFIX/toolchains/llvm/prebuilt/linux-x86_64/"
# Remove files that have identical names on case-insensitive file systems.
FILES_TO_REMOVE=(
  "sysroot/usr/include/linux/netfilter_ipv4/ipt_ECN.h"
  "sysroot/usr/include/linux/netfilter_ipv4/ipt_TTL.h"
  "sysroot/usr/include/linux/netfilter_ipv6/ip6t_HL.h"
  "sysroot/usr/include/linux/netfilter/xt_CONNMARK.h"
  "sysroot/usr/include/linux/netfilter/xt_DSCP.h"
  "sysroot/usr/include/linux/netfilter/xt_MARK.h"
  "sysroot/usr/include/linux/netfilter/xt_RATEEST.h"
  "sysroot/usr/include/linux/netfilter/xt_TCPMSS.h"
)
for file in "${FILES_TO_REMOVE[@]}"; do
  rm "$PREFIX/toolchains/llvm/prebuilt/linux-x86_64/${file}"
done

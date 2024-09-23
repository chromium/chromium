#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# The commands below should output the built product to this directory.
PREFIX="$1"

# Glob patterns to include from the NDK.
GLOB_INCLUDES=(
  # Used for tracing utilities, see //build/android/pylib/utils/simpleperf.py.
  simpleperf
  # Used for remote debugging, include server / client binaries and libs.
  toolchains/llvm/prebuilt/linux-x86_64/bin/lldb
  toolchains/llvm/prebuilt/linux-x86_64/bin/lldb.sh
  toolchains/llvm/prebuilt/linux-x86_64/lib/clang/*/lib/linux/*/lldb-server
  toolchains/llvm/prebuilt/linux-x86_64/lib/lib*.so*
  toolchains/llvm/prebuilt/linux-x86_64/lib/python*
  toolchains/llvm/prebuilt/linux-x86_64/python3
  # Used for compilation.
  toolchains/llvm/prebuilt/linux-x86_64/sysroot
)

# Glob patterns to exclude from the final output.
GLOB_EXCLUDES=(
  toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/linux/netfilter_ipv4/ipt_ECN.h
  toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/linux/netfilter_ipv4/ipt_TTL.h
  toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/linux/netfilter_ipv6/ip6t_HL.h
  toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/linux/netfilter/xt_CONNMARK.h
  toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/linux/netfilter/xt_DSCP.h
  toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/linux/netfilter/xt_MARK.h
  toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/linux/netfilter/xt_RATEEST.h
  toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/linux/netfilter/xt_TCPMSS.h
)

# Fix an compile error in the Android NDK (crbug.com/352592408).
sed -i 's/1UL << 32/1ULL << 32/g' toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/android/hardware_buffer.h

# Move included files from the working directory to the staging directory.
for pattern in "${GLOB_INCLUDES[@]}"; do
  cp --parents -r $pattern "$PREFIX"
done

# Remove excluded files from the staging directory.
for pattern in "${GLOB_EXCLUDES[@]}"; do
  rm -rf "${PREFIX}/${pattern}"
done


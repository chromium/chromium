#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# THIS MUST BE KEPT IN SYNC WITH ../../android_toolchain_canary/3pp/install.sh.

set -e
set -x
set -o pipefail

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# The commands below should output the built product to this directory.
PREFIX="$1"
CLANG_VERSION="14.0.7"

# Glob patterns to include from the NDK.
GLOB_INCLUDES=(
  # Used for tracing utilities, see //build/android/pylib/utils/simpleperf.py.
  simpleperf
  # Used for remote debugging, include both client and server binaries.
  toolchains/llvm/prebuilt/linux-x86_64/bin/lldb
  toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/${CLANG_VERSION}/lib/linux/*/lldb-server
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

# Move included files from the working directory to the staging directory.
for pattern in "${GLOB_INCLUDES[@]}"; do
  cp --parents -r $pattern "$PREFIX"
done

# Remove excluded files from the staging directory.
for pattern in "${GLOB_EXCLUDES[@]}"; do
  rm -rf $pattern
done

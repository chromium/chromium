#!/bin/bash
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Usage: nacl_interp_loader.sh PLATFORM NEXE ARGS...

case "$1" in
i?86)
  arch=x86_32
  libdir=lib32
  ;;
x86_64)
  arch=x86_64
  libdir=lib64
  ;;
arm|v7l)
  arch=arm
  libdir=lib32
  ;;
*)
  echo >&2 "$0: Do not recognize architecture \"$1\""
  exit 127
  ;;
esac

shift

case "${NACL_SDK_ROOT}" in
*pepper_15* | *pepper_16* | *pepper_17*)
  SEL_LDR="$NACL_SDK_ROOT/toolchain/linux_x86/bin/sel_ldr_${arch}"
  IRT="$NACL_SDK_ROOT/toolchain/linux_x86/runtime/irt_core_${arch}.nexe"
  RTLD="$NACL_SDK_ROOT/toolchain/linux_x86/x86_64-nacl/${libdir}/runnable-ld.so"
  LIBDIR="$NACL_SDK_ROOT/toolchain/linux_x86/x86_64-nacl/${libdir}"
  ;;
*)
  SEL_LDR="$NACL_SDK_ROOT/tools/sel_ldr_${arch}"
  IRT="$NACL_SDK_ROOT/tools/irt_core_${arch}.nexe"
  RTLD="$NACL_SDK_ROOT/toolchain/linux_x86_glibc/x86_64-nacl/${libdir}/runnable-ld.so"
  LIBDIR="$NACL_SDK_ROOT/toolchain/linux_x86_glibc/x86_64-nacl/${libdir}"
  ;;
esac

IGNORE_VALIDATOR_ARG=""
if [ x"$NACL_IGNORE_VALIDATOR" == x"1" ]; then
  IGNORE_VALIDATOR_ARG="-c"
fi

exec "$SEL_LDR" -E "NACL_PWD=`pwd`" -E "MONO_PATH=$MONO_PATH" \
  -E "MONO_CFG_DIR=$MONO_CFG_DIR" -E "MONO_SHARED_DIR=$MONO_SHARED_DIR" \
  -a $IGNORE_VALIDATOR_ARG -S -B "$IRT" -l /dev/null -- "$RTLD" \
  --library-path $LIBDIR "$@"

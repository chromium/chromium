#!/bin/bash
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

if [[ $# -ne 3 ]]; then
  echo "Usage: $0 [path_to_mono] [build_dir] [install_dir]"
  exit -1
fi

readonly CLEAN=1
readonly DEBUG=0
readonly PARALLEL=0
readonly MONO_DIR=$(readlink -f $1)
readonly BUILD_DIR=$(readlink -f $2)
readonly INSTALL_DIR=$(readlink -f $3)
readonly ORIGINAL_CWD=$(pwd)

set +e
if [ -f ${BUILD_DIR}/Makefile -a ${CLEAN} != 0 ]; then
  cd ${BUILD_DIR}
  make distclean
fi
set -e
cd $ORIGINAL_CWD

case "${TARGET_ARCH}" in
x86-32)
  readonly USE_PNACL=0
  readonly USE_NEWLIB=0
  readonly TC_FLAVOUR=linux_x86_glibc
  readonly NACL_CROSS_PREFIX_DASH=i686-nacl-
  CONFIG_OPTS="--host=i686-pc-linux-gnu \
               --build=i686-pc-linux-gnu \
               --target=i686-pc-linux-gnu"
  readonly LIBDIR=lib32
  CUSTOM_CFLAGS=""
  readonly CUSTOM_LDFLAGS=""
  ;;
x86-64)
  readonly USE_PNACL=0
  readonly USE_NEWLIB=0
  readonly TC_FLAVOUR=linux_x86_glibc
  readonly NACL_CROSS_PREFIX_DASH=x86_64-nacl-
  CONFIG_OPTS="--host=x86_64-pc-linux-gnu \
               --build=x86_64-pc-linux-gnu \
               --target=x86_64-pc-linux-gnu"
  readonly LIBDIR=lib
  CUSTOM_CFLAGS=""
  readonly CUSTOM_LDFLAGS=""
  ;;
arm)
  readonly USE_PNACL=0
  readonly USE_NEWLIB=1
  readonly TC_FLAVOUR=linux_arm_newlib
  readonly NACL_CROSS_PREFIX_DASH=arm-nacl-
  CONFIG_OPTS="--host=armv7l-unknown-linux-gnueabi \
               --build=x86_64-pc-linux-gnu \
               --target=armv7l-unknown-linux-gnueabi \
               --with-jumptables=yes"
  readonly LIBDIR=libarm
  # TODO(olonho): move it to Mono's configure, once nacl target implemented.
  CUSTOM_CFLAGS="\
-DARM_FPU_VFP=1 \
-D__ARM_ARCH_7__ \
-Dtimezone=_timezone \
-DDISABLE_SOCKETS \
-DDISABLE_ATTACH \
"
  readonly CUSTOM_LDFLAGS=""
  ;;
arm-pnacl)
  readonly USE_PNACL=1
  readonly USE_NEWLIB=1
  readonly TC_FLAVOUR=linux_pnacl
  readonly NACL_CROSS_PREFIX_DASH=pnacl-
  readonly PNACL_LINK_ARGS="-arch armv7 -O2"
  CONFIG_OPTS="--host=armv7l-unknown-linux-gnueabi \
                        --build=x86_64-pc-linux-gnu \
                        --target=armv7l-unknown-linux-gnueabi \
                        --with-jumptables=yes"
  readonly LIBDIR=libarm
  # TODO(olonho): move it to Mono's configure, once nacl target implemented.
  CUSTOM_CFLAGS="\
-D__arm__ \
-D__ARM_ARCH_7__ \
-D__portable_native_client__ \
-DARM_FPU_VFP=1 \
-Dtimezone=_timezone \
-DDISABLE_SOCKETS \
-DDISABLE_ATTACH \
"
  readonly CUSTOM_LDFLAGS=${PNACL_LINK_ARGS}
  ;;
*)
  echo "Unsupported target ${TARGET_ARCH}"
  exit 1
esac

if [ ${USE_NEWLIB} == 1 ]; then
  CUSTOM_CFLAGS="${CUSTOM_CFLAGS} -DUSE_NEWLIB"
  CONFIG_OPTS="${CONFIG_OPTS} --enable-shared=no"
else
  CONFIG_OPTS="${CONFIG_OPTS} --enable-shared=yes"
  # UGLY hack to allow dynamic linking
  sed -i -e s/elf_i386/elf_nacl/ -e s/elf_x86_64/elf64_nacl/ \
    ${MONO_DIR}/configure
  sed -i -e s/elf_i386/elf_nacl/ -e s/elf_x86_64/elf64_nacl/ \
    ${MONO_DIR}/libgc/configure
  sed -i -e s/elf_i386/elf_nacl/ -e s/elf_x86_64/elf64_nacl/ \
    ${MONO_DIR}/eglib/configure
fi

if [ ${CLEAN} != 0 ]; then
  rm -rf ${BUILD_DIR}
  mkdir -p ${BUILD_DIR}
fi
cd ${BUILD_DIR}

mkdir -p ${INSTALL_DIR}

readonly NACL_BIN_PATH=${NACL_SDK_ROOT}/toolchain/${TC_FLAVOUR}/bin

if [ ${USE_PNACL} == 1 ]; then
  readonly NACLCC=${NACL_BIN_PATH}/pnacl-clang
  readonly NACLCXX=${NACL_BIN_PATH}/pnacl-clang++
else
  readonly NACLCC=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}gcc
  readonly NACLCXX=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}g++
fi
readonly NACLAR=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}ar
readonly NACLRANLIB=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}ranlib
readonly NACLLD=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}ld
readonly NACLOBJDUMP=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}objdump
readonly NACLSTRIP=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}strip

if [ ${DEBUG} == 1 ]; then
  CFLAGS="$CUSTOM_CFLAGS"
  CONFIG_OPTS="${CONFIG_OPTS} --enable-debug=yes"
else
  CFLAGS="-g $CUSTOM_CFLAGS"
  CONFIG_OPTS="${CONFIG_OPTS} --enable-debug=no"
fi

if [ ${PARALLEL} == 1 ]; then
  readonly JOBS="-j16"
else
  readonly JOBS=
fi


LDFLAGS="$CUSTOM_LDFLAGS"
LIBS="-lnacl_dyncode -lc -lg -lnosys -lpthread"

CC=${NACLCC} CXX=${NACLCXX} LD=${NACLLD} \
STRIP=${NACLSTRIP} AR=${NACLAR} RANLIB=${NACLRANLIB} OBJDUMP=${NACLOBJDUMP} \
PKG_CONFIG_LIBDIR= \
PATH=${NACL_BIN_PATH}:${PATH} \
LIBS="${LIBS}" \
CFLAGS="${CFLAGS}" \
LDFLAGS="${LDFLAGS}" \
${MONO_DIR}/configure ${CONFIG_OPTS} \
  --exec-prefix=${INSTALL_DIR} \
  --libdir=${INSTALL_DIR}/${LIBDIR} \
  --prefix=${INSTALL_DIR} \
  --program-prefix=${NACL_CROSS_PREFIX_DASH} \
  --oldincludedir=${INSTALL_DIR}/include \
  --with-glib=embedded \
  --with-tls=pthread \
  --enable-threads=posix \
  --without-sigaltstack \
  --without-mmap \
  --with-gc=included \
  --enable-nacl-gc \
  --with-sgen=no \
  --enable-nls=no \
  --enable-nacl-codegen \
  --disable-system-aot \
  --disable-parallel-mark \
  --with-static-mono=no

if [ ${USE_NEWLIB} == 1 ]; then
  # Newlib build doesn't support building shared libs, and unfortunately, this
  # leads to libmonoruntime.la no being built as well, unless we'll do that
  # explicitly.
  make ${JOBS} -C mono/metadata libmonoruntime.la
fi
make ${JOBS}
make ${JOBS} install

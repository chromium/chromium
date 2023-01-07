#!/bin/bash
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

SCRIPT_DIR="$(cd $(dirname $0) && pwd)"
cd ${SCRIPT_DIR}

OUT_DIR=out
NACLPORTS_URL=https://chromium.googlesource.com/external/naclports.git
NACLPORTS_REV=e53078c33d99b0b3cbadbbbbb92cccf7a48d5dc1
NACLPORTS_DIR=${OUT_DIR}/naclports

if [ -z "${NACL_SDK_ROOT:-}" ]; then
  echo "-------------------------------------------------------------------"
  echo "NACL_SDK_ROOT is unset."
  echo "This environment variable needs to be pointed at some version of"
  echo "the Native Client SDK (the directory containing toolchain/)."
  echo "NOTE: set this to an absolute path."
  echo "-------------------------------------------------------------------"
  exit -1
fi

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}

# echo a command to stdout and then execute it.
LogExecute() {
  echo $*
  $*
}

Banner Cloning naclports
if [ -d ${NACLPORTS_DIR} -a ! -d ${NACLPORTS_DIR}/src/.git ]; then
  rm -rf ${NACLPORTS_DIR}
fi

if [ ! -d ${NACLPORTS_DIR} ]; then
  mkdir -p ${NACLPORTS_DIR}
  pushd ${NACLPORTS_DIR}
  gclient config --name=src ${NACLPORTS_URL}
  popd
fi

pushd ${NACLPORTS_DIR}
gclient sync -r ${NACLPORTS_REV}
popd


Banner Building lua
pushd ${NACLPORTS_DIR}/src
# Do a 'clean' first, since previous lua build from the naclports bundle
# building might be installed in the toolchain, and that one is built
# without readline support.
make TOOLCHAIN=pnacl clean
make TOOLCHAIN=pnacl lua-ppapi
popd

Banner Done!

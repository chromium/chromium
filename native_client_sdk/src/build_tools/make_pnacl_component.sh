#!/bin/bash
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script builds out/pnacl_multicrx_<rev>.zip for upload to the Chrome
# Web Store. It runs GN + ninja once for each architecture and assembles
# the results along with a manifest file.

# TODO(sbc): rewrite this in python

set -o errexit
set -o nounset

SCRIPT_DIR="$(cd $(dirname $0) && pwd)"
CHROME_SRC=$(dirname $(dirname $(dirname ${SCRIPT_DIR})))
cd ${CHROME_SRC}

run_gn() {
  local arch=$1
  gn gen out_pnacl/$arch --args="target_cpu=\"$arch\" is_debug=false"
}

cpu_package() {
  local arch=$1
  local alt_arch=$2
  local base_out_dir=out

  rm -rf out_pnacl/$arch
  run_gn $arch
  ninja -C out_pnacl/$arch pnacl_support_extension
  local target_dir=${base_out_dir}/pnacl_${alt_arch}
  mkdir -p ${target_dir}
  cp out_pnacl/$arch/pnacl/* ${target_dir}/.
}

individual_packages() {
  cpu_package x64 x86_64
  cpu_package arm arm
  cpu_package x86 x86_32
}

multi_crx() {
  local outfile=$1
  local version=$2
  local base_out_dir=out
  local target_dir=${base_out_dir}/pnacl_multicrx
  mkdir -p ${target_dir}
  cat > ${target_dir}/manifest.json <<EOF
{
  "description": "Portable Native Client Translator Multi-CRX",
  "name": "PNaCl Translator Multi-CRX",
  "manifest_version": 2,
  "minimum_chrome_version": "30.0.0.0",
  "version": "${version}",
  "platforms": [
    {
      "nacl_arch": "x86-32",
      "sub_package_path": "_platform_specific/x86_32/"
    },
    {
      "nacl_arch": "x86-64",
      "sub_package_path": "_platform_specific/x86_64/"
    },
    {
      "nacl_arch": "arm",
      "sub_package_path": "_platform_specific/arm/"
    }
  ]
}
EOF

  for arch in x86_32 x86_64 arm; do
    local sub_dir="${target_dir}/_platform_specific/${arch}"
    local src_dir="${base_out_dir}/pnacl_${arch}"
    mkdir -p ${sub_dir}
    cp ${src_dir}/pnacl_public_* ${sub_dir}/.
  done
  (cd ${target_dir} && zip -r ../${outfile} . && ls -l ../${outfile})
  echo "DONE: created ${outfile} -- upload that!"
  echo "You can also delete ${target_dir} later (the pre-zipped contents)."
}

if [ $# != 2 ]; then
  echo "Usage: $0 <outfile> <rev_number>"
  exit 1
fi

outfile="$1"
version="$2"
echo "Building file ${outfile} version=${version}"
individual_packages
multi_crx ${outfile} ${version}

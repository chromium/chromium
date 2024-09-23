#!/usr/bin/env bash
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
SCRIPT_DIR="$(dirname "$0")"

PLUGINS_DIR="${HOME}/Library/Developer/Xcode/Plug-ins"
SPECS_DIR="${HOME}/Library/Developer/Xcode/Specifications"

if [ ! -d "${PLUGINS_DIR}" ]; then
    echo "Creating plug-ins directory at ${PLUGINS_DIR}."
    mkdir -p "${PLUGINS_DIR}"
fi

if [ ! -d "${SPECS_DIR}" ]; then
    echo "Creating specifications directory at ${SPECS_DIR}."
    mkdir -p "${SPECS_DIR}"
fi


echo "Installing plug-in at ${PLUGINS_DIR}"
echo "Installing language specification at ${SPECS_DIR}"

cp -r "${SCRIPT_DIR}/GN.ideplugin" "${PLUGINS_DIR}"
cp "${SCRIPT_DIR}/gn.xclangspec" "${SPECS_DIR}"

echo "Done"
#!/bin/sh

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ME2ME_HOST="/Library/PrivilegedHelperTools/ChromeRemoteDesktopHost.bundle"
UNINSTALLER_CHROME="/Applications/Chrome Remote Desktop Host Uninstaller.app"
UNINSTALLER_CHROMIUM="/Applications/Chromoting Host Uninstaller.app"
KEYSTONE="/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle"

INFO_PLIST="Contents/Info.plist"

set -e -u

function print_plist_version {
  local name="${1}"
  local file="${2}"
  if [[ -e "${file}/${INFO_PLIST}" ]]; then
    set `PlistBuddy -c 'Print CFBundleVersion' "${file}/${INFO_PLIST}"`
    echo "${name}: version = ${1}"
  else
    echo "${name}: plist doesn't exist"
  fi
}

print_plist_version "Me2me host" "${ME2ME_HOST}"
print_plist_version "Chrome Remote Desktop Host Uninstaller" "${UNINSTALLER_CHROME}"
print_plist_version "Chromoting Host Uninstaller" "${UNINSTALLER_CHROMIUM}"
print_plist_version "Keystone" "${KEYSTONE}"

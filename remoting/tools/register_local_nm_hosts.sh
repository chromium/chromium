#!/bin/sh
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Script that can be used to register native messaging hosts in the GN output
# directory.

set -e

# 'readlink' works differently on macOS, so 'pwd -P' is used instead to
# resolve symlinks.
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd -P)"
ME2ME_HOST_NAME="com.google.chrome.remote_desktop"
IT2ME_HOST_NAME="com.google.chrome.remote_assistance"

install_manifest() {
  local manifest_template="$1"
  local host_path="$2"
  local host_path_var_name="$3"
  local target_dir="$4"

  local template_name="$(basename ${manifest_template})"
  local manifest_name="${template_name%.*}"
  local target_manifest="${target_dir}/${manifest_name}"

  echo Registering ${host_path} in ${target_manifest}
  mkdir -p "${target_dir}"

  "$SRC_DIR/remoting/tools/build/remoting_localize.py" \
    --define "${host_path_var_name}=${host_path}" \
    --define IT2ME_HOST_DESCRIPTION=dev \
    --define ME2ME_HOST_DESCRIPTION=dev \
    --template "${manifest_template}" \
    --output "${target_manifest}" \
    en
}

register_hosts() {
  local build_dir="$1"
  local chrome_data_dir="$2"

  local nm_host="remoting_native_messaging_host"
  local ra_host="remote_assistance_host"
  if [[ $(uname -s) == "Darwin" ]]; then
    nm_host="native_messaging_host.app/Contents/MacOS/native_messaging_host"
    ra_host="remote_assistance_host.app/Contents/MacOS/remote_assistance_host"
  fi

  install_manifest \
     "${SRC_DIR}/remoting/host/setup/${ME2ME_HOST_NAME}.json.jinja2" \
     "${build_dir}/${nm_host}" \
     ME2ME_HOST_PATH "${chrome_data_dir}"

  install_manifest \
     "${SRC_DIR}/remoting/host/it2me/${IT2ME_HOST_NAME}.json.jinja2" \
     "${build_dir}/${ra_host}" \
     IT2ME_HOST_PATH "${chrome_data_dir}"
}

register_hosts_for_all_channels() {
  local build_dir="$1"

  if [ -n "$CHROME_USER_DATA_DIR" ]; then
    register_hosts "${build_dir}" \
        "${CHROME_USER_DATA_DIR}/NativeMessagingHosts"
  elif [ $(uname -s) == "Darwin" ]; then
    register_hosts "${build_dir}" \
        "${HOME}/Library/Application Support/Google/Chrome/NativeMessagingHosts"
    register_hosts "${build_dir}" \
        "${HOME}/Library/Application Support/Chromium/NativeMessagingHosts"
  else
    register_hosts "${build_dir}" \
        "${HOME}/.config/google-chrome/NativeMessagingHosts"
    register_hosts "${build_dir}" \
        "${HOME}/.config/google-chrome-beta/NativeMessagingHosts"
    register_hosts "${build_dir}" \
        "${HOME}/.config/google-chrome-unstable/NativeMessagingHosts"
    register_hosts "${build_dir}" \
        "${HOME}/.config/chromium/NativeMessagingHosts"
  fi
}

unregister_hosts() {
  local chrome_data_dir="$1"

  rm -f "${chrome_data_dir}/${ME2ME_HOST_NAME}.json"
  rm -f "${chrome_data_dir}/${IT2ME_HOST_NAME}.json"
}

unregister_hosts_for_all_channels() {
  if [ -n "$CHROME_USER_DATA_DIR" ]; then
    unregister_hosts \
        "${CHROME_USER_DATA_DIR}/NativeMessagingHosts"
  elif [ $(uname -s) == "Darwin" ]; then
    unregister_hosts \
        "${HOME}/Library/Application Support/Google/Chrome/NativeMessagingHosts"
    unregister_hosts \
        "${HOME}/Library/Application Support/Chromium/NativeMessagingHosts"
  else
    unregister_hosts "${HOME}/.config/google-chrome/NativeMessagingHosts"
    unregister_hosts "${HOME}/.config/google-chrome-beta/NativeMessagingHosts"
    unregister_hosts \
        "${HOME}/.config/google-chrome-unstable/NativeMessagingHosts"
    unregister_hosts "${HOME}/.config/chromium/NativeMessagingHosts"
  fi
}

print_usage() {
  echo "Usage: $0 <GN-output-dir>|-u" >&2
  echo "   -u   Unregister" >&2
}

if [[ $# -ne 1 ]]; then
  print_usage
  exit 1
fi

if [[ "$1" == "-u" ]]; then
  unregister_hosts_for_all_channels
  exit 0
fi

if [[ ! -f "$1/args.gn" ]]; then
  echo "$1 is not a GN output directory."
  print_usage
  exit 1
fi

# The manifests need an absolute path.
gn_out_dir="$(cd "$1" && pwd -P)"
register_hosts_for_all_channels "$gn_out_dir"

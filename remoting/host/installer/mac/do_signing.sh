#!/bin/bash

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script signs the Chromoting binaries, builds the Chrome Remote Desktop
# installer and then packages it into a .dmg.  It requires that Packages be
# installed (for 'packagesbuild').
# Packages: http://s.sudre.free.fr/Software/Packages/about.html
# Optionally, it will submit the .dmg to Apple for notarization.
# Run with "-h" to see usage information.

set -e -u

ME="$(basename "${0}")"
readonly ME

declare -a g_cleanup_dirs

setup() {
  local input_dir="${1}"

  # The file that contains the properties for this signing build.
  # The file should contain only key=value pairs, one per line.
  PROPS_FILENAME="${input_dir}/do_signing.props"

  # Individually load the properties for this build. Don't 'source' the file
  # to guard against code accidentally being added to the props file.
  DMG_VOLUME_NAME=$(read_property "DMG_VOLUME_NAME")
  DMG_FILE_NAME=$(read_property "DMG_FILE_NAME")
  HOST_BUNDLE_NAME=$(read_property "HOST_BUNDLE_NAME")
  HOST_PKG=$(read_property "HOST_PKG")
  HOST_UNINSTALLER_NAME=$(read_property "HOST_UNINSTALLER_NAME")
  NATIVE_MESSAGING_HOST_BUNDLE_NAME=$(read_property\
    "NATIVE_MESSAGING_HOST_BUNDLE_NAME")
  REMOTE_ASSISTANCE_HOST_BUNDLE_NAME=$(read_property\
    "REMOTE_ASSISTANCE_HOST_BUNDLE_NAME")

  # Binaries/bundles to sign.
  ME2ME_HOST="PrivilegedHelperTools/${HOST_BUNDLE_NAME}"
  ME2ME_EXE_DIR="${ME2ME_HOST}/Contents/MacOS/"
  ME2ME_AGENT_PROCESS_BROKER="${ME2ME_EXE_DIR}/remoting_agent_process_broker"
  ME2ME_LAUNCHD_SERVICE="${ME2ME_EXE_DIR}/remoting_me2me_host_service"
  ME2ME_NM_HOST="${ME2ME_EXE_DIR}/${NATIVE_MESSAGING_HOST_BUNDLE_NAME}/"
  IT2ME_NM_HOST="${ME2ME_EXE_DIR}/${REMOTE_ASSISTANCE_HOST_BUNDLE_NAME}/"
  UNINSTALLER="Applications/${HOST_UNINSTALLER_NAME}.app"

  # The Chromoting Host installer is a meta-package that consists of 3
  # components:
  #  * Chromoting Host Service package
  #  * Chromoting Host Uninstaller package
  #  * Keystone package (GoogleSoftwareUpdate - for Official builds only)
  PKGPROJ_HOST="ChromotingHost.pkgproj"
  PKGPROJ_HOST_SERVICE="ChromotingHostService.pkgproj"
  PKGPROJ_HOST_UNINSTALLER="ChromotingHostUninstaller.pkgproj"

  # Bundle-specific entitlements which include restricted entitlements. These
  # can only be used for signing specific bundles with official credentials.
  ME2ME_ENTITLEMENTS="me2me-entitlements.plist"

  # The default entitlements file. This contains unrestricted entitlements that
  # can safely be applied for local signing.
  DEFAULT_ENTITLEMENTS="app-entitlements.plist"

  # Final (user-visible) pkg name.
  PKG_FINAL="${HOST_PKG}.pkg"

  DMG_FILE_NAME="${DMG_FILE_NAME}.dmg"

  # Temp directory for Packages output.
  PKG_DIR=build
  g_cleanup_dirs+=("${PKG_DIR}")

  # Temp directories for building the dmg.
  DMG_TEMP_DIR="$(mktemp -d -t "${ME}"-dmg)"
  g_cleanup_dirs+=("${DMG_TEMP_DIR}")

  DMG_EMPTY_DIR="$(mktemp -d -t "${ME}"-empty)"
  g_cleanup_dirs+=("${DMG_EMPTY_DIR}")
}

err() {
  echo "[$(date +'%Y-%m-%d %H:%M:%S%z')]: ${@}" >&2
}

err_exit() {
  err "${@}"
  exit 1
}

# shell_safe_path ensures that |path| is safe to pass to tools as a
# command-line argument. If the first character in |path| is "-", "./" is
# prepended to it. The possibly-modified |path| is output.
shell_safe_path() {
  local path="${1}"
  if [[ "${path:0:1}" = "-" ]]; then
    echo "./${path}"
  else
    echo "${path}"
  fi
}

# Read a single property from the properties file.
read_property() {
  local property="${1}"
  local filename="${PROPS_FILENAME}"
  echo `grep "\<${property}\>=" "${filename}" | tail -n 1 | cut -d "=" -f2-`
}

verify_clean_dir() {
  local dir="${1}"
  if [[ ! -d "${dir}" ]]; then
    mkdir "${dir}"
  fi

  if [[ -e "${output_dir}/${DMG_FILE_NAME}" ]]; then
    err "Output directory is dirty from previous build."
    exit 1
  fi
}

sign() {
  local name="${1}"
  local keychain="${2}"
  local id="${3}"
  local entitlements="${4:-}"

  if [[ ! -e "${name}" ]]; then
    err_exit "Input file doesn't exist: ${name}"
  fi

  echo Signing "${name}"

  # It may be more natural to define a separate array just for the keychain
  # args, but there is a bug in older versions of Bash:
  # Expanding a zero-size array with "set -u" aborts with "unbound variable".
  local args=(-vv --sign "${id}")
  if [[ -n "${keychain}" ]]; then
    args+=(--keychain "${keychain}")
  fi
  if [[ -n "${entitlements}" ]]; then
    args+=(--entitlements "${input_dir}/${entitlements}")
  fi
  args+=(--timestamp --options runtime "${name}")
  codesign "${args[@]}"
  codesign -v "${name}"
}

sign_binaries() {
  local input_dir="${1}"
  local keychain="${2}"
  local id="${3}"

  local binaries=(\
    "${ME2ME_AGENT_PROCESS_BROKER}" \
    "${ME2ME_LAUNCHD_SERVICE}" \
    "${ME2ME_NM_HOST}" \
    "${IT2ME_NM_HOST}" \
    "${ME2ME_HOST}" \
    "${UNINSTALLER}" \
  )
  for binary in "${binaries[@]}"; do
    local entitlements="${DEFAULT_ENTITLEMENTS}"

    # Restricted entitlements must only be claimed for builds signed with
    # official credentials. Locally-signed development packages do not use any
    # productsign_id.
    if [[ -n "${productsign_id}" && "${binary}" == "${ME2ME_HOST}" ]]; then
      entitlements="${ME2ME_ENTITLEMENTS}"
    fi
    sign "${input_dir}/${binary}" "${keychain}" "${id}" "${entitlements}"
  done
}

sign_installer() {
  local input_dir="${1}"
  local keychain="${2}"
  local id="${3}"

  local package="${input_dir}/${PKG_DIR}/${PKG_FINAL}"
  local args=(--sign "${id}" --timestamp)
  if [[ -n "${keychain}" ]]; then
      args+=(--keychain "${keychain}")
  fi
  args+=("${package}" "${package}.signed")
  productsign "${args[@]}"
  mv -f "${package}.signed" "${package}"
}

build_package() {
  local pkg="${1}"
  echo "Building .pkg from ${pkg}"
  packagesbuild -v "${pkg}"
}

build_packages() {
  local input_dir="${1}"
  build_package "${input_dir}/${PKGPROJ_HOST_SERVICE}"
  build_package "${input_dir}/${PKGPROJ_HOST_UNINSTALLER}"
  build_package "${input_dir}/${PKGPROJ_HOST}"
}

build_dmg() {
  local input_dir="${1}"
  local output_dir="${2}"

  # Create the .dmg.
  echo "Building .dmg..."
  "${input_dir}/pkg-dmg" \
      --format UDBZ \
      --tempdir "${DMG_TEMP_DIR}" \
      --source "${DMG_EMPTY_DIR}" \
      --target "${output_dir}/${DMG_FILE_NAME}" \
      --volname "${DMG_VOLUME_NAME}" \
      --copy "${input_dir}/${PKG_DIR}/${PKG_FINAL}" \
      --copy "${input_dir}/Scripts/keystone_install.sh:/.keystone_install"

  if [[ ! -f "${output_dir}/${DMG_FILE_NAME}" ]]; then
    err_exit "Unable to create disk image: ${DMG_FILE_NAME}"
  fi
}

notarize() {
  local input_dir="${1}"
  local dmg="${2}"
  local user="${3}"

  echo "Notarizing and stapling .dmg..."
  if [[ -n "${NOTARIZATION_TOOL-}" ]]; then
    "${NOTARIZATION_TOOL}" --file "${dmg}"
  else
    err_exit "A \$NOTARIZATION_TOOL must be specified."
  fi
}

cleanup() {
  if [[ "${#g_cleanup_dirs[@]}" > 0 ]]; then
    rm -rf "${g_cleanup_dirs[@]}"
  fi
}

usage() {
  echo "Usage: ${ME} -o output_dir -i input_dir "\
       "[-c codesign_id] [-p productsign_id] [-k keychain] "\
       "[-n notarization_user]" >&2
  echo >&2
  echo "  Sign the binaries using the specified <codesign_id>, build" >&2
  echo "  the installer, and then sign the installer using the given" >&2
  echo "  <productsign_id>." >&2
  echo "  If the signing ids are not specified then the" >&2
  echo "  installer is built without signing any binaries." >&2
  echo "  If <keychain> is specified, it must contain all the signing ids." >&2
  echo "  If not specified, then the default keychains will be used." >&2
  echo "  If <notarization_user> is specified, the final DMG will be" >&2
  echo "  notarized by Apple and stapled, using a command from"
  echo "  \$NOTARIZATION_TOOL. This tool must accept a --file argument" >&2
  echo "  and handle the notarization and stapling process. The user" >&2
  echo "  argument is legacy and is not used." >&2
}

main() {
  local output_dir=""
  local input_dir=""
  local do_sign_binaries=0
  local codesign_id=""
  local productsign_id=""
  local keychain=""
  local notarization_user=""

  local OPTNAME OPTIND OPTARG
  while getopts ":o:i:c:p:k:n:h" OPTNAME; do
    case ${OPTNAME} in
      o )
        output_dir="$(shell_safe_path "${OPTARG}")"
        ;;
      i )
        input_dir="$(shell_safe_path "${OPTARG}")"
        ;;
      c )
        codesign_id="${OPTARG}"
        ;;
      p )
        productsign_id="${OPTARG}"
        ;;
      k )
        keychain="$(shell_safe_path "${OPTARG}")"
        ;;
      n )
        notarization_user="${OPTARG}"
        ;;
      h )
        usage
        exit 0
        ;;
      * )
        err "Ignoring invalid command-line option: ${OPTARG}"
        ;;
    esac
  done
  shift $(($OPTIND - 1))

  if [[ -z "${output_dir}" || -z "${input_dir}" ]]; then
    err "output_dir and input_dir are required."
    usage
    exit 1
  fi

  # There's no point in specifying a keychain but no identity, so it's probably
  # being called incorrectly.
  if [[ -n "${keychain}" && -z "${codesign_id}" ]]; then
    err_exit "Can't use keychain without a codesign_id"
  fi

  if [[ -n "${codesign_id}" ]]; then
    do_sign_binaries=1
    echo "Signing binaries with identity: ${codesign_id}"
    if [[ -n "${productsign_id}" ]]; then
      echo "Signing installer with identity: ${productsign_id}"
    fi
    if [[ -n "${keychain}" ]]; then
      echo "Using keychain: ${keychain}"
    fi
  else
    echo "Not signing binaries (no identify specified)"
  fi

  setup "${input_dir}"
  verify_clean_dir "${output_dir}"

  if [[ "${do_sign_binaries}" == 1 ]]; then
    sign_binaries "${input_dir}" "${keychain}" "${codesign_id}"
  fi
  build_packages "${input_dir}"
  if [[ "${do_sign_binaries}" == 1 && -n "${productsign_id}" ]]; then
    echo "Signing installer..."
    sign_installer "${input_dir}" "${keychain}" "${productsign_id}"
  fi
  build_dmg "${input_dir}" "${output_dir}"
  if [[ "${do_sign_binaries}" == 1 ]]; then
    sign "${output_dir}/${DMG_FILE_NAME}" "${keychain}" "${codesign_id}"
  fi

  if [[ -n "${notarization_user}" ]]; then
    notarize "${input_dir}" \
             "${output_dir}/${DMG_FILE_NAME}" \
             "${notarization_user}"
  fi

  cleanup
}

main "${@}"
exit ${?}

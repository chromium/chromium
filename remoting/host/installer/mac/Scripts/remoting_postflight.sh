#!/bin/sh

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Version = @@VERSION@@

HELPERTOOLS=/Library/PrivilegedHelperTools
SERVICE_NAME=org.chromium.chromoting
CONFIG_FILE="$HELPERTOOLS/$SERVICE_NAME.json"
OLD_SCRIPT_FILE="$HELPERTOOLS/$SERVICE_NAME.me2me.sh"
PLIST=/Library/LaunchAgents/org.chromium.chromoting.plist
BROKER_PLIST=/Library/LaunchDaemons/org.chromium.chromoting.broker.plist
PAM_CONFIG=/etc/pam.d/chrome-remote-desktop
ENABLED_FILE="$HELPERTOOLS/$SERVICE_NAME.me2me_enabled"
ENABLED_FILE_BACKUP="$ENABLED_FILE.backup"
HOST_BUNDLE_NAME=@@HOST_BUNDLE_NAME@@
HOST_SERVICE_BINARY="$HELPERTOOLS/$HOST_BUNDLE_NAME/Contents/MacOS/remoting_me2me_host_service"
HOST_LEGACY_BUNDLE_NAME=@@HOST_LEGACY_BUNDLE_NAME@@
HOST_EXE="$HELPERTOOLS/$HOST_BUNDLE_NAME/Contents/MacOS/remoting_me2me_host"
USERS_TMP_FILE="$HOST_SERVICE_BINARY.users"

# ksadmin moved from MacOS to Helpers in Keystone 1.2.13.112, 2019-11-12. A
# symbolic link from the old location was left in place, but may not remain
# indefinitely. Try the new location first, falling back to the old if needed.
KSADMIN=/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/Helpers/ksadmin
if [[ ! -x "${KSADMIN}" ]]; then
  KSADMIN=/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/MacOS/ksadmin
fi

KSUPDATE=https://tools.google.com/service/update2
KSPID=com.google.chrome_remote_desktop
KSPVERSION=@@VERSION@@

function on_error {
  logger An error occurred during Chrome Remote Desktop setup.
  exit 1
}

function find_login_window_for_user {
  # This function mimics the behaviour of pgrep, which may not be installed
  # on Mac OS X.
  local user=$1
  ps -ec -u "$user" -o comm,pid | awk '$1 == "loginwindow" { print $2; exit }'
}

# Return 0 (true) if the current OS is El Capitan (OS X 10.11) or newer.
function is_el_capitan_or_newer {
  local full_version=$(sw_vers -productVersion)

  # Split the OS version into an array.
  local version
  IFS='.' read -a version <<< "${full_version}"
  local v0="${version[0]}"
  local v1="${version[1]}"
  if [[ $v0 -gt 10 || ( $v0 -eq 10 && $v1 -ge 11 ) ]]; then
    return 0
  else
    return 1
  fi
}

trap on_error ERR
trap 'rm -f "$USERS_TMP_FILE"' EXIT

logger Running Chrome Remote Desktop postflight script @@VERSION@@

# Register a ticket with Keystone to keep this package up to date.
$KSADMIN --register --productid "$KSPID" --version "$KSPVERSION" \
    --xcpath "$PLIST" --url "$KSUPDATE"

# If there is a backup _enabled file, re-enable the service.
if [[ -f "$ENABLED_FILE_BACKUP" ]]; then
  logger Restoring _enabled file
  mv "$ENABLED_FILE_BACKUP" "$ENABLED_FILE"
fi

# If there is a backup script, restore it.
if [[ -f "$INSTALLER_TEMP/script_backup" ]]; then
  logger Restoring original launchd script
  mv "$INSTALLER_TEMP/script_backup" "$OLD_SCRIPT_FILE"
fi

# Create the PAM configuration unless it already exists and has been edited.
update_pam=1
CONTROL_LINE="# If you edit this file, please delete this line."
if [[ -f "$PAM_CONFIG" ]] && ! grep -qF "$CONTROL_LINE" "$PAM_CONFIG"; then
  update_pam=0
fi

if [[ "$update_pam" == "1" ]]; then
  logger Creating PAM config.
  # On MacOS machines with SIP enabled, the executing process requires disk
  # access permissions which cannot be granted when run silently, such as
  # during a host upgrade via Keystone. The result is a difficult to diagnose
  # error with the output "Operation not permitted". Since this is a default
  # config that we've never changed, we attempt to create it but do not bail
  # if an error occurs. Note that the error will still be logged but the
  # script will continue to execute in this case.
  $(cat > "$PAM_CONFIG" <<EOF
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

auth       required   pam_deny.so
account    required   pam_permit.so
password   required   pam_deny.so
session    required   pam_deny.so

# This file is auto-updated by the Chrome Remote Desktop installer.
$CONTROL_LINE
EOF
) || true
else
  logger PAM config has local edits. Not updating.
fi

# Create a symlink from the legacy .bundle name to the new .app name. This
# allows existing references to the legacy name to continue to work, and means
# that no changes are required to the launchd script.
rm -rf "$HELPERTOOLS/$HOST_LEGACY_BUNDLE_NAME"
ln -s "$HELPERTOOLS/$HOST_BUNDLE_NAME" "$HELPERTOOLS/$HOST_LEGACY_BUNDLE_NAME"

# Load the service for each user for whom the service was unloaded in the
# preflight script (this includes the root user, in case only the login screen
# is being remoted and this is a Keystone-triggered update).
# Also, in case this is a fresh install, load the service for the user running
# the installer, so they don't have to log out and back in again.
if [[ -n "$USER" && "$USER" != "root" ]]; then
  id -u "$USER" >> "$USERS_TMP_FILE"
fi

if [[ -r "$USERS_TMP_FILE" ]]; then
  logger Starting broker service
  launchctl load -w $BROKER_PLIST

  for uid in $(sort "$USERS_TMP_FILE" | uniq); do
    logger Starting service for user "$uid".

    load="launchctl load -w -S Aqua $PLIST"
    start="launchctl start $SERVICE_NAME"

    if is_el_capitan_or_newer; then
      bootstrap_user="launchctl asuser $uid"
    else
      # Load the launchd agent in the bootstrap context of user $uid's
      # graphical session, so that screen-capture and input-injection can
      # work. To do this, find the PID of a process which is running in that
      # context. The loginwindow process is a good candidate since the user
      # (if logged in to a session) will definitely be running it.
      pid="$(find_login_window_for_user "$uid")"
      if [[ ! -n "$pid" ]]; then
        exit 1
      fi
      sudo_user="sudo -u #$uid"
      bootstrap_user="launchctl bsexec $pid"
    fi

    logger $bootstrap_user $sudo_user $load
    $bootstrap_user $sudo_user $load
    logger $bootstrap_user $sudo_user $start
    $bootstrap_user $sudo_user $start
  done
fi

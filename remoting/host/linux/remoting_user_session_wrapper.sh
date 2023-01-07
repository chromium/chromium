#!/bin/bash

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a simple wrapper script around the remoting_user_session binary,
# intended only for development use. It is copied into a build
# subdirectory as
# $CHROMIUM_OUTPUT_DIR/remoting/user-session
# and runs the remoting_user_session binary from the same directory via sudo to
# allow testing without making remoting_user_session setuid root. The
# linux_me2me_host.py script is also copied into the remoting/ build directory,
# so it finds this user-session wrapper script in the same directory.

REMOTING_DIR="$(realpath "$(dirname "$0")")"

if [ -n "$DISPLAY" ]; then
  ELEVATE=pkexec
else
  ELEVATE=sudo
fi

PASSTHROUGH_VARIABLES=(
    "GOOGLE_CLIENT_ID_REMOTING" "GOOGLE_CLIENT_ID_REMOTING_HOST"
    "GOOGLE_CLIENT_SECRET_REMOTING" "GOOGLE_CLIENT_SECRET_REMOTING_HOST"
    "CHROME_REMOTE_DESKTOP_HOST_EXTRA_PARAMS")

ENVIRONMENT=("USER=${USER}" "LOGNAME=${LOGNAME}")
for var in "${PASSTHROUGH_VARIABLES[@]}"; do
  if [ -n "${!var+x}" ]; then
    ENVIRONMENT+=("${var}=${!var}")
  fi
done

# Simulate setuid
exec "$ELEVATE" python -c "
import os
import sys

os.chdir(sys.argv[1])
os.setreuid($(id -u), -1)
os.execvp('/usr/bin/env', ['/usr/bin/env'] + sys.argv[2:])" \
"${REMOTING_DIR}" "${ENVIRONMENT[@]}" \
"${REMOTING_DIR}/remoting_user_session" "$@"

#!/bin/sh

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

NAME=org.chromium.chromoting
PLIST=/Library/LaunchAgents/$NAME.plist

# ksadmin moved from MacOS to Helpers in Keystone 1.2.13.112, 2019-11-12. A
# symbolic link from the old location was left in place, but may not remain
# indefinitely. Try the new location first, falling back to the old if needed.
KSADMIN=/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/Helpers/ksadmin
if [[ ! -x "${KSADMIN}" ]]; then
  KSADMIN=/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/MacOS/ksadmin
fi

KSUPDATE=https://tools.google.com/service/update2
KSPID=com.google.chrome_remote_desktop
KSPVERSION=0.5

# Register a ticket with Keystone so we're updated.
$KSADMIN --register --productid $KSPID --version $KSPVERSION --xcpath $PLIST --url $KSUPDATE

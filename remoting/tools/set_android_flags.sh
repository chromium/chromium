#!/bin/sh
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script can be used to set test flags in the Chromoting Android app.

set -e

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <flags>" >&2
  exit 1
fi

FLAGS=$1

TMP_FILE=`tempfile`

# Pull the file from the device.
adb shell run-as org.chromium.chromoting cat \
  /data/data/org.chromium.chromoting/shared_prefs/Chromoting.xml > $TMP_FILE

# Remove flags parameter if it's already there.
sed -i '/<string name=\"flags\">/d' $TMP_FILE

# Add flags at the end.
sed -i "s/<\/map>/    <string name=\"flags\">$FLAGS<\/string>\n<\/map>/" \
  $TMP_FILE

# Confirmation prompt.
echo "Updated content of Chromoting.xml:"
cat $TMP_FILE
while true; do
    read -p "Continue pushing to the device (y/n)? " yn
    case $yn in
        [Yy]* ) break;;
        [Nn]* ) rm $TMP_FILE; exit;;
        * ) echo "Please answer yes or no.";;
    esac
done

# Push the file back to the device.
FILE_CONTENT="`cat $TMP_FILE`"
FILE_CONTENT="${FILE_CONTENT//\'/\'}"
FILE_CONTENT="${FILE_CONTENT//\"/\\\\\\\"}"
adb shell run-as org.chromium.chromoting sh -c "echo \\\"${FILE_CONTENT}\\\" > \
     /data/data/org.chromium.chromoting/shared_prefs/Chromoting.xml"

rm $TMP_FILE

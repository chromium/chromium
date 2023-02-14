#!/bin/bash

URL='https://android.googlesource.com/platform/frameworks/support/+/androidx-main/core/core/src/main/java/androidx/core/os/BuildCompat.java?format=TEXT'
this_dir=$(dirname $0)

set -e
set -x

# Strip out a few lines that don't compile due to missing symbols.
curl -s "$URL" | base64 -d | grep -Ev 'AD_SERVICES' > "$this_dir/java/androidx/core/os/BuildCompat.java"

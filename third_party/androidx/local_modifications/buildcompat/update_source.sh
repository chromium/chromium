#!/bin/bash

URL='https://android.googlesource.com/platform/frameworks/support/+/androidx-main/core/core/src/main/java/androidx/core/os/BuildCompat.kt?format=TEXT'
this_dir=$(dirname $0)

set -e
set -x

curl -s "$URL" | base64 -d > "$this_dir/java/androidx/core/os/BuildCompat.kt"

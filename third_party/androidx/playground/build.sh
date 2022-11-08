#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Usage:
# Make edits to *.java
# Then run: ./build.sh | less

set -e
cd $(dirname $0)

# Returns the first parameter after ensuring the path exists.
function get_path() {
  if [[ ! -f $1 ]]; then
    >&2 echo "Pattern matched no files: $1"
    exit 1
  fi
  echo "$1"
}

ANDROID_JAR=$(get_path ../../android_sdk/public/platforms/*/android.jar)
JAVA_HOME=../../jdk/current
JAVA_BIN=../../jdk/current/bin

# E.g.:
EXTRA_JARS=../../../out/Debug/lib.java/third_party/androidx/androidx_collection_collection_jvm.jar:../../../out/Debug/lib.java/third_party/android_deps/org_jetbrains_kotlin_kotlin_stdlib.jar

rm -f *.class
$JAVA_BIN/javac -cp $ANDROID_JAR:$EXTRA_JARS -target 11 -source 11 *.java
$JAVA_BIN/java -cp .:$ANDROID_JAR:$EXTRA_JARS Playground

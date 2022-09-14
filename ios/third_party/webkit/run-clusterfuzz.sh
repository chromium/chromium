#!/bin/sh
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script is used to launch WebKitTestRunner on ClusterFuzz bots.

rm -rf "$HOME/Library/Application Support/DumpRenderTree"

BASEDIR=$(dirname "$0")
DYLD_FRAMEWORK_PATH=$BASEDIR DYLD_LIBRARY_PATH=$BASEDIR ./WebKitTestRunner $@

rm -rf "$HOME/Library/Application Support/DumpRenderTree"

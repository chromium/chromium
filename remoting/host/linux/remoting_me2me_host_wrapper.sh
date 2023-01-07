#!/bin/sh

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a simple wrapper script around the remoting_me2me_host binary,
# intended only for development use. It is copied into a build
# subdirectory as
# $CHROMIUM_OUTPUT_DIR/remoting/chrome-remote-desktop-host
# and runs the remoting_me2me_host binary in the parent output directory.
# The linux_me2me_host.py script is also copied into the remoting/
# build directory, so it finds this host wrapper script in the same
# directory.

exec "$(dirname "$0")/../remoting_me2me_host" "$@"

#!/bin/sh

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Version = @@VERSION@@

logger Keystone installer for Chrome Remote Desktop
logger Version @@VERSION@@
logger /usr/sbin/installer -pkg "$1/@@HOST_PKG@@.pkg" -target /

/usr/sbin/installer -pkg "$1/@@HOST_PKG@@.pkg" -target /

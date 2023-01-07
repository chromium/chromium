# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The commands in this makefile require that the appengine SDK
# directory is in your PATH.  See README for more details.

all:

run:
	dev_appserver.py app.yaml

push:
	appcfg.py update --no_cookies .

.PHONY: all run push

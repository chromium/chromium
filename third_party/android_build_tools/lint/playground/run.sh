#!/bin/bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -xe
cd $(dirname $0)

javac Playground.java
java Playground

#!/bin/bash
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# simple script to check html via tidy. Either specify html files on
# command line or rely on default which checks all html files in
# current directory
set -o nounset
set -o errexit


CheckFile () {
  echo "========================================"
  echo "checking $1"
  echo "========================================"
  tidy -e -q $1
}


if [ $# -eq 0  ] ; then
  for file in *.html ; do
   CheckFile ${file}
  done
else
  for file in $* ; do
   CheckFile ${file}
  done
fi

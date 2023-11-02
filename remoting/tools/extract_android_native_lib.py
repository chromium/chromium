#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extracts a native library from an Android JAR."""

import os
import sys
import zipfile


def main():
  if len(sys.argv) != 4:
    print 'Usage: %s <android_app_abi> <jar file> <output file>' % sys.argv[0]
    sys.exit(1)

  android_app_abi = sys.argv[1]  # e.g. armeabi-v7a
  jar_file = sys.argv[2]  # e.g. path/to/foo.jar
  output_file = sys.argv[3]  # e.g. path/to/libfoo.so

  library_filename = os.path.basename(output_file)
  library_in_jar = os.path.join('lib', android_app_abi, library_filename)

  with zipfile.ZipFile(jar_file, 'r') as archive:
    with open(output_file, 'wb') as target:
      content = archive.read(library_in_jar)
      target.write(content)


if __name__ == '__main__':
  sys.exit(main())

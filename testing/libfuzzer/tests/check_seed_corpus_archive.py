#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script that prints out number of files in given zip-archive. Used for testing.

import os
import sys
import zipfile

if sys.version_info.major == 2:
    from ConfigParser import ConfigParser
else:
    from configparser import ConfigParser

seed_corpus_archive_path = os.path.join(os.path.dirname(sys.argv[0]),
                                                        sys.argv[1])

if not os.path.exists(seed_corpus_archive_path):
  sys.exit(-1)

archive = zipfile.ZipFile(seed_corpus_archive_path)
sys.stdout.write('%d\n' % len(archive.namelist()))

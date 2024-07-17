#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import hashlib
import json
import pathlib


# Update this when upgrading NDK.
_URL = "https://dl.google.com/android/repository/android-ndk-r27-linux.zip"

def do_latest():
  # Change version time this file changes.
  md5 = hashlib.md5()
  md5.update(pathlib.Path(__file__).read_bytes())

  this_dir = pathlib.Path(__file__).parent
  # Trigger on changes to other files in this directory.
  md5.update((this_dir / '3pp.pb').read_bytes())
  md5.update((this_dir / 'install.sh').read_bytes())
  print(md5.hexdigest())


def do_get_url():
  partial_manifest = {
    'url': [_URL],
    'ext': '.zip',
  }
  print(json.dumps(partial_manifest))


def main():
  ap = argparse.ArgumentParser()
  sub = ap.add_subparsers(required=True)

  latest = sub.add_parser("latest")
  latest.set_defaults(func=do_latest)

  download = sub.add_parser("get_url")
  download.set_defaults(func=lambda: do_get_url())

  opts = ap.parse_args()
  opts.func()


if __name__ == '__main__':
  main()

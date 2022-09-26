# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is generated, do not edit. Update BuildConfigGenerator.groovy instead.

create {
  source {
    script { name: "fetch.py" }
  }
}

upload {
  pkg_prefix: "chromium/third_party/android_deps/libs"
  universal: true
}

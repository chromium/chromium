# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    url {
      download_url: "https://registry.npmjs.org/google-closure-compiler-osx/-/google-closure-compiler-osx-20210505.0.0.tgz"
      version: "20210505.0.0"
    }
    unpack_archive: true
  }
  build {}
}

upload {
  pkg_prefix: "chromium/third_party"
  pkg_name_override: "native_closure_compiler_macos"
  universal: true
}

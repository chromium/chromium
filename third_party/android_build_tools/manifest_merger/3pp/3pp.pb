# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    script {
      name: "fetch.py"
      use_fetch_checkout_workflow: true
    }
  }

  build {
    install: "install.py"
    tool: "chromium/third_party/maven"
    dep: "chromium/third_party/jdk"
  }
}

upload {
  pkg_prefix: "chromium/third_party/android_build_tools"
  universal: true
}

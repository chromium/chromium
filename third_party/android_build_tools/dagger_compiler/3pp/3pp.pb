# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    script {
      name: "3pp.py"
      use_fetch_checkout_workflow: true
    }
  }

  build {
    install: ["3pp.py", "install"]
    tool: "chromium/third_party/maven"
  }
}

upload {
  pkg_prefix: "chromium/third_party/android_build_tools"
  universal: true
}

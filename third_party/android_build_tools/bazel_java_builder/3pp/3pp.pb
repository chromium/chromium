# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    git {
      repo: "https://github.com/bazelbuild/bazel.git"
      tag_pattern: "%s"
      version_restriction {
        op: EQ
        val: "3.7.2"
      }
    }
    patch_dir: "patches"
    patch_version: "chromium.2"
  }
  build {
    install: "install.py"
    external_tool: "infra/3pp/tools/bazelisk/${platform}@3@1.29.0"
    external_dep: "chromium/third_party/jdk@2@11.0.4+11.cr0"
  }
}

upload {
  pkg_prefix: "chromium/third_party/android_build_tools"
  universal: true
}

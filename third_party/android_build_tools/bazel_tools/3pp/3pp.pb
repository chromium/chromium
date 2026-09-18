# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  platform_re: "linux-amd64|mac-arm64"
  source {
    git {
      repo: "https://github.com/bazelbuild/bazel.git"
      tag_pattern: "%s"
      # Do not upgrade to 8.x without re-running both packagers. Two separate
      # things break, and both were observed on the try builders:
      #  * linux-amd64: bazelisk picks the Bazel release named by the tree's
      #    .bazelversion (8.7.0 for tag 8.8.0), and that binary needs
      #    GLIBC_2.25 / GLIBCXX_3.4.22 / CXXABI_1.3.11, none of which the
      #    manylinux docker image used by the 3pp recipe has.
      #  * mac-arm64: apple_support in Bazel 8 resolves Xcode to
      #    /Library/Developer/CommandLineTools, which does not exist on the
      #    packager bot, so every compile fails in xcrun.
      # The one thing 8.3+ *would* fix (the vendored zlib 1.3 "fdopen" clash
      # with the macOS SDK; 8.1 and 8.2 still have it) is instead worked
      # around in install.py.
      version_restriction {
        op: EQ
        val: "7.4.1"
      }
    }
  }
  build {
    install: "install.py"
    external_tool: "infra/3pp/tools/bazelisk/${platform}@3@1.29.0"
    # Unlike bazel_java_builder, no "chromium/third_party/jdk" external_dep is
    # needed: only C++ targets are built, and the Bazel distribution that
    # bazelisk downloads embeds its own JDK to run the Bazel server.
  }
}

upload {
  pkg_prefix: "chromium/third_party/android_build_tools"
}

# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# One create block per host platform. The install script runs after the source
# archive is pulled and stages only the NDK artifacts Chromium consumes for that
# host. The Linux NDK ships as android-ndk-<ver>-linux.zip; the macOS NDK ships
# from the same repository as android-ndk-<ver>-darwin.zip.
create {
  platform_re: "linux-amd64"
  source {
    script { name: "fetch.py" }
    unpack_archive: true
  }
  build {
    install: "install.sh"
    no_toolchain: true
  }
}
create {
  platform_re: "mac-arm64"
  source {
    script { name: "fetch.py" }
    unpack_archive: true
  }
  build {
    install: "install_mac.sh"
    no_toolchain: true
  }
}

upload {
  # Together with the "3pp"'s parent dirname, this defines the CIPD path to
  # store the generated CIPD package.
  pkg_prefix: "chromium/third_party/android_toolchain"
}

# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    url {
      download_url: "https://dl.google.com/android/repository/android-ndk-r23c-linux.zip"
      version: "r23c"
      extension: ".zip"
    }
    unpack_archive: true
    patch_version: "cr0"
  }

  # This will execute the `install.sh` script in 3pp dir after the source CIPD
  # package is pulled.
  build {
    install: "install.sh"
    no_toolchain: true
  }
}

upload {
  # Together with the "3pp"'s parent dirname, this defines the CIPD path to
  # store the generated CIPD package. The value should be:
  #   "chromium/third_party/android_toolchain/<ndk_version>"
  pkg_prefix: "chromium/third_party/android_toolchain/r23c"
  universal: true
}

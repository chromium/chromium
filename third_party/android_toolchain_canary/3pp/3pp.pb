# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    url {
      download_url: "https://ci.android.com/builds/submitted/10625055/linux/latest/raw/android-ndk-10625055-linux-x86_64.zip"
      version: "10625055"
      extension: ".zip"
    }
    unpack_archive: true
    patch_version: "cr1"
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
  # store the generated CIPD package.
  pkg_prefix: "chromium/third_party/android_toolchain_canary"
  universal: true
}

create {
  source {
    url {
      download_url: "https://android.googlesource.com/platform/dalvik/+archive/090cb5952bab050da27003badb2d27e279e62115.tar.gz"
      version: "13.0.0_r24"
    }
    patch_version: 'cr2'
    subdir: 'lib'
    unpack_archive: true
  }

  build {
    dep: "chromium/third_party/jdk"
    no_docker_env: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
}

create {
  source {
    url {
      download_url: "https://dl.google.com/dl/android/studio/jetifier-zips/1.0.0-beta10/jetifier-standalone.zip"
      version: "1.0.0-beta10"
      extension: '.zip'
    }
    patch_version: 'cr0'
    unpack_archive: true
  }
}

upload {
  universal: true
  pkg_prefix: "chromium/third_party/android_build_tools"
}

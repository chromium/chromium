create {
  platform_re: "linux-.*"
  source {
    url {
      download_url: "https://www.alsa-project.org/files/pub/lib/alsa-lib-1.2.7.2.tar.bz2"
      version: "1.2.7.2"
    }
    unpack_archive: true
    cpe_base_address: "cpe:/a:alsa-project:alsa"
  }

  build {}
}

upload {
  pkg_prefix: "chromium/third_party"
}

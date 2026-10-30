create {
  source {
    script { name: "fetch.py" }
    unpack_archive: true
  }
}

upload {
  pkg_prefix: "chromium/third_party"
  # The Agility SDK package is Windows only, containing builts DLLs for x64, x86, and arm64
  universal: true
}

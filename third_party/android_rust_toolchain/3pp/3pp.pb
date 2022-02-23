create {
  platform_re: "linux-.*|mac-.*"
  source {
    git {
      repo: "https://android.googlesource.com/platform/prebuilts/rust.git"
      tag_pattern: "rustc-%s",
    }
    patch_version: "cr2"
  }
  build {
  }
}

upload { pkg_prefix: "chromium/third_party" }

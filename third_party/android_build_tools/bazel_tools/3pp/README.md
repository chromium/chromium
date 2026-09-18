# Bazel Tools 3pp

This 3pp recipe builds three native command-line tools out of the
[Bazel](https://github.com/bazelbuild/bazel) source tree:

* `singlejar` (`//src/tools/singlejar:singlejar`)
* `ijar` (`//third_party/ijar:ijar`)
* `zipper` (`//third_party/ijar:zipper`)

All three binaries are placed at the root of the resulting CIPD package.

## Platforms

Built for `linux-amd64` and `mac-arm64` only, so the package path is
per-platform:

```
chromium/third_party/android_build_tools/bazel_tools/${platform}
```

## Bazel versions

There are two distinct versions in play, and they are deliberately different:

* **Source version** — the git tag checked out, pinned by `version_restriction`
  in `3pp.pb` (currently `7.4.1`). This is what determines the CIPD instance
  tag (`7.4.1.chromium.2`).
* **Bazel binary version** — chosen by `bazelisk` from the `.bazelversion` file
  checked into that tag (`7.3.1` for tag `7.4.1`). `install.py` deliberately
  does *not* set `USE_BAZEL_VERSION`, because that env var takes precedence
  over `.bazelversion` and would build the tree with a Bazel release upstream
  never tested it against.

### Why 7.4.1 specifically?

The usable range is bounded on both sides. All of this was determined from
actual `3pp-linux-amd64-packager` / `3pp-mac-arm64-packager` runs.

Floor:

* **>= 4.1** — `//third_party/android_build_tools/bazel_java_builder` pins 3.7.2,
  but that predates native macOS arm64 support, so bazelisk falls back to the
  `darwin-x86_64` binary and would produce x86_64 tools on the mac-arm64 bot.

Ceiling — 8.x was tried (tag 8.8.0) and failed on *both* platforms:

* **linux-amd64** — bazelisk runs the release named by the tree's
  `.bazelversion` (8.7.0 for tag 8.8.0). That binary needs `GLIBC_2.25`,
  `GLIBCXX_3.4.22` and `CXXABI_1.3.11`; the manylinux image the 3pp recipe
  builds in provides none of them, so Bazel cannot even start.
  ([failed build](https://ci.chromium.org/b/8670454286836754529))
* **mac-arm64** — `apple_support` under Bazel 8 resolves Xcode to
  `/Library/Developer/CommandLineTools`, which does not exist on the packager
  bot, so every compile dies in `xcrun`. Bazel 7 finds the bot's Xcode fine.
  ([failed build](https://ci.chromium.org/b/8670454286344579809))

The one genuine improvement in 8.3+ is that it drops the vendored zlib 1.3
(whose `zutil.h` breaks against modern macOS SDK headers) in favour of BCR
zlib 1.3.1. Note 8.1 and 8.2 do *not* help — they still vendor zlib 1.3 with
the offending block. Since we cannot take 8.x at all, `install.py` works
around it directly with `--copt=-Dfdopen=fdopen` on mac; see the comment there.

## Updating

1. Bump `val` in the `version_restriction` block of `3pp/3pp.pb` to the new
   Bazel tag, and update `Version` and the URLs in `../README.chromium`.
2. If you need to force a rebuild *without* changing the Bazel tag (for example
   after editing `install.py`), increment `patch_version` in `3pp/3pp.pb`
   instead.
3. Upload the CL and run the `3pp-linux-amd64-packager` and
   `3pp-mac-arm64-packager` try builders.
4. Once the packagers have uploaded new CIPD instances, roll the new instance
   IDs into `//DEPS`.

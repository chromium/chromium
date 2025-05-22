# Rust in Chromium

[TOC]

# Why?

Handling untrustworthy data in non-trivial ways is a major source of security
bugs, and it's therefore against Chromium's security policies
[to do it in the Browser or Gpu process](../docs/security/rule-of-2.md) unless
you are working in a memory-safe language.

Rust provides a cross-platform, memory-safe language so that all platforms can
handle untrustworthy data directly from a privileged process, without the
performance overhead and complexity of a utility process.

# Status

The Rust toolchain is enabled for and supports all platforms and development
environments that are supported by the Chromium project. The first milestone
to include full production-ready support was M119.

Rust can be used anywhere in the Chromium repository (not just `//third_party`)
subject to [current interop capabilities][interop-rust-doc], however it is
currently subject to a internal approval and FYI process. Googlers can view
go/chrome-rust for details. New usages of Rust are documented at
[`rust-fyi@chromium.org`](https://groups.google.com/a/chromium.org/g/rust-fyi).

For questions or help, reach out to
[`rust-dev@chromium.org`](https://groups.google.com/a/chromium.org/g/rust-dev),
or [`#rust` channel](https://chromium.slack.com/archives/C01T3EWCJ9Z)
on the [Chromium Slack](https://www.chromium.org/developers/slack/),
or (Google-internal, sorry)
[Chrome Rust chatroom](https://chat.google.com/room/AAAAk1UCFGg?cls=7).

If you use VSCode, we have [additional advice below](#using-vscode).

# Adding a third-party Rust library

Third-party libraries are pulled from [crates.io](https://crates.io), but
Chromium does not use Cargo as a build system.

## Third-party review

All third-party libraries (not just Rust) need to go through third-party review.
See [//docs/adding_to_third_party.md](adding_to_third_party.md) for instructions.

## Importing a crate from crates.io

Third-party crates (from [crates.io](https://crates.io))
that Chromium depends on are described by two files:

* `//third_party/rust/chromium_crates_io/Cargo.toml`.
  This file defines the set of crates
  **directly** depended on from first-party code (from Chromium first-party
  code, but also from Pdfium, V8, etc.).  Their transitive dependencies don't
  need to be listed, because they will be automatically identified and covered
  by tools like `gnrt`.  The file is a [standard `Cargo.toml` file](
  https://doc.rust-lang.org/cargo/reference/manifest.html), even though the crate
  itself is never built - it is only used to enable/disable crate features,
  specify crate versions, etc.
* `//third_party/rust/chromium_crates_io/gnrt_config.toml`.
  This file defines Chromium-specific, `cargo`-agnostic metadata like:
      - Configuring certain aspects of Chromium build (e.g. `allow_unsafe`,
        `allow_unstable_features`, `extra_src_roots`, `group = "test"`, etc.)
      - Specifying licensing information when it can't be automatically inferred
        (e.g. pointing out `license_files` with non-standard filenames).

To import a third-party crate follow the steps below:

1. Change directory to the root `src/` dir of Chromium.
1. Add the crate to `//third_party/rust/chromium_crates_io/Cargo.toml`:
   * `vpython3 ./tools/crates/run_gnrt.py add foo` to add the latest version of `foo`.
   * `vpython3 ./tools/crates/run_gnrt.py add foo@1.2.3` to add a specific version of `foo`.
   * Or, edit `//third_party/rust/chromium_crates_io/Cargo.toml` by hand,
     finding the version you want from [crates.io](https://crates.io).
1. Download the crate's files:
   * `./tools/crates/run_gnrt.py vendor` to download the new crate.
   * This will also apply any patches in `//third_party/rust/chromium_crates_io/patches`.
     See `//third_party/rust/chromium_crates_io/patches/README.md` for more details.
1. (optional) If the crate is only to be used by tests and tooling, then
   specify the `"test"` group in `//third_party/rust/chromium_crates_io/gnrt_config.toml`:
   ```
   [crate.foo]
   group = "test"
   ```
1. Generate the `BUILD.gn` file for the new crate:
   * `vpython3 ./tools/crates/run_gnrt.py gen`
1. Add the new files to git:
   * `git add -f third_party/rust/chromium_crates_io/vendor`.
     (The `-f` is important, as files may be skipped otherwise from a
     `.gitignore` inside the crate.)
   * `git add third_party/rust`
1. Upload the CL and get a review from `//third_party/rust/OWNERS`
   (check
   [`third_party/rust/OWNERS-review-checklist.md`](../third_party/rust/OWNERS-review-checklist.md)
   to see what to expect).

Note that at this point the new crate is still not seen by `gn` nor `ninja`,
and is not covered by CQ.  To make the new crate part of the build,
you need to add a `deps` edge between an existing build target
and the newly added `//third_party/rust/some_crate/v123:lib` target.
This will allow `autoninja -C out/Default third_party/rust/some_crate/v123:lib`
to work.  Additionally, this will help CQ to prevent regressions when updating
`rustc` or enabling new Rust warnings.

## Security

If a shipping library needs security review (has any `unsafe`), and the review
finds it's not satisfying the [rule of 2](../docs/security/rule-of-2.md), then
move it to the `"sandbox"` group in `//third_party/rust/chromium_crates_io/gnrt_config.toml`
to make it clear it can't be used in a privileged process:
```
[crate.foo]
group = "sandbox"
```

If a transitive dependency moves from `"safe"` to `"sandbox"` and causes
a dependency chain across the groups, it will break the `gnrt vendor` step.
You will need to fix the new crate so that it's deemed safe in unsafe review,
or move the other dependent crates out of `"safe"` as well by setting their
group in `gnrt_config.toml`.

# Updating existing third-party crates

Third-party crates will get updated semi-automatically through the process
described in
[`../tools/crates/create_update_cl.md`](../tools/crates/create_update_cl.md).
If you nevertheless need to manually update a crate to its latest minor or major
version, then follow the steps below.  To facilitate easier review, we recommend
uploading separate patchsets for 1) manual changes, and 2) tool-driven,
automated changes.

1. Change directory to the root `src/` dir of Chromium.
1. Update the versions in `//third_party/rust/chromium_crates_io/Cargo.toml`.
   * `vpython3 ./tools/crates/run_gnrt.py update <crate name>`.
   * Under the hood this invokes `cargo update` and accepts the same
     [command line parameters](https://doc.rust-lang.org/cargo/commands/cargo-update.html#update-options).
     In particular, you may need to specify `--breaking` when working on
     major version updates.
1. Download any updated crate's files:
   * `./tools/crates/run_gnrt.py vendor`
1. Add the downloaded files to git:
   * `git add -f third_party/rust/chromium_crates_io/vendor`
   * The `-f` is important, as files may be skipped otherwise from a
     `.gitignore` inside the crate.
1. Generate the `BUILD.gn` files
   * `vpython3 ./tools/crates/run_gnrt.py gen`
   * Or, directly through (nightly) cargo:
     `cargo run --release --manifest-path tools/crates/gnrt/Cargo.toml --target-dir out/gnrt gen`
1. Add the generated files to git:
   * `git add third_party/rust`

### Directory structure for third-party crates

The directory structure for a crate "foo" version 3.4.2 is:

```
//third_party/
    rust/
        foo/  (for the "foo" crate)
            v3/  (version 3.4.2 maps to the v3 epoch)
                BUILD.gn  (generated by gnrt gen)
                README.chromium  (generated by gnrt vendor)
        chromium_crates_io/
            vendor/
                foo-v3  (crate sources downloaded from crates.io)
            patches/
                foo/  (patches for the "foo" crate)
                    0001-Some-changes.diff
                    0002-Other-changes.diff
            Cargo.toml
            Cargo.lock
            gnrt_config.toml
```

## Writing a wrapper for binding generation

Most Rust libraries will need a more C++-friendly API written on top of them in
order to generate C++ bindings to them. The wrapper library can be placed
in `//third_party/rust/<cratename>/<epoch>/wrapper` or at another single place
that all C++ goes through to access the library. The [CXX](https://cxx.rs) is
used to generate bindings between C++ and Rust.

See
[`//third_party/rust/serde_json_lenient/v0_1/wrapper/`](
https://source.chromium.org/chromium/chromium/src/+/main:third_party/rust/serde_json_lenient/v0_1/wrapper/)
and
[`//components/qr_code_generator`](
https://source.chromium.org/chromium/chromium/src/+/main:components/qr_code_generator/;l=1;drc=b185db5d502d4995627e09d62c6934590031a5f2)
for examples.

Rust libraries should use the
[`rust_static_library`](
https://source.chromium.org/chromium/chromium/src/+/main:build/rust/rust_static_library.gni)
GN template (not the built-in `rust_library`) to integrate properly into the
mixed-language Chromium build and get the correct compiler options applied to
them.

See `rust-ffi.md` for information on C++/Rust FFI.

# Unstable features

Unstable features are **unsupported** by default in Chromium. Any use of an
unstable language or library feature should be agreed upon by the Rust toolchain
team before enabling it.  See
[`tools/rust/unstable_rust_feature_usage.md`](../tools/rust/unstable_rust_feature_usage.md)
for more details.

# Logging

Use the [log](https://docs.rs/log) crate's macros in place of base `LOG`
macros from C++. They do the same things. The `debug!` macro maps to
`DLOG(INFO)`, the `info!` macro maps to `LOG(INFO)`, and `warn!` and `error!`
map to `LOG(WARNING)` and `LOG(ERROR)` respectively. The additional `trace!`
macro maps to `DLOG(INFO)` (but there is [WIP to map it to `DVLOG(INFO)`](
https://chromium-review.googlesource.com/c/chromium/src/+/5996820)).

Note that the standard library also includes a helpful
[`dbg!`](https://doc.rust-lang.org/std/macro.dbg.html) macro which writes
everything about a variable to `stderr`.

Logging may not yet work in component builds:
[crbug.com/374023535](https://crbug.com/374023535).

# Tracing

TODO: [crbug.com/377915495](https://crbug.com/377915495).

# Using VSCode

1. Ensure you're using the `rust-analyzer` extension for VSCode, rather than
   earlier forms of Rust support.
2. Run `gn` with the `--export-rust-project` flag, such as:
   `gn gen out/Release --export-rust-project`.
3. `ln -s out/Release/rust-project.json rust-project.json`
4. When you run VSCode, or any other IDE that uses
   [rust-analyzer](https://rust-analyzer.github.io/) it should detect the
   `rust-project.json` and use this to give you rich browsing, autocompletion,
   type annotations etc. for all the Rust within the Chromium codebase.
5. Point rust-analyzer to the rust toolchain in Chromium. Otherwise you will
   need to install Rustc in your system, and Chromium uses the nightly
   compiler, so you would need that to match. Add the following to
   `.vscode/settings.json` in the Chromium checkout:
   ```
   {
      // The rest of the settings...

      "rust-analyzer.cargo.extraEnv": {
        "PATH": "../../third_party/rust-toolchain/bin:$PATH",
      }
   }
   ```
   This assumes you are working with an output directory like `out/Debug` which
   has two levels; adjust the number of `..` in the path according to your own
   setup.

# Using cargo

If you are building a throwaway or experimental tool, you might like to use pure
`cargo` tooling rather than `gn` and `ninja`. Even then, you may choose
to restrict yourself to the toolchain and crates that are already approved for
use in Chromium, by

* Using `tools/crates/run_cargo.py` (which will use
  Chromium's `//third_party/rust-toolchain/bin/cargo`)
* Configuring `.cargo/config.toml` to ask to use the crates vendored
  into Chromium's `//third_party/rust/chromium_crates_io`.

An example of how this can work can be found in
https://crrev.com/c/6320795/5.

[interop-rust-doc]: https://docs.google.com/document/d/1kvgaVMB_isELyDQ4nbMJYWrqrmL3UZI4tDxnyxy9RTE/edit?tab=t.0#heading=h.fpqr6hf3c3j0

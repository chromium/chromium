# Linkers for macOS and iOS builds

Chromium defaults to LLVM's LLD linker for macOS and iOS builds, but supports opting into the Apple linker (ld-prime) for local macOS and iOS development.

## Overview

Historically, Chromium migrated from Apple's legacy ld64 linker to LLD for both macOS and iOS builds to improve link times and unify the toolchain.

Recent benchmarks show that Apple's new linker (ld-prime) is faster than LLD for local macOS development (tracked by https://crbug.com/502338406). However, to maintain toolchain uniformity, control, and semantic equivalence across platforms, Chromium continues to default to LLD for all configurations. The Apple linker is supported as a developer-selectable opt-in.

- **LLD (Default)**: Used for all macOS and iOS builds except iOS arm64e, including release builds, ThinLTO, bot builds, and default local developer builds.
- **Apple linker (ld-prime) (Opt-in)**: Can be enabled for macOS and iOS builds by setting `use_lld = false` in GN args. Supported only for local development on ARM Macs (non-cross arm64 macOS builds and arm64 iOS simulator builds) to get faster link times.

## LLD Background & Design Decisions

Using LLD as the default linker offers several advantages:

- **Control**: It is developed within the LLVM repository and shipped with our Clang package. We can fix compiler/linker issues upstream and deploy them quickly without waiting for Xcode releases.
- **Interoperability**: Clang and LLD use the same LLVM libraries built at the same revision, which makes LTO setup simpler. With ld64 or ld-prime, LTO requires building a plugin loaded by the linker.
- **Uniformity**: While the Mach-O, ELF, and COFF ports of LLD are independent codebases, they share common LLVM libraries and command-line compatibility conventions, simplifying cross-platform build configurations.


## Linker Selection Criteria

### macOS Builds
- **Default:** LLD is used for all builds.
- **Opt-in (Apple linker ld-prime):** Set `use_lld = false` in `args.gn`. Supported only for local non-cross arm64 macOS builds (ARM Mac) to improve link speed (yielding up to a 4-6x speedup depending on configuration). Note that ld-prime is not supported for Intel Macs (x86-64) as either a build host or build target (opt-in support is tracked in https://crbug.com/518652539), cross-compilation, or ThinLTO builds.

### iOS Builds
- **Default:** LLD is used for all builds except arm64e, which always uses the Apple linker because LLD does not support pointer authentication yet (https://crbug.com/408032145).
- **Opt-in (Apple linker ld-prime):** Set `use_lld = false` in `args.gn`. Supported only for local arm64 simulator builds on ARM Macs to improve link speed (yielding a 4.5-5x speedup with a cold file cache and 1.8-3.1x with a warm one, https://crbug.com/518651436). iOS has no component build, so every build links the full app and the linker speed shows directly in the edit-build loop. ThinLTO builds are not supported.

## Future Work & LLD Optimizations

We are committed to further improving LLD performance to close the speed gap with the Apple linker. We will continue to evaluate both linkers as they evolve.

## Hacking on LLD
If you want to work on LLD, follow [this paragraph](clang.md#Using-a-custom-clang-binary).

## Creating stand-alone repros for LLD bugs
For simple cases, LLD's `--reproduce=foo.tar` flag / `LLD_REPRODUCE=foo.tar` env var is sufficient.

See the "Note to self" on [LLVM bug 48657](https://bugs.llvm.org/show_bug.cgi?id=48657#c0) for making a repro file that involves the full app and framework bundles.

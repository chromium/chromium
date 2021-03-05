# LLD for Mac builds

*Disclaimer* We don't use LLD for Mac builds. Using it is not supported, and
if you try to use it, it likely won't work. Only keep reading if you want to
work on the build.

## Background

Chromium uses [LLD](https://lld.llvm.org/) as linker on all platforms,
except when targeting macOS or iOS. LLD is faster than other ELF linkers (ELF
is the executable file format used on most OSs, including Linux, Android,
Chrome OS, Fuchsia), and it's faster than other COFF linkers (the executable
file format on Windows).

ld64, the standard Mach-O linker (the executable file format on iOS and macOS),
is on the other hand already fairly fast and works well, so there are fewer
advantages to using LLD here. (Having said that, LLD is currently 4x faster
at linking Chromium Framework than ld64 in symbol\_level=0 release builds,
despite ld64 being already fast. Maybe that's due to LLD not yet doing
critical things and it will get slower, but at the moment it's faster than
ld64.)

LLD does have a few advantages unrelated to speed, however:

- It's developed in the LLVM repository, and we ship it in our clang package
  (except on macOS, where it's not in the default clang package but an opt-in
  download instead). We can fix issues upstream and quickly deploy fixed
  versions, instead of having to wait for Xcode releases (which is where ld64
  ships).

- For the same reason, it has a much simpler LTO setup: Clang and LLD both link
  in the same LLVM libraries that are built at the same revision, and compiler
  and linker bitcodes are interopable for that reason. With ld64, the LTO code
  has to be built as a plugin that's loaded by the linker.

- LLD/Mach-O supports "LLVM-y" features that the ELF and COFF LLDs support as
  well, such as thin archives, colored diagnostics, and response files
  (ld64 supports this too as of Xcode 12, but we had to wait many years for it,
  and it's currently [too crashy](https://crbug.com/1147968) to be usable).

For that reason, it's possible to opt in to LLD for macOS builds (not for
iOS builds, and that's intentionally not in scope).

A background note: There are two versions of LLD upstream: The newer ports,
and an older design that's still available as ld64.lld.darwinold for Mach-O
LLD. We use the new `lld/MachO` port, not the old one. The new one is the
default selection for `-fuse-ld=lld` on macOS as of March 1 2021.

Just like the LLD ELF port tries to be commandline-compatible with other ELF
linkers and the LLD COFF port tries to be commandline-compatible with the
Visual Studio linker link.exe, the LLD Mach-O port tries to be
commandline-compatible with ld64. This means LLD accepts different flags on
different platforms.

## Current status and known issues

A `symbol_level = 0` `is_debug = false` `use_lld = true` x64 build produces
a mostly-working Chromium.app, but there are open issues and missing features:

- LLD's ARM support is very new
  - relocations aren't handled correctly ([bug](https://llvm.org/PR49444))
  - ad-hoc code signing only signs executables, not yet dylibs
    ([in-progress patch](https://reviews.llvm.org/D97994)).
  - likely other bugs for `target_cpu="arm64"`
- LLD produces bad debug info, and LLD-linked binaries don't yet show C++
  source code in a debugger ([bug](https://llvm.org/PR48714)]
- We haven't tried actually running any other binaries, so chances are many
  other tests fail
- LLD doesn't yet implement `-dead_strip`, leading to many linker warnings
- LLD doesn't yet implement deduplication (aka "ICF")
- LLD doesn't yet call graph profile sort
- LLD doesn't yet implement `-exported_symbol` or `-exported_symbols_list`,
  leading to some linker warnings

## Opting in

1. First, obtain lld. Do either of:

   1. run `src/tools/clang/scripts/update.py --package=lld_mac` to download a
      prebuilt lld binary.
   2. build `lld` and `llvm-ar` locally and copy it to
      `third_party/llvm-build/Release+Asserts/bin`. Also run
      `ln -s lld third_party/llvm-build/Release+Asserts/bin/ld64.lld`.

   You have to do this again every time `runhooks` updates the clang
   package.

   The prebuilt might work less well than a more up-to-date, locally-built
   version -- see the list of open issues above for details. If anything is
   marked "fixed upstream", then the fix is upstream but not yet in the
   prebuilt lld binary.

2. Add `use_lld = true` to your args.gn

3. Then just build normally.

`use_lld = true` makes the build use thin archives. For that reason, `use_lld`
also switches from `libtool` to `llvm-ar`.

## Creating stand-alone repros for bugs

For simple cases, LLD's `--reproduce=foo.tar` flag / `LLD_REPRODUCE=foo.tar`
env var is sufficient.

See "Note to self:" [here](https://bugs.llvm.org/show_bug.cgi?id=48657#c0) for
making a repro file that involved the full app and framework bundles.

Locally, apply this patch before building chrome to make a repro that can
be used with both lld and ld (useful for making comparisons):

    diff --git a/build/config/compiler/BUILD.gn b/build/config/compiler/BUILD.gn
    index 5ea2f2130abb..ed871642cee9 100644
    --- a/build/config/compiler/BUILD.gn
    +++ b/build/config/compiler/BUILD.gn
    @@ -1780,7 +1780,7 @@ config("export_dynamic") {
     config("thin_archive") {
       # The macOS and iOS default linker ld64 does not support reading thin
       # archives.
    -  if ((is_posix && !is_nacl && (!is_apple || use_lld)) || is_fuchsia) {
    +  if ((is_posix && !is_nacl && (!is_apple)) || is_fuchsia) {
         arflags = [ "-T" ]
       } else if (is_win && use_lld) {
         arflags = [ "/llvmlibthin" ]


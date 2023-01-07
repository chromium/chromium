# LLD for Mac builds

Like on most other platforms, Chromium uses the LLD linker on iOS and macOS.
This is a recent change. If things go well, it will ship in m95,
but we might discover showstopper bugs that delay things a bit.


## Background

Chromium uses [LLD](https://lld.llvm.org/) as linker on all platforms.
LLD is faster than other ELF linkers (ELF
is the executable file format used on most OSs, including Linux, Android,
Chrome OS, Fuchsia), and it's faster than other COFF linkers (the executable
file format on Windows).

LLD is currently 3-4x as fast as ld64, the macOS system linker, at linking
Chromium Framework in symbol\_level=0 release builds, despite ld64 being already
fast.

LLD has advantages unrelated to speed, too:

- It's developed in the LLVM repository, and we ship it in our clang package.
  We can fix issues upstream and quickly deploy fixed versions, instead of
  having to wait for Xcode releases (which is where ld64 ships).

- For the same reason, it has a much simpler LTO setup: Clang and LLD both link
  in the same LLVM libraries that are built at the same revision, and compiler
  and linker bitcodes are interopable for that reason. With ld64, the LTO code
  has to be built as a plugin that's loaded by the linker.

- LLD/Mach-O supports "LLVM-y" features that the ELF and COFF LLDs support as
  well, such as thin archives, colored diagnostics, and response files
  (ld64 supports this too as of Xcode 12, but we had to wait many years for it,
  and it's currently [too crashy](https://crbug.com/1147968) to be usable).

- While LLD for ELF, LLD for COFF, and LLD for MachO are mostly independent
  codebases, they all use LLVM libraries. That gives them similar behavior.
  Using LLD unifies the build across platforms somewhat.

For that reason, we moved to LLD for iOS and macOS builds.

Just like the LLD ELF port tries to be commandline-compatible with other ELF
linkers and the LLD COFF port tries to be commandline-compatible with the
Visual Studio linker link.exe, the LLD Mach-O port tries to be
commandline-compatible with ld64. This means LLD accepts different flags on
different platforms.

## Current status and known issues

LLD is used by default in almost all build configurations. All
tests on all bots are passing, both Intel and Arm.

There are a few open issues:

- LLD implements deduplication (aka "ICF"), but we don't yet
  enable it in arm builds ([bug](https://crbug.com/1253924)).
- LLD doesn't yet implement call graph profile sort.
- LTO support in LLD/macOS isn't complete yet, and we don't use LTO
  on macOS yet for that reason.

## Hacking on LLD

If you want to work on LLD, follow [this paragraph](clang.md#Using-a-custom-clang-binary).

## Creating stand-alone repros for bugs

For simple cases, LLD's `--reproduce=foo.tar` flag / `LLD_REPRODUCE=foo.tar`
env var is sufficient.

See "Note to self:" [here](https://bugs.llvm.org/show_bug.cgi?id=48657#c0) for
making a repro file that involved the full app and framework bundles.

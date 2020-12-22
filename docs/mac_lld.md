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
advantages to using LLD here. (Having said that, LLD is currently 2-3x faster
than ld64 in symbol\_level=0 release builds, despite ld64 being already fast.
Maybe that's due to LLD not yet doing critical things and it will get slower,
but at the moment it's faster than ld64.)

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

A background note: There are two versions of LLD upstream: The newer ports
that are nowadays used for ELF and COFF, and an older design that's still the
default Mach-O LLD. There's however an effort underway to write a new Mach-O
LLD that's built on the same design as the ELF and COFF ports. Chromium Mac
builds uses the new Mach-O port of LLD ("`ld64.lld.darwinnew`").

Just like the LLD ELF port tries to be commandline-compatible with other ELF
linkers and the LLD COFF port tries to be commandline-compatible with the
Visual Studio linker link.exe, the LLD Mach-O port tries to be
commandline-compatible with ld64. This means LLD accepts different flags on
different platforms.

## Known issues

- LLD/Mach-O is moving quickly, so things usually work best with a trunk build
  of LLD instead of the prebuilt one
- LLD-linked binaries don't work on macOS 10.13 or older
  ([bug](https://llvm.org/PR48395), fixed upstream)
- LLD-linked `protoc` crashes when it runs as part of the build
  ([bug](https://llvm.org/PR48491), fixed upstream)
- LLD cannot yet link swiftshader binaries
  ([bug](https://llvm.org/PR48332), fixed upstream)
- LLD-linked `v8_context_snapshot_generator` crashes when it runs as part of
  the build ([bug](https://llvm.org/PR48511), fixed upstream)
- verify\_framework\_order fails in LLD builds
  ([bug](https://llvm.org/PR48536), fixed upstream)
- LLD-built Chromium.app crashes with `malloc: *** error for object
  0x7fa46941f20a: pointer being freed was not allocated` at starutp (FIXME:
  file bug)
- LLD does not yet have any ARM support
  ([in-progress patch](https://reviews.llvm.org/D88629))
- LLD likely produces bad debug info, and LLD-linked binaries likely don't
  yet work in a debugger
- LLD-linked base\_unittests fails some unwind-related tests
  ([bug](https://llvm.org/PR48389))
- We haven't tried actually running any other binaries, so chances are many
  other tests fail
- LLD doesn't yet implement `-dead_strip`, leading to many linker warnings
- LLD doesn't yet implement deduplication (aka "ICF")
- LLD doesn't yet call graph profile sort
- LLD doesn't yet implement `-exported_symbol` or `-exported_symbols_list`,
  leading to some linker warnings

## Opting in

1. First, obtain lld. Do either of:

   1. build `lld` and `llvm-ar` locally and copy it to
      `third_party/llvm-build/Relase+Asserts/bin`. Also run
      `ln -s lld third_party/llvm-build/Release+Asserts/bin/ld64.lld.darwinnew`.
   2. run `src/tools/clang/scripts/update.py --package=lld_mac` to download a
      prebuilt lld binary.

   You have to do this again every time `runhooks` updates the clang
   package.

   The prebuilt might work less well than a more up-to-date, locally-built
   version.

2. Add `use_lld = true` to your args.gn

3. Then just build normally.

`use_lld = true` makes the build use thin archives. For that reason, `use_lld`
also switches from `libtool` to `llvm-ar`.


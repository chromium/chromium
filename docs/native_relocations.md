# Native Relocations

[TOC]

## What are they?
 * They tell the runtime linker a list of addresses to post-process after
   loading the executable into memory.
 * There are several types of relocations, but >99% of them are "relative"
   relocations and are created any time a global variable or constant is
   initialized with the address of something.
   * This includes vtables, function pointers, and string literals, but not
     `char[]`.

### Linux & Android Relocations (ELF Format)
 * Relocations are stored in sections of type: `REL`, `RELA`, [`APS2`][APS2], or
   [`RELR`][RELR].
 * Relocations are stored in sections named: `.rel.dyn`, `.rel.plt`,
   `.rela.dyn`, or `.rela.plt`.
 * For `REL` and `RELA`, each relocation is stored using either 2 or 3 words,
   based on the architecture.
 * For `RELR` and `APS2`, relative relocations are compressed.
   * [`APS2`][APS2]: Somewhat involved compression which trades off runtime
     performance for smaller file size.
   * [`RELR`][RELR]: Supported in Android P+. Smaller and simpler than `APS2`.
     * `RELR` is [used by default][cros] on Chrome OS.
 * As of Oct 2019, Chrome on Android (arm32) has about 390,000 of them.

[APS2]: android_native_libraries.md#Packed-Relocations
[RELR]: https://reviews.llvm.org/D48247
[cros]: https://chromium-review.googlesource.com/c/chromiumos/overlays/chromiumos-overlay/+/1210982

### Windows Relocations (PE Format)
 * For PE files, relocations are stored in per-code-page
   [`.reloc` sections][win_relocs].
 * Each relocation is stored using 2 bytes. Each `.reloc` section has a small
   overhead as well.
 * 64-bit executables have fewer relocations thanks to the ability to use
   RIP-relative (instruction-relative) addressing.

[win_relocs]: https://docs.microsoft.com/en-us/windows/win32/debug/pe-format#the-reloc-section-image-only

## Why do they matter?
### Binary Size
 * On Linux, relocations are stored very inefficiently.
   * As of Oct 2019:
     * Chrome on Linux has a `.rela.dyn` section of more than 14MiB!
     * Chrome on Android uses [`APS2`] to compress these down to ~300kb.
     * Chrome on Android with [`RELR`] would require only 60kb, but is
       [not yet enabled][relr_bug].
     * Chrome on Windows (x64) has `.relocs` sections that sum to 620KiB.

[relr_bug]: https://bugs.chromium.org/p/chromium/issues/detail?id=895194

### Memory Overhead
 * On Windows, there is [almost no memory overhead] from relocations.
 * On Linux and Android, memory with relocations cannot be loaded read-only and
   result in dirty memory. 99% of these symbols live in `.data.rel.ro`, which as
   of Oct 2019 is ~6.5MiB on Linux and ~2MiB on Android. `.data.rel.ro` is data
   that *would* have been put into `.rodata` and mapped read-only if not for the
   required relocations. The memory does not get written to after it's
   relocated, so the linker makes it read-only once relocations are applied (but
   by that point the damage is done and we have the dirty pages).
   * On Linux, we share this overhead between processes via the [zygote].
   * [On Android][relro_sharing], we share this overhead between processes by
     loading the shared library at the same address in all processes, and then
     `mremap` onto shared memory to dedupe after-the-fact.

[almost no memory overhead]: https://devblogs.microsoft.com/oldnewthing/20160413-00/?p=93301
[zygote]: linux/zygote.md
[relro_sharing]: android_native_libraries.md#relro-sharing

### Start-up Time
 * On Windows, relocations are applied just-in-time on page faults, and are
   backed by the PE file (not the pagefile).
 * On other platforms, the runtime linker applies all relocations upfront.
 * On low-end Android, it can take ~100ms (measured on a first-gen Android Go
   devices with APS2 relocations).
 * On Linux, it's [closer to 20ms][zygote].

## How do I see them?

```sh
# For ELF files:
third_party/llvm-build/Release+Asserts/bin/llvm-readelf --relocs out/Release/libmonochrome.so

# For PE files:
python tools\win\pe_summarize.py out\Release\chrome.dll
```

## Can I avoid them?
It's not practical to avoid them altogether, but there are times when you can be
smart about them.

For Example:
```c++
// The following uses 2 bytes of padding for each smaller string but creates no relocations.
// Total size overhead: 4 * 5 = 20 bytes.
const char kArr[][5] = {"as", "ab", "asdf", "fi"};

// The following requires no string padding, but uses 4 relocatable pointers.
// Total size overhead:
//   Linux 64-bit: (8 bytes per pointer + 24 bytes per relocation) * 4 entries + 14 bytes of char = 142 bytes
//   Windows 64-bit: (8 bytes per pointer + 2 bytes per relocation) * 4 entries + 14 bytes of char = 54 bytes
//   CrOS 64-bit: (8 bytes per pointer + ~0 bytes per relocation) * 4 entries + 14 bytes of char = ~46 bytes
//   Android 32-bit: (4 bytes per pointer + ~0 bytes per relocation) * 4 entries + 14 bytes of char = ~30 bytes
const char * const kArr2[] = {"as", "ab", "asdf", "fi"};
```

Notes:
* String literals (but not char arrays) are de-duped with others in the binary,
  so it is possible that the second example above might use 14 fewer bytes.
* Not all string literals require relocations. Which ones require them depends
  on the ABI. Generally, All global variables that are initialized to the
  address of something require them.

Here's a simpler example:

```c++
// No pointer, no relocation. Just 5 bytes of character data.
const char kText[] = "asdf";

// Requires pointer, relocation, and character data.
// In most cases there is no advantage to pointers for strings.
const char* const kText = "asdf";
```

Another thing to look out for:
 * Large data structures with relocations that you don't need random access to,
   or which are seldom needed.
   * For such cases, it might be better to store the data encoded and then
     decode when required.

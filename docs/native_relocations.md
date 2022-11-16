# Native Relocations

[TOC]

## What are they?
 * They tell the runtime linker a list of addresses to post-process after
   loading the executable into memory.
 * There are several types of relocations, but >99% of them are "relative"
   relocations and are created any time a global symbol or compile-time
   initialized static local is initialized with the address of something, and
   the compiler cannot optimize away the relocation (e.g. for pointers where
   not all uses are visible, or for values that are passed to functions that
   are not inlined).
   
 
Examples of things that require relative relocations:

```C++
// Pointer to yourself.
extern const void* const kUserDataKey = &kUserDataKey;
// Array of pointers.
extern const char* const kMemoryDumpAllowedArgs[] = {"dumps", nullptr};
// Array of structs that contain one or more pointers.
extern const StringPiece kStrings[] = {"one, "two"};
```

Vtables are arrays of function pointers, and so require relative relocations.
However, on some Chrome platforms we use a non-standard ABI that uses offsets
rather than pointers in order to remove the relocations overhead
([crbug/589384]).

[crbug/589384]: https://crbug.com/589384

### Linux & Android Relocations (ELF Format)
 * Relocations are stored in sections named: `.rel.dyn`, `.rel.plt`,
   `.rela.dyn`, or `.rela.plt`.
 * Relocations are stored in sections of type: `REL`, `RELA`, [`APS2`][APS2], or
   [`RELR`][RELR].
   * `REL` is default for arm32. It uses two words to per relocation: `address`,
     `flags`.
   * `RELA` is default for arm64. It uses three words per relocation: `address`,
     `flags`, `addend`.
   * [`APS2`][APS2] is what Chrome for Android uses. It stores the same fields
     as REL` / `RELA`, but uses variable length ints (LEB128) and run-length
     encoding.
   * [`RELR`][RELR] is what [Chrome OS uses], and is supported in Android P+
     ([tracking bug for enabling]). It encodes only relative relocations and
     uses a bitmask to do so (which works well since all symbols that require
     relocations live in `.data.rel.ro`).
 
[APS2]: android_native_libraries.md#Packed-Relocations
[RELR]: https://maskray.me/blog/2021-10-31-relative-relocations-and-relr
[tracking bug for enabling]: https://bugs.chromium.org/p/chromium/issues/detail?id=895194
[Chrome OS uses]: https://chromium-review.googlesource.com/c/chromiumos/overlays/chromiumos-overlay/+/1210982

### Windows Relocations (PE Format)
 * For PE files, relocations are stored in the [`.reloc` section][win_relocs].
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
 * On Windows, relocations are applied by the kernel during page faults. There
   is therefore [almost no memory overhead] from relocations, as the memory they
   are applied to is still considered "clean" memory and shared between
   processes.
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
 * On Windows, relocations are applied just-in-time, and so their overhead is
   both small and difficult to measure.
 * On other platforms, the runtime linker applies all relocations upfront.
 * On low-end Android, it can take ~100ms (measured on a first-gen Android Go
   devices with APS2 relocations).
   * On a Pixel 4a, it's ~50ms, and with RELR relocations, it's closer to 15ms.
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
extern const char kText[] = "asdf";

// Requires pointer, relocation, and character data.
// In most cases there is no advantage to pointers for strings.
// When not "extern", the compiler can often figure out how to avoid the relocation.
extern const char* const kText = "asdf";
```

Another thing to look out for:
 * Large data structures with relocations that you don't need random access to,
   or which are seldom needed.
   * For such cases, it might be better to store the data encoded and then
     decode when required.

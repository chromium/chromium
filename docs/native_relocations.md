# Native Relocations

*** note
Information here is mostly Android & Linux-specific and may not be 100% accurate.
***

## What are they?
 * For ELF files, they are sections of type REL, RELA, or RELR. They generally
   have the name ".rel.dyn" and ".rel.plt".
 * They tell the runtime linker a list of addresses to post-process after
   loading the executable into memory.
 * There are several types of relocations, but >99% of them are "relative"
   relocations and are created any time a global variable or constant is
   initialized with the address of something.
   * This includes vtables, function pointers, and string literals, but not
     `char[]`.
 * Each relocation is stored as either 2 or 3 words, based on the architecture.
   * On Android, they are compressed, which trades off runtime performance for
     smaller file size.
 * As of Oct 2019, Chrome on Android has about 390000 of them.
   * Windows and Mac have them as well, but I don't know how they differ.

## Why do they matter?
 * **Binary Size:** Except on Android, relocations are stored very
   inefficiently.
   * Chrome on Linux has a `.rela.dyn` section of more than 14MiB!
   * Android uses a [custom compression scheme][android_relro1] to shrink them
     down to ~300kb.
   * There is an even better [RELR][RELR] encoding available on Android P+, but
     not widely available on Linux yet. It makes relocations ~60kb.
 * **Memory Overhead:** Symbols with relocations cannot be loaded read-only
   and result in "dirty" memory. 99% of these symbols live in `.data.rel.ro`,
   which as of Oct 2019 is ~6.5MiB on Linux and ~2MiB on Android.
   `.data.rel.ro` is data that *would* have been put into `.rodata` and mapped
   read-only if not for the required relocations. It does not get written to
   after it's relocated, so the linker makes it read-only once relocations are
   applied (but by that point the damage is done and we have the dirty pages).
   * On Linux, we share this overhead between processes via the [zygote].
   * [On Android][android_relro2], we share this overhead between processes by
     loading the shared library at the same address in all processes, and then
     `mremap` onto shared memory to dedupe after-the-fact.
 * **Start-up Time** The runtime linker applies relocations when loading the
   executable. On low-end Android, it can take ~100ms (measured on a first-gen
   Android Go devices with APS2 relocations). On Linux, it's
   [closer to 20ms][zygote].

[zygote]: linux_zygote.md
[RELR]: https://reviews.llvm.org/D48247
[android_relro1]: android_native_libraries.md#Packed-Relocations
[android_relro2]: android_native_libraries.md#relro-sharing

## How do I see them?

```sh
third_party/llvm-build/Release+Asserts/bin/llvm-readelf --relocs out/Release/libmonochrome.so
```

## Can I avoid them?
It's not practical to avoid them altogether, but there are times when you can be
smart about them.

For Example:
```c++
// Wastes 2 bytes for each smaller string but creates no relocations.
// Total size overhead: 4 * 5 = 20 bytes.
const char kArr[][5] = {"as", "ab", "asdf", "fi"};

// String data stored optimally, but uses 4 relocatable pointers.
// Total size overhead:
//   64-bit: 8 bytes per pointer + 24 bytes per relocation + 14 bytes of char = 142 bytes
//   32-bit: 4 bytes per pointer + 8 bytes per relocation + 14 bytes of char = 62 bytes
const char *kArr2[] = {"as", "ab", "asdf", "fi"};
```

Note:
* String literals are de-duped with others in the binary, so it's possible that
  the second example above might use 14 fewer bytes.
* Not all string literals require relocations. Only those that are stored into
  global variables require them.

Another thing to look out for:
 * Large data structures with relocations that you don't need random access to,
   or which are seldom needed.
   * For such cases, it might be better to store the data encoded and then
     decode when required.

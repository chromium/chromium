# Static Initializers

[TOC]

Some background on the original decision to ban static initializers:

http://neugierig.org/software/chromium/notes/2011/08/static-initializers.html

Note: Another name for static initializers is "global constructors".

# How Static Initializers are Checked

* For Linux and Mac:
  * The expected count is stored in [//testing/scripts/check_static_initializers.py](https://source.chromium.org/chromium/chromium/src/+/main:testing/scripts/check_static_initializers.py)
* For Android:
  * The expected count is stored in the build target [//chrome/android:monochrome_static_initializers](https://cs.chromium.org/chromium/src/chrome/android/BUILD.gn)

## Removing Static Initializers

Common fixes include:

* Add constexpr.
* Move global variable to be a static variable within a function that returns
  it, often wrapped in `base::NoDestructor`.

## Listing Static Initializers

### Step 1 - Use objdump to report them
For Linux:

    tools/linux/dump-static-initializers.py out/Release/chrome

For Android (from easiest to hardest):

    # Build with: is_official_build=true is_chrome_branded=true
    # This will dump the list of SI's only when they don't match the expected
    # number in static_initializers.gni (this is what the bots use).
    ninja chrome/android:monochrome_static_initializers
    # or:
    tools/binary_size/diagnose_bloat.py HEAD  # See README.md for flags.
    # or (the other two use this under the hood):
    tools/linux/dump-static-initializers.py --toolchain-prefix third_party/android_ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/arm-linux-androideabi- out/Release/lib.unstripped/libmonochrome.so
    # arm32 ^^ vv arm64
    tools/linux/dump-static-initializers.py --toolchain-prefix third_party/android_ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android- out/Release/lib.unstripped/libmonochrome.so
    # Note: For arm64, having use_thin_lto=true seems to dump a couple extra
    #     initializers that don't actually exist.

The last one may actually be the easiest if you've already properly built
`libmonochrome.so` with `is_official_build=true`.

### Step 2 - Ask compiler to report them

If the source of the new initializers is not obvious from Step 1, you can ask the
compiler to pinpoint the exact source line.

1. Edit [//build/config/BUILDCONFIG.gn](https://cs.chromium.org/chromium/src/build/config/BUILDCONFIG.gn)
and add `"//build/config/compiler:wglobal_constructors"` to `default_compiler_configs`
2. Remove the config from the `configs` in `//base:base`
3. Set GN arg `treat_warnings_as_errors=false`
4. Compile and look for warnings **from the files identified by step 1** (may want to pipe ninja output to a file).

*** note
The compiler warning triggers for every static initializer that exists
*before optimization*. We care only about those that survive optimization.
More details in [crbug/1136086](https://bugs.chromium.org/p/chromium/issues/detail?id=1136086).
***

* For more information about `diagnose_bloat.py`, refer to its [README.md](/tools/binary_size/README.md#diagnose_bloat.py)
* List of existing static initializers documented in [static_initializers.gni](/chrome/android/static_initializers.gni)

### Step 3 - Manual verification

If the source of the new initializers is not revealed with
`dump-static-initializers.py` (e.g. for static initializers introduced in
compiler-rt), there's a manual option.

1. Locate the address range of the .init_array section with
`llvm-readelf --hex-dump=.init_array ./lib.unstripped/libmonochrome_64.so`.
It will yield an address range like 0x0917fd40 to 0x0918fd78.
2. Each .init_array slot may be zero if the contents are relocatable. To translate,
 use a command like  `llvm-readelf --relocations ./lib/unstripped | grep 0x0917fd40`
to obtain a result mapping each .init_array slot to a function address.
```
    000000000918fd40  0000000000000403 R_AARCH64_RELATIVE                51732f0
```
3. Finally, convert the address into a function name with
`llvm-addr2line --functions -e ./lib.unstripped/libmonochrome_64.so 51732f0`
```
    __cxx_global_var_init
    ./../../buildtools/third_party/libc++/trunk/src/iostream.cpp:80
```

# Static Initializers

[TOC]

Some background on the original decision to ban static initializers:

http://neugierig.org/software/chromium/notes/2011/08/static-initializers.html

# How Static Initializers are Checked

* For Linux and Mac:
  * The expected count is stored in [//infra/scripts/legacy/scripts/slave/chromium/sizes.py](https://cs.chromium.org/chromium/src/infra/scripts/legacy/scripts/slave/chromium/sizes.py)
* For Android:
  * The expected count is stored in the build target [//chrome/android:monochrome_static_initializers](https://cs.chromium.org/chromium/src/chrome/android/BUILD.gn)

## Removing Static Initializers

Common fixes include:

* Add constexpr.
* Move global variable to be a static variable within a function that returns
  it, often wrapped in `base::NoDestructor`.

## Listing Static Initializers

For Linux:

    tools/linux/dump-static-initializers.py out/Release/chrome

For Android (from easiest to hardest):

    # Build with: is_official_build=true is_chrome_branded=true
    # This will dump the list of SI's only when they don't match the expected
    # number in static_initializers.gni (this is what the bots use).
    ninja chrome/android:monochrome_static_initializers
    # or:
    tools/binary_size/diagnose_bloat.py HEAD  # See README.md for flags.
    # or:
    tools/linux/dump-static-initializers.py --toolchain-prefix third_party/android_ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/arm-linux-androideabi- out/Release/libmonochrome.so 

* For more information about `diagnose_bloat.py`, refer to its [README.md](/tools/binary_size/README.md#diagnose_bloat.py)
* List of existing static initializers documented in [static_initializers.gni](/chrome/android/static_initializers.gni)

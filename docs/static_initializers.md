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

* Add constexpr,
* Use LazyInstance<>,
* Move global variable to be a static variable within a function that returns it.

## Listing Static Initializers

For Linux:

    tools/linux/dump-static-initializers.py out/Release/chrome

For Android:

    build/android/resource_sizes.py --chromium-output-directory out/Release --dump-static-initializers out/Release/apks/MonochromePublic.apk
    tools/binary_size/diagnose_bloat.py HEAD

For more information about `diagnose_bloat.py`, refer to its [README.md](../tools/binary_size/README.md)

# Testing the Android Crazy Linker

The crazy linker is a custom dynamic linker used by Chrome on older Android
versions where dynamic linking is not as advanced. It provides
`android_dlopen_ext` functionality, RELRO sharing, compressed relocations, etc.

For crazy reasons outlined in `linker/test_case.py` this linker cannot be tested
using GTest or instrumentation test, hence it also carries a custom testing
framework. The tests are not run as part of CQ, but it is still desirable to run
them before landing changes in code locations listed below.

Sorry.

These instructions assume
[Building Chromium for Android](android_build_instructions.md) as a
prerequisite.

## Code Locations

The tested functionality is spread across these locations:
```
third_party/android_crazy_linker
base/android/java/src/org/chromium/base/library_loader
```

The tests themselves are living mostly in these places:
```
build/android/pylib/linker/test_case.py
content/shell/android
```

## Running native tests

This will run both unittests and regression tests:
```
autoninja -C out/Release android_crazy_linker_tests
out/Release/bin/run_android_crazy_linker_tests --unit-tests
```

Verbosity of the output can be increased by setting `CRAZY_DEBUG` to 1 in
`crazy_linker_debug.h`.

## Running Java Tests

We recommend running these tests in Release mode, as there are known
complications in testing with the component build. Setting `Linker.DEBUG` to
`true` should also help increase verbosity of the output.
```
autoninja -C out/Release chromium_linker_test_apk
out/Release/bin/run_chromium_linker_test_apk
```

## Fuzzer Tests

There are also a few tests for fuzzing the ZIP parser. The instructions to run
them are at the bottom of `third_party/android_crazy_linker/BUILD.gn`.

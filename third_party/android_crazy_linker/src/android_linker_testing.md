# Testing the Android Crazy Linker

The crazy linker is a custom dynamic linker used by Chrome on older Android
versions where dynamic linking is not as advanced. It provides
`android_dlopen_ext` functionality, RELRO sharing, compressed relocations, etc.

These instructions assume
[Building Chromium for Android](android_build_instructions.md) as a
prerequisite.

## Code Locations

The tested functionality is spread across these locations:
```
third_party/android_crazy_linker
base/android/java/src/org/chromium/base/library_loader
```

## Running native tests

This will run both unittests and regression tests:
```
autoninja -C out/Release android_crazy_linker_tests
out/Release/bin/run_android_crazy_linker_tests --unit-tests
```

Verbosity of the output can be increased by setting `CRAZY_DEBUG` to 1 in
`crazy_linker_debug.h`.

## Fuzzer Tests

There are also a few tests for fuzzing the ZIP parser. The instructions to run
them are at the bottom of `third_party/android_crazy_linker/BUILD.gn`.

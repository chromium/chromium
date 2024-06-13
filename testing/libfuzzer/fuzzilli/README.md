# Fuzzilli x Chrome

This experimental driver integrates Fuzzilli with Chrome for fuzz testing. It
is currently under active development, so some functionalities may not behave
as expected.

# How to use this driver?

## Pre-requisite

To use this driver, your fuzzer must:
- Take JS as input.
- Return `-1` from the fuzzing function if JS throws an exception. Return `0`
otherwise.

You must also build [Fuzzilli](https://github.com/googleprojectzero/fuzzilli).
See Fuzzilli documentation.

## Mandatory compile flags

Ensure the following build flags are enabled when compiling Chrome:

```
dcheck_always_on = false
is_asan = true
use_fuzzilli = true
use_remoteexec=true
symbol_level=2
v8_fuzzilli = true
v8_static_library = true
v8_dcheck_always_on = true
```

## Running with Fuzzilli

```
swift run -c release FuzzilliCli --storagePath=/path/to/tmp/storage --profile=your_profile --jobs=1 /out/fuzzilli/your_fuzzer
```

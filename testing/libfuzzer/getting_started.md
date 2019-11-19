# Getting started with fuzzing in Chromium

This document walks you through the basic steps to start fuzzing and suggestions
for improving your fuzz targets. If you're looking for more advanced fuzzing
topics, see the [main page](README.md).

[TOC]

## Getting started

### Setting up your build environment

Generate build files by using the `use_libfuzzer` [GN] argument together with a
sanitizer:

```bash
# AddressSanitizer is the default config we recommend testing with.
# Linux:
tools/mb/mb.py gen -m chromium.fuzz -b 'Libfuzzer Upload Linux ASan' out/libfuzzer
# Chrome OS:
tools/mb/mb.py gen -m chromium.fuzz -b 'Libfuzzer Upload Chrome OS ASan' out/libfuzzer
# Mac:
tools/mb/mb.py gen -m chromium.fuzz -b 'Libfuzzer Upload Mac ASan' out/libfuzzer
# Windows:
python tools\mb\mb.py gen -m chromium.fuzz -b "Libfuzzer Upload Windows ASan" out\libfuzzer
```

*** note
**Note:** You can also invoke [AFL] by using the `use_afl` GN argument, but we
recommend libFuzzer for local development. Running libFuzzer locally doesn't
require any special configuration and gives quick, meaningful output for speed,
coverage, and other parameters.
***

It’s possible to run fuzz targets without sanitizers, but not recommended, as
sanitizers help to detect errors which may not result in a crash otherwise.
`use_libfuzzer` is supported in the following sanitizer configurations.

| GN Argument | Description | Supported OS |
|-------------|-------------|--------------|
| `is_asan=true` | Enables [AddressSanitizer] to catch problems like buffer overruns. | Linux, Windows, Mac, Chrome OS |
| `is_msan=true` | Enables [MemorySanitizer] to catch problems like uninitialized reads<sup>\[[\*](reference.md#MSan)\]</sup>. | Linux |
| `is_ubsan_security=true` | Enables [UndefinedBehaviorSanitizer] to catch<sup>\[[\*](reference.md#UBSan)\]</sup> undefined behavior like integer overflow.| Linux |

For more on builder and sanitizer configurations, see the [Integration
Reference] page.

*** note
**Hint**: Fuzz targets are built with minimal symbols by default. You can adjust
the symbol level by setting the `symbol_level` attribute.
***

### Creating your first fuzz target

After you set up your build environment, you can create your first fuzz target:

1. In the same directory as the code you are going to fuzz (or next to the tests
   for that code), create a new `<my_fuzzer>.cc` file.

   *** note
   **Note:** Do not use the `testing/libfuzzer/fuzzers` directory. This
   directory was used for initial sample fuzz targets but is no longer
   recommended for landing new targets.
   ***

2. In the new file, define a `LLVMFuzzerTestOneInput` function:

  ```cpp
  #include <stddef.h>
  #include <stdint.h>

  extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Put your fuzzing code here and use |data| and |size| as input.
    return 0;
  }
  ```

3. In `BUILD.gn` file, define a `fuzzer_test` GN target:

  ```python
  import("//testing/libfuzzer/fuzzer_test.gni")
  fuzzer_test("my_fuzzer") {
    sources = [ "my_fuzzer.cc" ]
    deps = [ ... ]
  }
  ```

*** note
**Note:** Most of the targets are small. They may perform one or a few API calls
using the data provided by the fuzzing engine as an argument. However, fuzz
targets may be more complex if a certain initialization procedure needs to be
performed. [quic_stream_factory_fuzzer.cc] is a good example of a complex fuzz
target.
***

### Running the fuzz target

After you create your fuzz target, build it with ninja and run it locally:

```bash
# Build the fuzz target.
ninja -C out/libfuzzer url_parse_fuzzer
# Create an empty corpus directory.
mkdir corpus
# Run the fuzz target.
./out/libfuzzer/url_parse_fuzzer corpus
# If have other corpus directories, pass their paths as well:
./out/libfuzzer/url_parse_fuzzer corpus seed_corpus_dir_1 seed_corpus_dir_N
```

Your fuzz target should produce output like this:

```
INFO: Seed: 1511722356
INFO: Loaded 2 modules   (115485 guards): 22572 [0x7fe8acddf560, 0x7fe8acdf5610), 92913 [0xaa05d0, 0xafb194),
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: A corpus is not provided, starting from an empty corpus
#2  INITED cov: 961 ft: 48 corp: 1/1b exec/s: 0 rss: 48Mb
#3  NEW    cov: 986 ft: 70 corp: 2/104b exec/s: 0 rss: 48Mb L: 103/103 MS: 1 InsertRepeatedBytes-
#4  NEW    cov: 989 ft: 74 corp: 3/106b exec/s: 0 rss: 48Mb L: 2/103 MS: 1 InsertByte-
#6  NEW    cov: 991 ft: 76 corp: 4/184b exec/s: 0 rss: 48Mb L: 78/103 MS: 2 CopyPart-InsertRepeatedBytes-
```

A `... NEW ...` line appears when libFuzzer finds new and interesting inputs. If
your fuzz target is efficient, it will find a lot of them quickly. A `... pulse
...` line appears periodically to show the current status.

For more information about the output, see [libFuzzer's output documentation].

*** note
**Note:** If you observe an `odr-violation` error in the log, please try setting
the following environment variable: `ASAN_OPTIONS=detect_odr_violation=0` and
running the fuzz target again.
***

#### Symbolizing a stacktrace

If your fuzz target crashes when running locally and you see non-symbolized
stacktrace, make sure you add the `third_party/llvm-build/Release+Asserts/bin/`
directory from Chromium’s Clang package in `$PATH`. This directory contains the
`llvm-symbolizer` binary.

Alternatively, you can set an `external_symbolizer_path` via the `ASAN_OPTIONS`
environment variable:

```bash
ASAN_OPTIONS=external_symbolizer_path=/my/local/llvm/build/llvm-symbolizer \
  ./fuzzer ./crash-input
```

The same approach works with other sanitizers via `MSAN_OPTIONS`,
`UBSAN_OPTIONS`, etc.

### Submitting your fuzz target

ClusterFuzz and the build infrastructure automatically discover, build and
execute all `fuzzer_test` targets in the Chromium repository. Once you land your
fuzz target, ClusterFuzz will run it at scale. Check the [ClusterFuzz status]
page after a day or two.

If you want to better understand and optimize your fuzz target’s performance,
see the [Efficient Fuzzing Guide].

*** note
**Note:** It’s important to run fuzzers at scale, not just in your own
environment, because local fuzzing will catch fewer issues. If you run fuzz
targets at scale continuously, you’ll catch regressions and improve code
coverage over time.
***

## Optional improvements

### Common tricks

Your fuzz target may immediately discover interesting (i.e. crashing) inputs.
You can make it more effective with several easy steps:

* **Create a seed corpus**. You can guide the fuzzing engine to generate more
  relevant inputs by adding the `seed_corpus = "src/fuzz-testcases/"` attribute
  to your fuzz target and adding example files to the appropriate directory. For
  more, see the [Seed Corpus] section of the [Efficient Fuzzing Guide].

  *** note
  **Note:** make sure your corpus files are appropriately licensed.
  ***

* **Create a mutation dictionary**. You can make mutations more effective by
  providing the fuzzer with a `dict = "protocol.dict"` GN attribute and a
  dictionary file that contains interesting strings / byte sequences for the
  target API. For more, see the [Fuzzer Dictionary] section of the [Efficient
  Fuzzer Guide].

* **Specify testcase length limits**. Long inputs can be problematic, because
  they are more slowly processed by the fuzz target and increase the search
  space. By default, libFuzzer uses `-max_len=4096` or takes the longest
  testcase in the corpus if `-max_len` is not specified.

  ClusterFuzz uses different strategies for different fuzzing sessions,
  including different random values. Also, ClusterFuzz uses different fuzzing
  engines (e.g. AFL that doesn't have `-max_len` option). If your target has an
  input length limit that you would like to *strictly enforce*, add a sanity
  check to the beginning of your `LLVMFuzzerTestOneInput` function:

  ```cpp
  if (size < kMinInputLength || size > kMaxInputLength)
    return 0;
  ```

* **Generate a [code coverage report]**. See which code the fuzzer covered in
  recent runs, so you can gauge whether it hits the important code parts or not.

  **Note:** Since the code coverage of a fuzz target depends heavily on the
  corpus provided when running the target, we recommend running the fuzz target
  built with ASan locally for a little while (several minutes / hours) first.
  This will produce some corpus, which should be used for generating a code
  coverage report.

#### Disabling noisy error message logging

If the code you’re fuzzing generates a lot of error messages when encountering
incorrect or invalid data, the fuzz target will be slow and inefficient.

If the target uses Chromium logging APIs, you can silence errors by overriding
the environment used for logging in your fuzz target:

```cpp
struct Environment {
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  // Put your fuzzing code here and use data+size as input.
  return 0;
}
```

### Mutating Multiple Inputs

By default, a fuzzing engine such as libFuzzer mutates a single input (`uint8_t*
data, size_t size`). However, APIs often accept multiple arguments of various
types, rather than a single buffer. You can use three different methods to
mutate multiple inputs at once.

#### libprotobuf-mutator

If you need to mutate multiple inputs of various types and length, see [Getting
Started with libprotobuf-mutator in Chromium].

*** note
**Note:** This method requires extra effort, but works with APIs and data
structures of any complexity.
***

#### Hash-based argument

If your API accepts a buffer with data and some integer value (i.e., a bitwise
combination of flags), we recommend calculating a hash value from (`data, size`)
and using that value to fuzz an additional integer argument. For example:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string str = std::string(reinterpret_cast<const char*>(data), size);
  std::size_t data_hash = std::hash<std::string>()(str);
  APIToBeFuzzed(data, size, data_hash);
  return 0;
}

```

*** note
**Note:** The hash value derived from the data is a random value, rather than a
meaningful one controlled by the fuzzing engine. A single bit mutation might
lead to a new code coverage, but the next mutation would generate a new hash
value and trigger another code path, without providing any real guidance to the
fuzzing engine.
***

#### Bytes taken from (data, size)

You can extract one or more bytes from the data provided by fuzzing engine and
use that value for fuzzing other arguments of the target API or making other
decisions (e.g. number of iterations or attempts for calling some function).
Note that those bytes should not be used as data for any other arguments, e.g.:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Don't forget to enforce minimal data length.
  if (size < 1)
    return 0;

  // Extract single byte for fuzzing "flags" value.
  uint8_t flags = data[0];

  // Wrong, there is a bias between flags and API input.
  APIToBeFuzzed(data, size, flags);

  // Good, API input and flags are independent.
  APIToBeFuzzed(data + 1, size - 1, flags);

  return 0;
}
```

This approach addresses the problem of the *hash-based argument* approach, but
has its own **caveats**:

* If you extract any bytes from the input (either first or last ones), you
cannot use valid samples as seed corpus. In that case, you'll have to generate
seed corpus manually, i.e. append necessary bytes to the valid sample inputs.

* Imagine that `APIToBeFuzzed()` had a bug, something like the following:

```cpp
void APIToBeFuzzed(uint8_t* buffer, size_t length, uint8_t options) {
  ...
  if (options == 0x66) {
    // Yes, looks ridiculous, but things like that did happen in the real world.
    *(buffer - 1) = -1;
  }
  ...
}
```

Assuming we used the fuzz target listed above, neither ASan nor other sanitizers
would detect a buffer underwrite vulnerability, as the byte addressed by
`buffer - 1` is actually a mapped memory allocated inside the fuzzing engine as
`data[0]`.

To avoid issues like that one, we would have to allocate a separate buffer and
copy API input into it, or use a container object e.g.:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Don't forget to enforce minimal data length.
  if (size < 1)
    return 0;

  // Extract single byte for fuzzing flags value.
  uint8_t flags = data[0];

  // Put API input into a separate container.
  std::vector<uint8_t> buffer(data + 1, data + size);

  APIToBeFuzzed(buffer.data(), buffer.size(), flags);

  return 0;
}
```

There is [FuzzedDataProvider] class that might be helpful for writing fuzz
targets using that approach. In order to use it, add
`#include <fuzzer/FuzzedDataProvider.h>` to your fuzz target source file. To
learn more about `FuzzedDataProvider`, check out the
[google/fuzzing documentation page] on it.

Note that it's not recommended to use `FuzzedDataProvider` unless you actually
need to split the fuzz input. If you need to convert the fuzz input into a
vector or string object, for example, simply initialize that object by passing
`const uint8_t* data, size_t size` to its constructor.


[AFL]: AFL_integration.md
[AddressSanitizer]: http://clang.llvm.org/docs/AddressSanitizer.html
[ClusterFuzz status]: libFuzzer_integration.md#Status-Links
[Efficient Fuzzing Guide]: efficient_fuzzing.md
[FuzzedDataProvider]: https://cs.chromium.org/chromium/src/third_party/libFuzzer/src/utils/FuzzedDataProvider.h
[Fuzzer Dictionary]: efficient_fuzzing.md#Fuzzer-dictionary
[GN]: https://gn.googlesource.com/gn/+/master/README.md
[Getting Started with libprotobuf-mutator in Chromium]: libprotobuf-mutator.md
[Integration Reference]: reference.md
[MemorySanitizer]: http://clang.llvm.org/docs/MemorySanitizer.html
[Seed Corpus]: efficient_fuzzing.md#Seed-corpus
[UndefinedBehaviorSanitizer]: http://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
[code coverage report]: efficient_fuzzing.md#Code-coverage
[crbug/598448]: https://bugs.chromium.org/p/chromium/issues/detail?id=598448
[google/fuzzing documentation page]: https://github.com/google/fuzzing/blob/master/docs/split-inputs.md#fuzzed-data-provider
[libFuzzer's output documentation]: http://llvm.org/docs/LibFuzzer.html#output
[quic_stream_factory_fuzzer.cc]: https://cs.chromium.org/chromium/src/net/quic/quic_stream_factory_fuzzer.cc
[sanitizers]: https://github.com/google/sanitizers

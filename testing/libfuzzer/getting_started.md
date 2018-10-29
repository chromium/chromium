# Getting Started with libFuzzer in Chromium

*** note
**Prerequisites:** libFuzzer in Chromium is supported on Linux, Mac, and Windows
only.
***

This document will walk you through:

* setting up your build environment.
* creating your first fuzz target.
* running the fuzz target and verifying its vitals.

## Configure Build

Use `use_libfuzzer` GN argument together with sanitizer to generate build files:

*Notice*: current implementation also supports `use_afl` argument, but it is
recommended to use libFuzzer for local development. Running libFuzzer locally
doesn't require any special configuration and gives meaningful output quickly
for speed, coverage and other parameters.

```bash
# With address sanitizer
gn gen out/libfuzzer '--args=use_libfuzzer=true is_asan=true is_debug=false is_component_build=true' --check
```

Supported sanitizer configurations are:

| GN Argument | Description |
|--------------|----|
| `is_asan=true` | Enables [Address Sanitizer] to catch problems like buffer overruns. (only supported sanitizer on Windows and Mac)|
| `is_msan=true` | Enables [Memory Sanitizer] to catch problems like uninitialized reads<sup>\[[*](reference.md#MSan)\]</sup>. |
| `is_ubsan_security=true` | Enables [Undefined Behavior Sanitizer] to catch<sup>\[[*](reference.md#UBSan)\]</sup> undefined behavior like integer overflow. |
| | It is possible to run libfuzzer without any sanitizers; *probably not what you want*.|

Fuzz targets are built with minimal symbols by default. The symbol level
can be adjusted in the usual way by setting `symbol_level`.

To get the exact GN configuration that are used on our builders, see
[Build Config].

## Write Fuzz Target

Create a new `<my_fuzzer>.cc` file and define a `LLVMFuzzerTestOneInput`
function:

```cpp
#include <stddef.h>
#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // put your fuzzing code here and use data+size as input.
  return 0;
}
```

*Note*: You should create the fuzz target file `<my_fuzzer>.cc` next to the code
that is being tested and in the same directory as your other unit tests. Please
do not use `testing/libfuzzer/fuzzers` directory, this was a directory used for
initial sample fuzz targets and is no longer recommended for landing new fuzz
targets.

[quic_stream_factory_fuzzer.cc] is a good example of real-world fuzz target.

## Define GN Target

Define `fuzzer_test` GN target in BUILD.gn:

```python
import("//testing/libfuzzer/fuzzer_test.gni")
fuzzer_test("my_fuzzer") {
  sources = [ "my_fuzzer.cc" ]
  deps = [ ... ]
}
```

## Build and Run Fuzz Target Locally

Build with ninja as usual and run:

```bash
ninja -C out/libfuzzer url_parse_fuzzer
./out/libfuzzer/url_parse_fuzzer
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

* `... NEW ...` line appears when libFuzzer finds new and interesting inputs.
* an efficient fuzz target should be able to finds lots of them rather quickly.
* `... pulse ...` line will appear periodically to show the current status.

For more information about libFuzzer's output, please refer to [its own
documentation].

### Symbolize Stacktrace

If your fuzz target crashes when running locally and you see non-symbolized
stacktrace, make sure that you have directory containing `llvm-symbolizer`
binary added in `$PATH`. The symbolizer binary is included in Chromium's Clang
package located at `third_party/llvm-build/Release+Asserts/bin/` directory.

Alternatively, you can set `external_symbolizer_path` option via
`ASAN_OPTIONS` env variable:

```bash
$ ASAN_OPTIONS=external_symbolizer_path=/my/local/llvm/build/llvm-symbolizer \
    ./fuzzer ./crash-input
```

The same approach works with other sanitizers (e.g. `MSAN_OPTIONS`,
`UBSAN_OPTIONS`, etc).

## Improving Your Fuzz Target

Your fuzz target may immediately discover interesting (i.e. crashing) inputs.
To make it more efficient, several small steps can take you really far:

* Create seed corpus. Add `seed_corpus = "src/fuzz-testcases/"` attribute
to your fuzzer target and add example files in appropriate folder. Read more
in [Seed Corpus] section of the [Efficient Fuzzer Guide].
*Make sure corpus files are appropriately licensed.*
* Create mutation dictionary. With a `dict = "protocol.dict"` attribute and
`key=value` dictionary file format, mutations can be more effective.
See [Fuzzer Dictionary] section of the [Efficient Fuzzer Guide].
* Specify testcase length limits. By default, libFuzzer uses `-max_len=4096`
or takes the longest testcase in the corpus if `-max_len` is not specified.
ClusterFuzz uses different strategies for different fuzzing sessions, including
different random values. Also, ClusterFuzz uses different fuzzing engines (e.g.
AFL that doesn't have `-max_len` option). If your target has an input length
limit that you would like to *strictly enforce*, add a sanity check to the
beginning of your target function:

```cpp
if (size < kMinInputLength || size > kMaxInputLength)
  return 0;
```

### Disable noisy error message logging

If the code that you are fuzzing generates lot of error messages when
encountering incorrect or invalid data, then you need to silence those errors
in the fuzz target. Otherwise, fuzz target will be slow and inefficient.

If the target uses Chromium logging APIs, the best way to do that is to
override the environment used for logging in your fuzz target:

```cpp
struct Environment {
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

Environment* env = new Environment();
```

## Mutating Multiple Inputs

By default, a fuzzing engine such as libFuzzer mutates a single input referenced
by `uint8_t* data, size_t size`. However, quite often an API under fuzz testing
accepts multiple arguments of various types rather than a single buffer. There
are three approaches for such cases:

### 1) libprotobuf-mutator

If you need to mutate multiple inputs of various types and length, please see
[Getting Started with libprotobuf-mutator in Chromium]. That approach allows
to mutate multiple inputs independently.

**Caveats:** This approach requires an extra effort, but works with APIs and
data structures of any complexity.

### 2) hash-based argument

Another frequent case of an API under fuzz testing is a function that accepts a
buffer with data and some integer value meaning a bitwise combination of flags.
For such cases, we recommend to calculate a hash value from `(data, size)` and
use that value for fuzzing of an additional integer argument, for example:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string str = std::string(reinterpret_cast<const char*>(data), size);
  std::size_t data_hash = std::hash<std::string>()(str);
  APIToBeFuzzed(data, size, data_hash);
  return 0;
}

```

**Caveats:** Hash value derived from the data would be a random value rather
than a meaningful value controlled by fuzzing engine, i.e. a single bit mutation
would result in a completely different hash value that might lead to a new code
coverage, but the next mutation would generate another hash value and trigger
another code path, without providing a real guidance to the fuzzing engine.

### 3) bytes taken from (data, size)

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

assuming we used the fuzz target listed above, neither ASan nor other santizers
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

There is [base::FuzzedDataProvider] class that might be helpful for writing
fuzz targets using that approach.


## Submitting Fuzz Target to ClusterFuzz

ClusterFuzz builds and executes all `fuzzer_test` targets in the Chromium
repository. It is extremely important to land a fuzz target into Chromium
repository so that ClusterFuzz can run it at scale. Do not rely on just
running fuzzers locally in your own environment, as it will catch far less
issues. It's crucial to run fuzz targets continuously forever for catching
regressions and improving code coverage over time.

## Next Steps

* After your fuzz target is landed, you should check [ClusterFuzz status] page
in a day or two.
* Check the [Efficient Fuzzer Guide] to better understand your fuzz target
performance and for optimization hints.


[Address Sanitizer]: http://clang.llvm.org/docs/AddressSanitizer.html
[ClusterFuzz status]: clusterfuzz.md#Status-Links
[Efficient Fuzzer Guide]: efficient_fuzzer.md
[Fuzzer Dictionary]: efficient_fuzzer.md#Fuzzer-Dictionary
[Memory Sanitizer]: http://clang.llvm.org/docs/MemorySanitizer.html
[Seed Corpus]: efficient_fuzzer.md#Seed-Corpus
[Undefined Behavior Sanitizer]: http://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
[crbug/598448]: https://bugs.chromium.org/p/chromium/issues/detail?id=598448
[quic_stream_factory_fuzzer.cc]: https://cs.chromium.org/chromium/src/net/quic/quic_stream_factory_fuzzer.cc
[Build Config]: reference.md#Builder-configurations
[its own documentation]: http://llvm.org/docs/LibFuzzer.html#output
[Getting Started with libprotobuf-mutator in Chromium]: libprotobuf-mutator.md
[base::FuzzedDataProvider]: https://cs.chromium.org/chromium/src/base/test/fuzzed_data_provider.h

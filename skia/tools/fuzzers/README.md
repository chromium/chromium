# Skia Fuzzers in Chromium

This directory provides GN build configurations and test environment setup to build and run Skia fuzz targets within Chromium on Windows.

## Overview

Skia's fuzzers are primarily developed upstream in the Skia repository under `//third_party/skia/fuzz/oss_fuzz/` and run continuously on Linux via [OSS-Fuzz](https://github.com/google/oss-fuzz).

Because OSS-Fuzz does not support Windows, this directory defines GN build targets to compile these upstream Skia fuzzers under Chromium on Windows (`is_win && use_fuzzing_engine`) so that they can be run continuously by Chromium's ClusterFuzz infrastructure.

## Directory Structure

* `BUILD.gn`: Defines `fuzzer_test` targets (such as `skia_image_decode_fuzzer`, `skia_path_deserialize_fuzzer`, `skia_jpeg_encoder_fuzzer`, etc.) that pull sources from `//third_party/skia/fuzz/oss_fuzz/` and link them against Skia and Chromium support libraries.
* `fuzzer_environment.cc`: Configures test support required by Chromium's environment, such as setting up `base::TestDiscardableMemoryAllocator` so discardable memory allocations succeed during fuzzer execution.
* `OWNERS`: Owners responsible for reviewing changes in this directory.

## Adding and Updating Fuzzers

* **Upstream First**: New Skia fuzzers or changes to existing fuzzer logic should generally be added upstream in `//third_party/skia/fuzz/oss_fuzz/` so that they benefit OSS-Fuzz on Linux.
* **Windows Support**: Once an upstream fuzzer is available in Chromium's `//third_party/skia`, add a corresponding `fuzzer_test` target to `BUILD.gn` in this directory inside the `if (is_win && use_fuzzing_engine)` block.
* For more general details on fuzzing in Chromium, see [//testing/libfuzzer/README.md](../../../testing/libfuzzer/README.md).

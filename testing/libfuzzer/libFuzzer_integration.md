# libFuzzer and ClusterFuzz Integration

ClusterFuzz is a distributed fuzzing infrastructure that automatically
executes libFuzzer powered fuzzer tests on scale.

Googlers can read more [here](https://goto.google.com/clusterfuzz).

## Status Links

* [Buildbot] - status of all libFuzzer builds.
* [ClusterFuzz Fuzzer Stats] - fuzzing metrics.
* [Code Coverage] - code coverage of libFuzzer targets in Chrome.
* [ClusterFuzz libFuzzer Logs] - individual fuzz target run logs.
* [Corpus GCS Bucket] - current corpus for each fuzz target. Can be used to
upload bootstrapped corpus.

## Integration Details

The integration between libFuzzer and ClusterFuzz consists of:

* Build rules definition in [fuzzer_test.gni].
* [Buildbot] that automatically discovers fuzz targets using `gn refs`, builds
fuzz targets with multiple sanitizers and uploads binaries to a GCS bucket.
Recipe is defined in [chromium_libfuzzer.py].
* ClusterFuzz downloads builds and runs fuzz targets continuously.
* Fuzz target run logs are uploaded to [ClusterFuzz libFuzzer Logs] GCS bucket.
* Fuzzing corpus is maintained for each fuzz target in [Corpus GCS Bucket]. Once
a day, the corpus is minimized to reduce number of duplicates and/or reduce
effect of parasitic coverage.
* [ClusterFuzz Fuzzer Stats] displays fuzzer runtime metrics as well as
provides links to crashes and coverage reports.


## Corpus

Chromium developers can access the corpus stored in the [Corpus GCS Bucket] via
web interface or by using `gsutil` tool (the latter is easier for downloading):

```bash
mkdir local_corpus_dir
gsutil -m cp -r gs://clusterfuzz-corpus/libfuzzer/<fuzz_target> local_corpus_dir
```

[Buildbot]: https://ci.chromium.org/p/chromium/g/chromium.fuzz/builders
[Code Coverage]: https://chromium-coverage.appspot.com/reports/latest_fuzzers_only/linux/index.html
[chromium_libfuzzer.py]: https://code.google.com/p/chromium/codesearch#chromium/build/scripts/slave/recipes/chromium_libfuzzer.py
[ClusterFuzz Fuzzer Stats]: https://clusterfuzz.com/fuzzer-stats/by-fuzzer/fuzzer/libFuzzer/job/libfuzzer_chrome_asan
[ClusterFuzz libFuzzer Logs]: https://console.cloud.google.com/storage/browser/clusterfuzz-libfuzzer-logs
[Corpus GCS Bucket]: https://console.cloud.google.com/storage/clusterfuzz-corpus/libfuzzer
[fuzzer_test.gni]: https://code.google.com/p/chromium/codesearch#chromium/src/testing/libfuzzer/fuzzer_test.gni

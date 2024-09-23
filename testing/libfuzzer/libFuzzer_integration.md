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
Recipe is defined in [fuzz.py].
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
[Code Coverage]: https://analysis.chromium.org/coverage/p/chromium?platform=fuzz&test_suite_type=any&path=%2F%2F&host=chromium.googlesource.com&project=chromium%2Fsrc&ref=refs%2Fheads%2Fmain&path=%2F%2F&modifier_id=0
[fuzz.py]: https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipes/chromium/fuzz.py?q=fuzz.py
[ClusterFuzz Fuzzer Stats]: https://clusterfuzz.com/fuzzer-stats/by-fuzzer/fuzzer/libFuzzer/job/libfuzzer_chrome_asan
[ClusterFuzz libFuzzer Logs]: https://console.cloud.google.com/storage/browser/clusterfuzz-libfuzzer-logs
[Corpus GCS Bucket]: https://console.cloud.google.com/storage/clusterfuzz-corpus/libfuzzer
[fuzzer_test.gni]: https://source.chromium.org/chromium/chromium/src/+/HEAD:testing/libfuzzer/fuzzer_test.gni

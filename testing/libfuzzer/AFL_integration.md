# AFL Integration

Chromium previously used a fuzzing engine called [AFL], using (nearly) the same
set of fuzz targets as libfuzzer. This support has now been disabled.

## Trophies

* [AFL Chromium bugs] - bugs found by AFL in Chromium.
* [AFL OSS-Fuzz bugs] - bugs found by AFL in [OSS-Fuzz].

[AFL]: http://lcamtuf.coredump.cx/afl/
[AFL Chromium bugs]: https://bugs.chromium.org/p/chromium/issues/list?can=1&q=afl_chrome_asan+-status%3AWontFix%2CDuplicate+label%3Aclusterfuzz
[AFL OSS-Fuzz bugs]: https://bugs.chromium.org/p/oss-fuzz/issues/list?can=1&q=label%3AEngine-afl%2CStability-AFL+label%3AClusterFuzz+-status%3AWontFix%2CDuplicate

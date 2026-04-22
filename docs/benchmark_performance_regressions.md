# Regressions to Competitive Benchmarks

Speed is one of Chrome’s core strengths. One of the tools we use to
measure Chrome’s speed is through benchmarks, and specifically the
competitive benchmarks (Speedometer, MotionMark, JetStream).
Improving and ensuring our current level of
performance does not regress is challenging. It’s all too easy for
performance regressions to creep in. Just as with test failures, the
longer we allow the regression to remain in the code base, the harder
it is to fix.

To ensure Chrome’s performance on benchmarks does not regress, we have the
following policy: If a regression is detected, we will file a bug with
relevant data (including links to pinpoint runs). If after one business day
the regression has not been resolved, the patch is reverted.

If you know your change impacts performance, you can reach out to the
appropriate group to discuss the issue before landing (see table that
follows with contacts). This can help prevent being reverted. Instructions
for using pinpoint are at the end of this document.

This policy applies to the latest version (and finalized ready to be
released development/main-branch versions).
These benchmarks consists of a number of subtests with different regression
thresholds.

| Benchmark     | Owner                  | Overall Threshold | Subtest Threshold |
|---------------|------------------------|-------------------|-------------------|
| Speedometer X | v8-performance-sheriff |               .3% |                1% |
| JetStream X   | v8-performance-sheriff |               .3% |                1% |
| MotionMark X  | chrome-gpu             |                1% |                2% |

Pinpoint will be used to locate and validate the regression. The
number of runs will come from statistical analysis and may
change from time to time (currently around 128 for Speedometer).

Bugs filed will generally have the following text:

***note
This patch has been identified as causing a statistically significant
regression to the competitive benchmark &lt;NAME HERE&gt;. The pinpoint run
&lt;LINK HERE&gt; gives high confidence this patch is responsible for the
regression. Please treat this as you would a unit test failure and
resolve the issue promptly. If you do not resolve the issue in 24
hours the patch will be reverted. For help, please reach out to the
appropriate group and/or owner.
The recommended course of action is:
1. Revert patch.
2. If unsure why your patch caused a regression, reach out to owners.
3. Update patch.
4. Use pinpoint to verify no regressions.
5. Reland.
Each patch is unique, so while this is the recommended course of action, it
won't cover every case.
More information on this policy can be found
[here](https://chromium.googlesource.com/chromium/src/+/main/docs/benchmark_performance_regressions.md).
***

### Using pinpoint

To run a pinpoint job you can either use a command line tool
[`tools/perf/cb pinpoint`](https://crsrc.org/c/tools/perf/cb) or
[pinpoint](https://pinpoint-dot-chromeperf.appspot.com/).

| Benchmark   | Benchmark Field               | Story                       | Runs | Priority |
|-------------|-------------------------------|-----------------------------|------|----------|
| Speedometer | **`speedometer3.crossbench`** |                             | 128  | P0       |
|             | `speedometer-main.crossbench` |                             | 128  | P1       |
|             | `speedometer2.crossbench`     |                             | 128  | P1       |
| JetStream   | **`jetstream3.crossbench`**   |                             | 128  | P0       |
|             | `jetstream-main.crossbench`   |                             | 128  | P1       |
|             | `jetstream2.crossbench`       |                             | 128  | P1       |
| MotionMark  | `rendering.desktop.notracing` | `motionmark_ramp_composite` |      | P0       |

Non-stable benchmark versions (e.g. those for development/main-branch, or
previous major releases) might be used as backup signals,
however the main priority is with the officially released stable version.
The upcoming versions of each benchmark (e.g. development/main-branch  with
the `-main` suffix)  have equal
priority to stable versions when a release date is fixed (e.g. within a
quarter).

| Platform   | Bot                     | Priority |
|------------|-------------------------| ---------|
| MacOS      | `mac-m4-mini-perf`      | P0       |
|            | `mac-m1_mini_2020-perf` | P1       |
| Windows    | `win-11-perf`           | P2       |
| Linux      | `linux-r350-perf`       | P2       |

The latest Apple available hardware with enough resources always has the
highest priority,

The only other field you should need to fill in is the "Exp patch" field.
Put your URL of your patch there, and click "Start".

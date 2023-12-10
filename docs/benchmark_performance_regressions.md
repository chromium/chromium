# Regressions to Competitive Benchmarks

Speed is one of Chrome’s core strengths. One of the tools we use to
measure Chrome’s speed is through benchmarks, and specifically the
competitive benchmarks (Speedometer, MotionMark, JetStream). Improving and ensuring our current level of
performance does not regress is challenging. It’s all too easy for
performance regressions to creep in. Just as with test failures, the
longer we allow the regression to remain in the code base, the harder
it is to fix.

To ensure Chrome’s performance on benchmarks does not regress, we have the following
policy: If a regression is detected, we will file a bug with relevant
data (including links to pinpoint runs). If after one business day the
regression has not been resolved, the patch is reverted.

If you know your change impacts performance, you can reach out to the
appropriate group to discuss the issue before landing (see table that
follows with contacts). This can help prevent being reverted. Instructions for using
pinpoint are at the end of this document.

This policy applies to the competitive benchmarks: JetStream,
MotionMark, and Speedometer. For Speedometer specifically, the policy applies
to both, the current (as of Nov 2023) stable version 2 and the work in progress
version 3. We expect Speedometer 3 to be fully released early 2024. At this
time, this policy applies to bots running MacOS with Apple Silicon. Each of
these benchmarks consists of a number of subtests. There are thresholds for
both the test, and subtest.

| Benchmark   | Owner                  | Overall Threshold | Subtest Threshold |
|-------------|------------------------|-------------------|-------------------|
| JetStream   | v8-performance-sheriff |               .3% |                1% |
| MotionMark  | chrome-gpu             |                1% |                2% |
| Speedometer | v8-performance-sheriff |               .3% |                1% |

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
Each patch is unique, so while this is the recommended course of action, it won't cover every case.
More information on this policy can be found [here](https://chromium.googlesource.com/chromium/src/+/main/docs/benchmark_performance_regressions.md).
***

### Using pinpoint

To run a pinpoint job you can either use a command line tool
(```depot_tools/pinpoint```) or [pinpoint](https://pinpoint-dot-chromeperf.appspot.com/).
I recommend the web ui as it's better supported. To use the web ui click the plus button in the
bottom right. For the bot, use mac-m1_mini_2020-perf or mac-m1_mini_2020-perf-pgo. The PGO
builder is closer to what we ship, but it will use a slightly dated pgo profile, which means
the results may not be exactly what you see once the profile is built with your change. The
following table suggests what to enter for the benchmark and story fields:

|Benchmark    | Benchmark Field             | Story                      |
|-------------|-----------------------------|----------------------------|
| Jestream    | Jetstream2                  | Jetstream2                 |
| Speedometer | speedometer2                | Speedometer2               |
|             | speedometer3                | Speedometer3               |
| MotionMark  | rendering.desktop.notracing | motionmark_ramp_composite  |

The only other field you should need to fill in is the "Exp patch" field. Put your URL of your
patch there, and click "Start".
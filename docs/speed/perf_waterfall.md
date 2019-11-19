# chromium.perf Waterfall

## Overview

The [chrome.perf waterfall](https://ci.chromium.org/p/chrome/g/chrome.perf/console)
continuously builds and runs our performance tests on real Android, Windows,
Mac, and Linux hardware; see [list of platforms](perf_lab_platforms.md).
Results are reported to the
[Performance Dashboard](https://chromeperf.appspot.com/) for analysis. The
[Perfbot Health Sheriffing Rotation](bot_health_sheriffing/main.md) ensures that the benchmarks stay green. The [Perf Sheriff Rotation](perf_regression_sheriffing.md) ensures that any regressions detected by those benchmarks are addressed quickly. Together, these rotations maintain
[Chrome's Core Principles](https://www.chromium.org/developers/core-principles)
of speed:

> "If you make a change that regresses measured performance, you will be
> required to fix it or revert".

## Contact

  * You can reach the Chromium performance sheriffs at perf-sheriffs@chromium.org.
  * Bugs on waterfall issues should have Component:
    [Speed>Benchmarks>Waterfall](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3ASpeed%3EBenchmarks%3EWaterfall+&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids).
  * For domain knowledge for a specific benchmark, consider reaching out to
    benchmark owners listed in
    [benchmark.csv](https://docs.google.com/spreadsheets/d/1xaAo0_SU3iDfGdqDJZX_jRV0QtkufwHUKH3kQKF3YQs/edit#gid=0)

## Links

  * [Perf Sheriff Rotation](perf_regression_sheriffing.md)
  * [Perfbot Health Sheriffing Rotation](bot_health_sheriffing/main.md)
  * [How to SSH to Bots in Lab](https://chrome-internal.googlesource.com/infra/infra_internal/+/master/doc/ssh.md)
    (googlers only!)
  * TODO: Page on how to repro failures locally
  * TODO: Page on how to hack on buildbot/recipe code
  * TODO: Page on bringing up new hardware

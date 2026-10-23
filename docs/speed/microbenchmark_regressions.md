# Micro-benchmark Regressions

[TOC]

##  Microbenchmarks

A microbenchmark attempts to measure the performance of a "small" bit of code.
These tests are typically in the sub-millisecond range. The code being tested
usually performs no I/O, or else is a test of some single, specific I/O task.
[1]

We maintain and measure a set of such microbenchmarks for example
[blink_perf](./benchmark/harnesses/blink_perf.md), dromaeo, gpu_perftests, and
rasterize_and_record_micro.

[1] https://github.com/google/caliper/wiki/JavaMicrobenchmarks#what-is-a-microbenchmark

## Interesting sources of false positives

Due to their nature microbenchmarks are much more sensitive to changes in the
underlying abstraction levels which can result in a large number of false
positive alerts. Below are some of such changes relevant to Chromium.

### AFDO rolls.

Automatic Feedback Directed Optimization is a process that produces
profile-based optimization hints that are applied by the compiler. It can result
in changes to inlining (as well as causing more inlining, it can also prevent
inlining). The inlining decisions made by AFDO can have noticeable impact on
micro benchmarks, especially those involving tight loops or tight
recursion. Unfortunately the decisions are not always stable and can flip from
*inline* to *don't inline* without any changes in the code being benchmarked.

https://crbug.com/889742 is has more details and many duped bugs.

### No-op refactors that prevent AFDO

It's also possible to make no-op changes to code, causing the previous AFDO data
to be inapplicable (e.g. function name change). This can result in apparent
regressions which recover spontaneously once new AFDO data is generated based on
the new code. E.g. https://crbug.com/855544 was a specific case of this. One way
to dig into this is to [examine the compiled functions](../disassemble_code.md)
before and after the no-op change, to see if inlining has changed.

If you have a function that you believe should always be inlined in production
builds, please contact gbiv@chromium.org for advice.

### Toolchain rolls

Our toolchain team regularly rolls in new versions of clang, the compiler for
all of Chromium. Though it's rare, these rolls may cause unintended performance
changes. These rolls are represented as regular CLs/commits to Chromium's
repository (e.g.
https://chromium-review.googlesource.com/c/chromium/src/+/1436036), so
it's often pretty simple to attribute a performance change to a compiler
roll. If you believe a compiler roll has slowed down your microbenchmark, please
reach out to whoever landed the roll for guidance. It may be difficult, but if
you can reduce your test-case in any way from 'simply build all of Chrome!'
(even by just providing the before/after assembly), it's a huge help.

## Repeatable benchmarks

Dynamic CPU speed scaling and other compute environment factors can increase
variance between benchmark runs. Controlling the environment to make benchmarks
more repeatable will make it easier to see the effect from code changes.

In general, try to lock the CPU at a specific frequency. Avoid letting dynamic
frequency scaling increase the clock speed and powersaving measures decrease the
clock speed. Note that this is focused on comparing different code on the same
CPU, and is not meaningful for benchmarking the same code on different CPUs.

Similarly, lock the process being benchmarked to a single core, or a specific
set of cores. Oftentime it is better to avoid core 0, which may have more
contention.

### Linux-specifics

Use the performance governor to set a fixed CPU frequency.

```
for cpu in /sys/devices/system/cpu/cpu[0-9]*; do
  sudo sh -c "echo performance > $cpu/cpufreq/scaling_governor";
done
```

Disable dynamic CPU speed scaling.

```
# Intel
sudo sh -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"

# AMD/Generic
sudo sh -c "echo 0 > /sys/devices/system/cpu/cpufreq/boost"
```

Scheduling execution on specific cores.

```
taskset -c 3 CMD
```

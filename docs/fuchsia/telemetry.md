# Running Telemetry Benchmarks on Fuchsia

[TOC]

General instruction on running and debugging benchmarks can be found in the
[`tools/perf/README.md`](../../tools/perf/README.md).

Fuchsia uses [web_engine_shell](../../fuchsia_web/webengine/test/README.md) to run
integration tests. Be sure to build any components you wish to deploy on your
device, along with `web_engine_shell`.

## Supported benchmarks

The list of supported benchmarks can be found in the
[bot_platforms.py](https://source.chromium.org/chromium/chromium/src/+/main:tools/perf/core/bot_platforms.py?q=_FUCHSIA_PERF_FYI_BENCHMARK_CONFIGS).

The list of stories that are skipped can be found in
[expectations.config](https://source.chromium.org/chromium/chromium/src/+/main:tools/perf/expectations.config),
with the `fuchsia` tag.

## Run on a local device

---

**NOTE** This has not been tested as these instructions were developed while
remote work was the only available option.

---

After building chromium, be sure to have a device that has gone through
OOBE attached to the host. Run the following from the chromium build
directory if you want to run a specific benchmark:

```shell
$ ../../content/test/gpu/run_telemetry_benchmark_fuchsia.py \
--browser=web-engine-shell  --output-format=histograms  \
--experimental-tbmv3-metrics -d
[--repo=/path/to/fuchsia/out/dir --no-repo-init] \  # Deploy custom Fuchsia.
[benchmark] [--story-filter=<story name>]
```

For instance, to run the simplest story, `load:chrome:blank` from the benchmark
`system_health.memory_desktop`:

```shell
$ ../../content/test/gpu/run_telemetry_benchmark_fuchsia.py \
--browser=web-engine-shell  --output-format=histograms  \
--experimental-tbmv3-metrics -d system_health.memory_desktop \
--story-filter=load:chrome:blank
```

If no benchmark or filter is specified, all supported benchmarks will run.

## Run on an ephemeral emulator
If you wish to run the tests on an emulator, simply drop the `-d` flag. This
will start an emulator and run through the supported tests, like so.

```shell
$ ../../content/test/gpu/run_telemetry_benchmark_fuchsia.py \
--browser=web-engine-shell  --output-format=histograms  \
--experimental-tbmv3-metrics system_health.memory_desktop \
--story-filter=load:chrome:blank
```

Note that this needs to be run from an x64 build directory.

## Run on a remote device

As connecting to a device that is not connected to a workstation is more common,
this flow is what is recommended and tested.

Be sure to first open a tunnel to your device:

```shell
$ ../../content/test/gpu/run_telemetry_benchmark_fuchsia.py \
--browser=web-engine-shell  --output-format=histograms  \
--experimental-tbmv3-metrics -d --target-id=[::1]:8022 \
[benchmark] [--story-filter=<story name>]
```
See the above section on how to use the `benchmark` and `--story-filter`.

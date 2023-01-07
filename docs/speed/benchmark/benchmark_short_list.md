# Chrome Speed Benchmark Shortlist

Target audience: if you are a Chrome developer who wants to quickly gauge how
your change would affect Chrome performance, then this doc is for you.

> **Warning:** this doc just gives you a general reduced list of Chrome
> benchmarks to try out if you don’t know where to start or just want some quick
> feedback. There is no guarantee that if your change doesn’t regress these
> benchmarks, it won’t regress any benchmarks or regress UMA metrics. We believe
> it’s best to rely on [key UMA metrics](https://docs.google.com/document/d/1Ww487ZskJ-xBmJGwPO-XPz_QcJvw-kSNffm0nPhVpj8/edit#heading=h.2uunmi119swk)
> to evaluate performance effects of any Chrome change.

Here is the list of benchmarks which we recommend:
Android:
*   system_health.common_mobile
*   system_health.memory_mobile
*   startup.mobile

Desktop:
*   system_health.common_desktop
*   system_health.memory_desktop

Both desktop & mobile:
*   speedometer2


Instructions for how to run these benchmarks:
*   [Running them locally](https://chromium.googlesource.com/catapult/+/main/telemetry/docs/run_benchmarks_locally.md)
*   [Running on perf trybot](https://chromium.googlesource.com/chromium/src/+/main/docs/speed/perf_trybots.md)

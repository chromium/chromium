# Chrome Speed Metrics

[TOC]

## Mission
The Chrome Speed Metrics team aims to quantify users' experience of the web to
provide Chrome engineers and web developers the metrics, insights, and
incentives they need to improve it. We aim to:

  * **Quantify** web UX via a high quality set of UX metrics which Chrome devs
    align on.
  * **Expose** these metrics consistently to Chrome and Web devs, in the lab and
    the wild.
  * **Analyze** these metrics, producing actionable reports driving our UX
    efforts.
  * **Own** implementation for these metrics for TBMv2, UMA/UKM, and web perf
    APIs.

## Goals

### Quantify Users’ Experience of the Web
Chrome needs a small, consistent set of high quality user experience metrics.
Chrome Speed Metrics is responsible for authoring reference implementations of
these metrics implemented using Trace Based Metrics v2 (TBMv2) in
[tracing/metrics](https://source.chromium.org/chromium/chromium/src/+/main:third_party/catapult/tracing/tracing/metrics/).
These reference implementations will often require adding C++ instrumentation.
Some metrics work will also be driven by more focused metrics teams, such as the
work on Frame Throughput. Chrome Speed Metrics also owns UMA/UKM metrics, and
speed metrics related Web Perf APIs.

The wider set of folks involved in defining these metrics will include:

  * Area domain experts.
  * Focused metrics teams.
  * Devtools folks.
  * DevX, documenting what these metrics mean for external developers.
  * Occasional other experts (e.g., UMA folks).

### Expose Consistent Metrics Everywhere
Chrome Speed Metrics is responsible for ensuring that our core metrics are
exposed everywhere. This includes collaborating with devtools, lighthouse, etc.
to make sure our metrics are easy to expose, and are exposed effectively.

### Analyze Chrome Performance, providing data to drive our performance efforts
Metrics aren’t useful if no one looks at them. Chrome Speed Metrics performs
detailed analysis on our key metrics and breakdown metrics, providing actionable
reports on how Chrome performs in the lab and in the wild. These reports are
used to guide regular decision making processes as part of Chrome’s planning
cycle, as well as to inspire Chrome engineers with concrete ideas for how to
improve Chrome’s UX.

### Own Core Metrics
The Chrome Speed Metrics team will gradually gain ownership of
[tracing/metrics](https://source.chromium.org/chromium/chromium/src/+/main:third_party/catapult/tracing/tracing/metrics/),
and will be responsible for the long term code health of this directory. We’re
also ramping up ownership in the Web Perf API space.

## Contact information
  * **Email**: speed-metrics-dev@chromium.org
  * **Tech lead**: sullivan@chromium.org
  * **File a bug**:
    * For issues related to web performance APIs, file a bug
      [here](https://bugs.chromium.org/p/chromium/issues/entry?template=Defect+report+from+developer&components=Blink%3EPerformanceAPIs)
    * For other kinds of issues, file a bug
      [here](https://bugs.chromium.org/p/chromium/issues/entry?template=Defect+report+from+developer&components=Speed%3EMetrics)

## APIs we own
  * [Element Timing](https://github.com/WICG/element-timing)
  * [Event Timing](https://github.com/WICG/event-timing)
  * [HR Time](https://github.com/w3c/hr-time/)
  * [Largest Contentful Paint](https://github.com/WICG/largest-contentful-paint)
  * [Layout Instability](https://github.com/WICG/layout-instability)
  * [Longtasks](https://github.com/w3c/longtasks/)
  * We own some of the implementation details of [Navigation
    Timing](https://github.com/w3c/navigation-timing/)
  * We are ramping up on [Page
    Visibility](https://github.com/w3c/page-visibility/)
  * [Paint Timing](https://github.com/w3c/paint-timing/)
  * [Performance Timeline](https://github.com/w3c/performance-timeline)
  * We own some of the implementation details of [Resource
    Timing](https://github.com/w3c/resource-timing)
  * [User Timing](https://github.com/w3c/user-timing)

## Web performance objectives
  * See our [web performance objectives](webperf_okrs.md).

## Metrics changelog
We realize it's important to keep developers on the loop regarding important
changes to our metric definitions. For this reason, we have created a [metrics
changelog](../speed/metrics_changelog/README.md) which will be updated over time.

## User Docs
  * [Metrics for web developers](https://web.dev/metrics/).
  * [Properties of a good metric](../speed/good_toplevel_metrics.md)
  * [Survey of current
    metrics](https://docs.google.com/document/d/1Ww487ZskJ-xBmJGwPO-XPz_QcJvw-kSNffm0nPhVpj8/edit)
  * [Debugging CLS](http://bit.ly/debug-cls)

## Talks
  * [Lessons learned from performance monitoring in
    Chrome](https://www.youtube.com/watch?v=ctavZT87syI), by Annie Sullivan.
  * [Shipping a performance API on
    Chromium](https://ftp.osuosl.org/pub/fosdem/2020/H.1309/webperf_chromium_development.webm),
    by Nicolás Peña Moreno.
  * [Understanding Cumulative Layout
    Shift](https://www.youtube.com/watch?v=zIJuY-JCjqw), by Annie Sullivan and
    Steve Kobes.

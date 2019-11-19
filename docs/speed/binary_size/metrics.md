# Binary Size Metrics

This document lists metrics used to track binary size.

[TOC]

## Metrics for Desktop

 * Sizes are collected by
   [//build/scripts/slave/chromium/sizes.py](https://cs.chromium.org/chromium/build/scripts/slave/chromium/sizes.py)
   * [Win32 Telemetry Graphs](https://chromeperf.appspot.com/report?sid=b3dcc318b51f3780924dfd3d82265ca901ac690cb61af91919997dda9821547c)
   * [Linux Telemetry Graphs](https://chromeperf.appspot.com/report?sid=bd18d34b6d29f26877e7075cb5c34c56c011d99803e9120d61610d7eaef38e9c)
   * [Mac Telemetry Graphs](https://chromeperf.appspot.com/report?sid=2cb6e0a9941e63418e7b83f91583282fa9fbaaafc2d19b3fa1179b28e7d3f7eb)

### Alerting

 * Alerts are sheriffed as part of the main perf sherif rotation.
 * Alerts generally fire for ~100kb jumps.

## Metrics for Android

For Googlers, more information available at [go/chrome-apk-size](https://goto.google.com/chrome-apk-size).

 * Sizes are collected by
   [//build/android/resource_sizes.py](https://cs.chromium.org/chromium/src/build/android/resource_sizes.py).
 * How to analyze Android binary size discussed in [apk_size_regressions.md#debugging-apk-size-increase](../apk_size_regressions.md#debugging-apk-size-increase).
 * Sizes for `ChromePublic.apk`, `ChromeModernPublic.apk`, `MonochromePublic.apk`, `SystemWebview.apk` are tracked.
   * But only `MonochromePublic.apk` is actively monitored.
 * We care most about on-disk size (for users with small device storage)
   * But also care about patch size (so that browser updates get applied)

### Normalized APK Size

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=d6542096534166992e063320f8e1b7128e10ed53091e865eef3b5295644e60ce)
 * Monitored by [Binary Size Sheriffs](../apk_size_regressions.md).
   * Alerts fire for changes of 16kb or more.
 * Computed as:
   * The size of an APK
   * With all native code as the sum of section sizes (except .bss), uncompressed.
   * With all dex code as if it were stored uncompressed.
   * With all translations as if they were not missing (estimates size of missing translations based on size of english strings).
     * Without translation-normalization, translation dumps cause jumps.
     * Translation-normalization applies only to apks (not to Android App Bundles).

### Native Code Size Metrics

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=7b392aa248c77bd4c4fe03ca870e30863e3dcb7f06167cb50c9d7d99010687a9)
 * File size of `libchrome.so`
 * Code vs data sections (.text vs .data + .rodata)

### Java Code Size Metrics

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=d2f2a1dfefd89c43902299efeddf6e4e6521db5e01d3716b8202f6ad8ad960da)
 * File size of `classes.dex`
 * "Dex": Counts of the number of fields, methods, strings, types.
 * "DexCache": The number of bytes of RAM required to load our dex file (computed from counts)

### Breakdown Metrics

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=c7f4afc0f438e7868b81be12c44aca7d0e9f7379bf1ae862df261fcd28d222f1)
 * Compressed size of each apk component.
 * The sum of these equals APK Size (which can be found under "Install Size Metrics": "APK Size")

### Install Metrics

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=33515ace819bc607a742c8748316ffe6a36b3fcbc598efd35cd80d0a83c190ae)
 * Estimated installed size: How much disk is required to install Chrome on a device.
   * This is just an estimate, since actual size depends on Android version (e.g. Dex overhead is very different for pre-L vs L+).
   * Does not include disk used up by profile, etc.
   * We believe it to be fairly accurate for all L+ devices (less space is required for Dalvik runtime pre-L).
 * The sum of these equals Estimated installed size.

### Transfer Size Metrics

 * Deflated apk size:
   * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=c7dcbe09dee57f6dab19f9307acd97a044a150710357ad25bf217ce004b3b4bb)
   * Only relevant for non-patch updates of Chrome (new installs, or manual app updates)
 * Patch Size:
   * Uses [https://github.com/googlesamples/apk-patch-size-estimator](https://github.com/googlesamples/apk-patch-size-estimator)
   * No longer runs:
     * Is too slow to be running on the Perf Builder
     * Was found to be fairly unactionable
     * Can be run manually: `build/android/resource_sizes.py --estimate-patch-size out/Release/apks/ChromePublic.apk`

### Uncompressed Metrics

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=33f59871f4e9fa3d155be3c13a068d35e6e621bcc98d9b7b103e0c8485e21097)
 * Uncompressed size of classes.dex, locale .pak files, etc
 * Reported only for things that are compressed within the .apk

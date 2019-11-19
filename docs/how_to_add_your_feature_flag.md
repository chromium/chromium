# Adding a new feature flag in chrome://flags

This document describes how to add your new feature behind a flag.  See also
[Configuration](configuration.md), which gives more explanation about flags and
other options for configuring Chrome.

## Step 1: Adding a new `base::Feature`
This step would be different between where you want to use the flag.
For example, if you want to use the flag in src/content, you should add a base::Feature to the following files:

* [content/public/common/content_features.cc](https://cs.chromium.org/chromium/src/content/public/common/content_features.cc)
* [content/public/common/content_features.h](https://cs.chromium.org/chromium/src/content/public/common/content_features.h)

If you want to use the flag in blink, you should also read
[Runtime Enable Features](https://www.chromium.org/blink/runtime-enabled-features).

You can refer to [this CL](https://chromium-review.googlesource.com/c/554510/) and [this document](https://chromium.googlesource.com/chromium/src/+/HEAD/content/child/InitializeBlinkFeatures.md)
to see

1. where to add the base::Feature
[[1](https://chromium-review.googlesource.com/c/554510/8/content/public/common/content_features.cc#253)]
[[2](https://chromium-review.googlesource.com/c/554510/8/content/public/common/content_features.h)]
2. how to use it
[[1](https://chromium-review.googlesource.com/c/554510/8/content/common/service_worker/service_worker_utils.cc#153)]
3. how to wire your new base::Feature to a blink runtime feature[[1](https://chromium.googlesource.com/chromium/src/+/HEAD/content/child/InitializeBlinkFeatures.md)]
4. how to use it in blink
[[1](https://chromium-review.googlesource.com/c/554510/8/third_party/blnk/renderere/core/workers/worker_thread.cc)]

Also, this patch added a virtual test for running web tests with the flag.
When you add a flag, you can consider to use that.

## Step 2: Adding the feature flag to the chrome://flags UI.
You have to modify these five files in total.

* [chrome/browser/about_flags.cc](https://cs.chromium.org/chromium/src/chrome/browser/about_flags.cc)
* [chrome/browser/flag_descriptions.cc](https://cs.chromium.org/chromium/src/chrome/browser/flag_descriptions.cc)
* [chrome/browser/flag_descriptions.h](https://cs.chromium.org/chromium/src/chrome/browser/flag_descriptions.h)
* [tools/metrics/histograms/enums.xml](https://cs.chromium.org/chromium/src/tools/metrics/histograms/enums.xml)
* [chrome/browser/flag-metadata.json](https://cs.chromium.org/chromium/src/chrome/browser/flag-metadata.json)

At first you need to add an entry to __about_flags.cc__,
__flag_descriptions.cc__ and __flag_descriptions.h__. After that, try running the following test.

```bash
# Build unit_tests
autoninja -C out/Default unit_tests
# Run AboutFlagsHistogramTest.CheckHistograms
./out/Default/unit_tests --gtest_filter=AboutFlagsHistogramTest.CheckHistograms
# Run AboutFlagsHistogramTest.CheckHistograms on Android
./out/Default/bin/run_unit_tests --gtest_filter=AboutFlagsHistogramTest.CheckHistograms
```

That test will ask you to add several entries to enums.xml.
You can refer to [this CL](https://chromium-review.googlesource.com/c/593707) as an example.

Finally, run the following test.

```bash
./out/Default/unit_tests --gtest_filter=AboutFlagsTest.EveryFlagHasMetadata
```

That test will ask you to update the flag expiry metadata in
[flag-metadata.json](https://cs.chromium.org/chromium/src/chrome/browser/flag-metadata.json).

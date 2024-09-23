# Adding a new feature flag in chrome://flags

This document describes how to add a new Chrome feature flag visible to users
via `chrome://flags` UI.

*** note
**NOTE:** It's NOT required if you don't intend to make your feature appear in
`chrome://flags` UI.
***

See also the following for definitions:
*  [Configuration: Features](configuration.md#features)
*  [Configuration: Flags](configuration.md#flags)

## Step 1: Adding a new `base::Feature`

*** note
**NOTE:** All files mentioned in Step 1 require the features to be listed in alphabetical order.
***

This step would be different depending on where you want to use the flag:

### To use the Flag in `content/` and its embedders

Add a `base::Feature` to the following files:

* [content/public/common/content_features.cc](https://cs.chromium.org/chromium/src/content/public/common/content_features.cc)
* [content/public/common/content_features.h](https://cs.chromium.org/chromium/src/content/public/common/content_features.h)

### To use the Flag in `content/` Only

Add a `base::Feature` to the following files:

* [content/common/features.cc](https://cs.chromium.org/chromium/src/content/common/features.cc)
* [content/common/features.h](https://cs.chromium.org/chromium/src/content/common/features.h)

### To Use the Flag in `third_party/blink/` (and Possibly in `content/`)

Add a `base::Feature` to the following files:

* [third_party/blink/common/features.cc](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/features.cc)
* [third_party/blink/public/common/features.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/features.h)

Historically, Blink also has its own runtime feature mechanism. So if you
feature needs to be runtime-enabled, read also Blink's
[Runtime Enable Features][blink-rte] doc and
[Initialization of Blink runtime features in content layer][blink-rte-init].

[blink-rte]: ../third_party/blink/renderer/platform/RuntimeEnabledFeatures.md

### Examples

You can refer to [this CL](https://chromium-review.googlesource.com/c/554510/)
and [this document](initialize_blink_features.md) to see

1. Where to add the `base::Feature`:
   [[1](https://chromium-review.googlesource.com/c/554510/8/content/public/common/content_features.cc#253)]
   [[2](https://chromium-review.googlesource.com/c/554510/8/content/public/common/content_features.h)]
2. How to use it:
   [[1](https://chromium-review.googlesource.com/c/554510/8/content/common/service_worker/service_worker_utils.cc#153)]
3. How to wire your new `base::Feature` to a Blink runtime feature:
   [[1][blink-rte-init]]
4. How to use it in Blink:
   [[1](https://chromium-review.googlesource.com/c/554510/8/third_party/blnk/renderere/core/workers/worker_thread.cc)]

Also, this patch added a virtual test for running web tests with the flag.
When you add a flag, you can consider to use that.

[blink-rte-init]: initialize_blink_features.md

## Step 2: Adding the feature flag to the chrome://flags UI.

*** promo
Googlers: Read also [Chrome Feature Flag in chrome://flags](http://go/finch-feature-api#chrome-feature-flag-in-chromeflags).
***

*** promo
**Tip:** Android WebView has its own flag UI. The WebView team recommends adding
your features there too if they are supported on WebView. Follow
[these steps](/android_webview/docs/developer-ui.md#Adding-your-flags-and-features-to-the-UI)
for WebView flags.
***

You have to modify these five files in total.

* [chrome/browser/about_flags.cc](https://cs.chromium.org/chromium/src/chrome/browser/about_flags.cc) (Add your changes at the bottom of the list)
* [chrome/browser/flag_descriptions.cc](https://cs.chromium.org/chromium/src/chrome/browser/flag_descriptions.cc) (Features should be alphabetically sorted)
* [chrome/browser/flag_descriptions.h](https://cs.chromium.org/chromium/src/chrome/browser/flag_descriptions.h) (Features should be alphabetically sorted)
* [tools/metrics/histograms/enums.xml](https://cs.chromium.org/chromium/src/tools/metrics/histograms/enums.xml)
* [chrome/browser/flag-metadata.json](https://cs.chromium.org/chromium/src/chrome/browser/flag-metadata.json)

At first you need to add an entry to __about_flags.cc__,
__flag_descriptions.cc__ and __flag_descriptions.h__. After that, try running
the following script which will update enums.xml:

```bash
# Updates enums.xml
./tools/metrics/histograms/generate_flag_enums.py --feature <your awesome feature>
# Run AboutFlagsHistogramTest.CheckHistograms to verify enums.xml
./out/Default/unit_tests --gtest_filter=AboutFlagsHistogramTest.CheckHistograms
# Run AboutFlagsHistogramTest.CheckHistograms on Android to verify enums.xml
./out/Default/bin/run_unit_tests --gtest_filter=AboutFlagsHistogramTest.CheckHistograms
```

*** note
**NOTE:** If CheckHistograms returns an error, it will ask you to add several
entries to enums.xml. After doing so, run `git cl format` which will insert the
entries in enums.xml in the correct order and run the tests again. You can refer
to [this CL](https://chromium-review.googlesource.com/c/593707) as an example.
***

Finally, run the following test.

```bash
./out/Default/unit_tests --gtest_filter=AboutFlagsTest.EveryFlagHasMetadata
```

That test will ask you to update the flag expiry metadata in
[flag-metadata.json](https://cs.chromium.org/chromium/src/chrome/browser/flag-metadata.json).

## Removing the feature flag.

When a feature flag is no longer used it should be removed. Once it has reached it's final state it
can be removed in stages.

First remove the flag from the UI:
* [chrome/browser/about_flags.cc](https://cs.chromium.org/chromium/src/chrome/browser/about_flags.cc)
* [chrome/browser/flag_descriptions.cc](https://cs.chromium.org/chromium/src/chrome/browser/flag_descriptions.cc)
* [chrome/browser/flag_descriptions.h](https://cs.chromium.org/chromium/src/chrome/browser/flag_descriptions.h)
* [chrome/browser/flag-metadata.json](https://cs.chromium.org/chromium/src/chrome/browser/flag-metadata.json)
* Do not edit enums.xml. Keep the flag for archeological purposes.

Once there is no way to change the flag value, it's usage can be removed from the code.

Finally, once the flag is no longer referenced, it can be removed from content/ and
third_party/blink/

## Related Documents

* [Chromium Feature API & Finch (Googler-only)](http://go/finch-feature-api)
* [Configuration: Prefs, Settings, Features, Switches & Flags](configuration.md)
* [Runtime Enabled Features](../third_party/blink/renderer/platform/RuntimeEnabledFeatures.md)
* [Initialization of Blink runtime features in content layer](initialize_blink_features.md)

# Integrating a feature with the Origin Trials framework

To expose your feature via the [Origin Trials framework], there are a few code
changes required.

*** note
**WARNING:** This is only available for features implemented in Blink.
***

[TOC]

## Code Changes

*** promo
**NOTE:** You can land these code changes before requesting to run an origin
trial.
These code changes make it possible to control a feature via an origin trial,
but don't require an origin trial to be approved. For more on the process, see
[Running an Origin Trial].
***

### Step 1: Add Runtime Enabled Feature in Blink for Origin Trial

First, you’ll need to configure [`runtime_enabled_features.json5`]. If you don't
have a Blink's [Runtime Enabled Feature] flag yet, you will need to add an entry
in this file.

The following fields of an entry are relevant:

- `name`: The name of your runtime enabled feature, e.g. `"MyFeature"`.
- `origin_trial_feature_name`: The name of your runtime enabled feature in the
  origin trial. This can be the same as your runtime feature flag (i.e. `name`
  field), or different. Eventually, this configured name will be used in the
  origin trials developer console.
- `origin_trial_os`: Specifies a `[list]` of platforms where they will allow the
  trial to be enabled. The list values are case-insensitive, but must match one
  of the defined `OS_<platform>` macros (see [`build_config.h`]).
- `origin_trial_allows_third_party`: Must be enabled to allow third-party tokens
  to work correctly. Set to true, if (and only if) you intend to support
  third-party matching.
- `base_feature`: Generates a `base::Feature` in the `blink::features`
  namespace if the value is not `"none"`. It helps to control the Origin Trial
  remotely. See also [Generate a `base::Feature` instance from a Blink Feature].

Not specific to Origin Trial:

- `status`: Controls when the runtime enabled feature is enabled in Blink. See
  also [the Status table].
- `base_feature_status`: Controls when the `base::Feature` defined by
  `base_feature` is enabled.

More details are explained in the json5 file and in the above linked doc.

If the runtime enabled feature flag is [used in C++](#1-in-c), you will have to
change all callers of the no-argument overload of
`RuntimeEnabledFeatures::MyFeatureEnabled()` to the overload that takes a
`const FeatureContext*`. You can pass an `ExecutionContext` here, e.g. using
`ExecutionContext::From(ScriptState*)`.

#### Examples

RuntimeEnabledFeature flag name, trial name and `base::Feature` are all the
same:

```json
{
  name: "MyFeature",  // Generates `RuntimeEnabledFeatures::MyFeatureEnabled()`
  origin_trial_feature_name: "MyFeature",
  status: "experimental",
  // No need to specify base_feature.
},
```

RuntimeEnabledFeature flag name, trial name, and `base::Feature` name are
different:

```json
{
  name: "MyFeature",
  origin_trial_feature_name: "MyFeatureTrial",
  base_feature: "MyBaseFeature",  // Generates blink::features::kMyBaseFeature
  status: "experimental",
},
```

Trial limited to specific platform:

```json
{
  name: "MyFeature",
  origin_trial_feature_name: "MyFeature",
  origin_trial_os: ["android"],
  status: "experimental",
},
```

#### WebView considerations

Because WebView is built as part of the `"android"` os target, it is not
possible to exclude a trial from WebView if it is enabled on Android.

If the feature under trial can be enabled on WebView alongside other Android
platforms, this is preferred.

In situations where this is not feasible, the recommended solution is to
explicitly disable the origin trial in
`AwMainDelegate::BasicStartupComplete()` in [`aw_main_delegate.cc`] by
appending the `embedder_support::kOriginTrialDisabledFeatures` switch with the
disabled trial names as values.

See https://crrev.com/c/3733267 for an example of how this can be done.

### Step 2: Gating Access

Once configured, there are two mechanisms to gate access to your feature behind
an origin trial. You can use either mechanism, or both, as appropriate to your
feature implementation.

#### 1) In C++

A native C++ method that you can call in Blink code at runtime to expose your
feature:

```cpp
bool RuntimeEnabledFeatures::MyFeatureEnabled(ExecutionContext*)
```

*** note
**WARNING:** Your feature implementation must not persist the result of the
enabled check. Your code should simply call
`RuntimeEnabledFeatures::MyFeatureEnabled(ExecutionContext*)` as often as
necessary to gate access to your feature.
***

#### 2-1) In Web IDL

An IDL attribute \[[RuntimeEnabled]\] that you can use to automatically generate
code to expose and hide JavaScript methods/attributes/objects.

```cpp
[RuntimeEnabled=MyFeature]
partial interface Navigator {
     readonly attribute MyFeatureManager myFeature;
}
```

#### 2-2) CSS Properties

*** promo
**NOTE:** For CSS properties, you do not need to edit the IDL files, as the
exposure on the [CSSStyleDeclaration] is handled at runtime.
***

You can also run experiment for new CSS properties with origin trial. After you
have configured your feature in [`runtime_enabled_features.json5`] as above,
head to [`css_properties.json5`]. As explained in the file, you use
`runtime_flag` to associate the CSS property with the feature you just defined.
This will automatically link the CSS property to the origin trial defined in the
runtime feature. It will be available in both JavaScript (`Element.style`) and
CSS (including `@supports`) when the trial is enabled.

*** promo
**EXAMPLE:** [origin-trial-test-property] defines a test css property controlled
via runtime feature `OriginTrialsSampleAPI` and subsequently an origin trial
named `Frobulate`.
***

*** note
**ISSUE:** In the rare cases where the origin trial token is added via script
after the css style declaration, the css property will be enabled and is fully
functional, however it will not appear on the [CSSStyleDeclaration] interface,
i.e. not accessible in `Element.style`. This issue is tracked in crbug/1041993.
***

### Step 3: Mapping Runtime Enabled Feature to `base::Feature` (optional)

Given the following example:

```json
{
  name: "MyFeature",
  origin_trial_feature_name: "MyFeature",
  base_feature: "MyFeature",
  status: "experimental",
},
```

```cpp
[RuntimeEnabled=MyFeature]
interface MyFeatureAPI {
  readonly attribute bool dummy;
}
```

```cpp
// third_party/blink/.../my_feature_api.cc
bool MyFeatureAPI::ConnectToBrowser() {
  if (base::FeatureList::IsEnabled(blink::features::kMyFeature) {
    // Do something
  }
  return false;
}
```

The above example shows a new feature relies on a `base::Feature` generated from
the `base_feature` definition in json file, e.g. `blink::features::kMyFeature`,
in addition to the runtime enabled feature flag `MyFeature`.
However, their values are not associated.

In addition, due to the [limitation](#limitations), the runtime enabled feature
flag is not available in the browser process **by default**:

> if you need to know in the browser process whether a feature should
> be enabled, then you will have to either have the renderer inform it at
> runtime, or else just assume that it's always enabled, and gate access to the
> feature from the renderer.

*** note
**TLDR:** Turning on `MyFeature` doesn't automatically turning on
`blink::features::kMyFeature`, and vice versa.
***

To mitigate the issue, there are several options:

#### Option 1: Fully Enabling `base::Feature`, e.g. `kMyFeature`

And letting Origin Trial decide when your feature (via runtime enabled feature
flag `blink::features::MyFeature`) is available, as suggested in the above
quote. The `base::Feature` can be enabled via a remote Finch config, or by
updating the default value in C++.

However, after the Origin Trial ends, it will be impossible to ramp up the
feature by Finch if the part controlled by `MyFeature` cannot be enabled
independently. For example, if you have a new Web API `MyFeatureAPI`, enabling
`MyFeature` will just make the IDL available to everyone without the
Blink/browser implementation.

*** note
**Example Bug:** https://crbug.com/1360678.
***

#### Option 2: Setting Up a Custom Mapping

1. Make `MyFeature` depend on `blink::features::kMyFeature` so that the feature
   is not enabled if `features::kMyFeatures` is not enabled. In
   [third_party/blink/renderer/core/origin_trials/origin_trial_context.cc](/third_party/blink/renderer/core/origin_trials/origin_trial_context.cc):

    ```cpp
    bool OriginTrialContext::CanEnableTrialFromName(const StringView& trial_name) {
      ...
      if (trial_name == "MyFeature") {
        return base::FeatureList::IsEnabled(blink::features::kMyFeatures);
      }
    }
    ```

2. Add custom relationship for `MyFeature` and `blink::features::kMyFeature` to
   handle your use case.

    Read
    [**Determine how your feature is initialized: Depends on the status of a base::Feature**](initialize_blink_features.md#step-2_determine-how-your-feature-is-initialized)
    first. If the mappings described there don't meet your use case, refer to
    the following examples.

    In [content/child/runtime_features.cc](https://source.chromium.org/chromium/chromium/src/+/main:content/child/runtime_features.cc):

    ```cpp
    void SetCustomizedRuntimeFeaturesFromCombinedArgs(
        const base::CommandLine& command_line) {
      // Example 1: https://bit.ly/configuring-trust-tokens
      // Example 2: https://crrev.com/c/3878922/14/content/child/runtime_features.cc
    }
    ```

### Step 4: Web Feature Counting

Once the feature is created, in order to run the origin trial you need to track
how often users use your feature. You can do it in two ways.

#### Increment counter in your C++ code

1. Add your feature counter to the end of [`webdx_feature.mojom`] (or
   [`web_feature.mojom`] if it's a feature that's somehow not expected to be
   described in the [web platform dx
   repository](https://github.com/web-platform-dx/web-features/))"

    ```cpp
    enum WebDXFeature {
      // ...
      kLastFeatureBeforeYours = 1235,
      // Here, increment the last feature count before yours by 1.
      kMyFeature = 1236,

      kNumberOfFeatures,  // This enum value must be last.
    };
    ```

2. Run [`update_use_counter_feature_enum.py`] to update the UMA mappings.

3. Increment your feature counter in C++ code.

    ```cpp
    #include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

    // ...

      if (RuntimeEnabledFeatures::MyFeatureEnabled(context)) {
        UseCounter::Count(context, WebFeature::kMyFeature);
      }
    ```

#### Update counter with \[MeasureAs\] IDL attribute

1. Add \[[MeasureAs="WebDXFeature::kMyFeature"]\] IDL attribute

    ```cpp
    partial interface Navigator {
      [RuntimeEnabled=MyFeature, MeasureAs="WebDXFeature::kMyFeature"]
      readonly attribute MyFeatureManager myFeature;
    ```

   Alternatively, if your feature counter doesn't fit as a WebDXFeature use
   counter, make it a WebFeature instead and drop the WebDXFeature:: prefix (and
   quotes) in the \[[MeasureAs]\] attribute above, or use \[[Measure]\] instead
   and follow the \[[Measure]\] IDL attribute naming convention.

2. Add your use counter to [`webdx_feature.mojom`] (or alternatively to
   [`web_feature.mojom`]). The code to increment your feature counter will be
   generated in the V8 bindings code automatically.

    ```cpp
    enum WebDXFeature {
      // ...
      kLastFeatureBeforeYours = 1235,
      // Here, increment the last feature count before yours by 1.
      kMyFeature = 1236,

      kNumberOfFeatures,  // This enum value must be last.
    };
    ```

### Step 5: Add Web Tests

When using the \[[RuntimeEnabled]\] IDL attribute, you should add web tests
to verify that the V8 bindings code is working as expected. Depending on how
your feature is exposed, you'll want tests for the exposed interfaces, as well
as tests for script-added tokens. For examples, refer to the existing tests in
[origin_trials/webexposed].

## Limitations

What you can't do, because of the nature of these origin trials, is know at
either browser or renderer startup time whether your feature is going to be used
in the current page/context. This means that if you require lots of expensive
processing to begin (say you index the user's hard drive, or scan an entire city
for interesting weather patterns,) that you will have to either do it on browser
startup for *all* users, just in case it's used, or do it on first access. (If
you go with first access, then only people trying the experiment will notice the
delay, and hopefully only the first time they use it.). We are investigating
providing a method like `OriginTrials::myFeatureShouldInitialize()` that will
hint if you should do startup initialization.  For example, this could include
checks for trials that have been revoked (or throttled) due to usage, if the
entire origin trials framework has been disabled, etc.  The method would be
conservative and assume initialization is required, but it could avoid expensive
startup in some known scenarios.

Similarly, if you need to know in the browser process whether a feature should
be enabled, then you will have to either have the renderer inform it at runtime,
or else just assume that it's always enabled, and gate access to the feature
from the renderer.

## Manual Testing

To test an origin trial feature during development, follow these steps:

1. Use [`generate_token.py`] to generate a token signed with the test private key.
   You can generate signed tokens for any origin that you need to help you test,
   including localhost or 127.0.0.1. Example:

    ```bash
    tools/origin_trials/generate_token.py http://localhost:8000 MyFeature
    ```

   There are additional flags to generate third-party tokens, set the expiry
   date, and control other options. See the command help for details (`--help`).
   For example, to generate a third-party token, with [user subset exclusion]:

    ```bash
    tools/origin_trials/generate_token.py --is-third-party --usage-restriction=subset http://localhost:8000 MyFeature
    ```

2. Copy the token from the end of the output and use it in a `<meta>` tag or
   an `Origin-Trial` header as described in the [Developer Guide].

3. Run Chrome with the test public key by passing:
   `--origin-trial-public-key=dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=`

You can also run Chrome with both the test public key and the default public key
along side by passing:
`--origin-trial-public-key=dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=,fMS4mpO6buLQ/QMd+zJmxzty/VQ6B1EUZqoCU04zoRU=`

*** promo
**TIP:** See
[this doc](https://www.chromium.org/developers/how-tos/run-chromium-with-flags/)
to apply commandline switches for Chrome for Android and
[this doc](/android_webview/docs/commandline-flags.md) to apply commandline
switches for Android WebView.
***

The `--origin-trial-public-key` switch is not needed with `content_shell`, as it
uses the test public key by default.

The test private key is stored in the repo at `tools/origin_trials/eftest.key`.
It's also used by Origin Trials unit tests and web tests.

If you cannot set command-line switches (e.g., on Chrome OS), you can also
directly modify [`chrome_origin_trial_policy.cc`].

To see additional information about origin trial token parsing (including
reasons for failures, or token names for successful tokens), you can add these
switches:

  `--vmodule=trial_token=2,origin_trial_context=1`

If you are building with `is_debug=false`, then you will also need to add
`dcheck_always_on=true` to your build options, and add this to the command line:

  `--enable-logging=stderr`

## Related Documents

- [Chromium Feature API & Finch (Googler-only)](http://go/finch-feature-api)
- [Configuration: Prefs, Settings, Features, Switches & Flags](configuration.md)
- [Runtime Enabled Features]
- [Initialization of Blink runtime features in content layer](initialize_blink_features.md)

[Origin Trials framework]: https://googlechrome.github.io/OriginTrials/developer-guide.html
[Runtime Enabled Feature]: /third_party/blink/renderer/platform/RuntimeEnabledFeatures.md
[Runtime Enabled Features]: /third_party/blink/renderer/platform/RuntimeEnabledFeatures.md
[Generate a `base::Feature` instance from a Blink Feature]: /third_party/blink/renderer/platform/RuntimeEnabledFeatures.md#generate-a-instance-from-a-blink-feature
[the Status table]: /third_party/blink/renderer/platform/RuntimeEnabledFeatures.md#adding-a-runtime-enabled-feature
[`build_config.h`]: /build/build_config.h
[`chrome_origin_trial_policy.cc`]: /chrome/common/origin_trials/chrome_origin_trial_policy.cc
[`generate_token.py`]: /tools/origin_trials/generate_token.py
[Developer Guide]: https://github.com/jpchase/OriginTrials/blob/gh-pages/developer-guide.md
[RuntimeEnabled]: /third_party/blink/renderer/bindings/IDLExtendedAttributes.md#RuntimeEnabled
[origin_trials/webexposed]: /third_party/blink/web_tests/http/tests/origin_trials/webexposed/
[`runtime_enabled_features.json5`]: /third_party/blink/renderer/platform/runtime_enabled_features.json5
[`webdx_feature.mojom`]: /third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom
[`web_feature.mojom`]: /third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom
[`update_use_counter_feature_enum.py`]: /tools/metrics/histograms/update_use_counter_feature_enum.py
[Measure]: /third_party/blink/renderer/bindings/IDLExtendedAttributes.md#Measure
[`css_properties.json5`]: /third_party/blink/renderer/core/css/css_properties.json5
[origin-trial-test-property]: https://chromium.googlesource.com/chromium/src/+/ff2ab8b89745602c8300322c2a0158e210178c7e/third_party/blink/renderer/core/css/css_properties.json5#2635
[CSSStyleDeclaration]: /third_party/blink/renderer/core/css/css_style_declaration.idl
[Running an Origin Trial]: https://www.chromium.org/blink/origin-trials/running-an-origin-trial
[user subset exclusion]: https://docs.google.com/document/d/1xALH9W7rWmX0FpjudhDeS2TNTEOXuPn4Tlc9VmuPdHA/edit#heading=h.myaz1twlipw
[`aw_main_delegate.cc`]: /android_webview/lib/aw_main_delegate.cc

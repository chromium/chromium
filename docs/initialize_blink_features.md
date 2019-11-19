# Initialization of Blink runtime features in content layer
This document outlines how to initialize your Blink runtime features in the
Chromium content layer, more specifically in
[content/child/runtime_features.cc][runtime_features]. To learn more on how to
set up features in blink, see
[Runtime Enabled Features][RuntimeEnabledFeatures].

## Step 1: Do you need a custom Blink feature enabler function?
If you simply need to enable/disable the Blink feature you can simply use
[WebRuntimeFeatures::EnableFeatureFromString()][EnableFeatureFromString].

However, if there are side effects (e.g. you need to disable other features if
this feature is also disabled), you should declare a custom enabler function in
- [third_party/blink/public/platform/web_runtime_features.h][WebRuntimeFeatures.h]
- [third_party/blink/public/platform/web_runtime_features.cc][WebRuntimeFeatures.cc]

## Step 2: Determine how your feature is initialized.
### 1) Depends on OS-specific Macros:
Add your code for controlling the Blink feature in
[SetRuntimeFeatureDefaultsForPlatform()][SetRuntimeFeatureDefaultsForPlatform]
using the appropriate OS macros.
### 2) Depends on the status of a base::Feature:
Add your code to the function
[SetRuntimeFeaturesFromChromiumFeatures()][SetRuntimeFeaturesFromChromiumFeatures].

If your Blink feature has a custom enabler function, add a new entry to
`blinkFeatureToBaseFeatureMapping`. For example, a new entry like this:
```
{wf::EnableNewFeatureX, features::kNewFeatureX, kEnableOnly},
```
will call `wf::EnableNewFeatureX` to enable it only if `features::kNewFeatureX`
is enabled.

If your Blink feature does not have a custom enabler function, you need to add
the entry to `runtimeFeatureNameToChromiumFeatureMapping`. For example, a new
entry like this:
```
{"NewFeatureY", features::kNewFeatureY, kUseFeatureState},
```
will call `wf::EnableFeatureFromString` with your feature name to set it to
whichever state your `features::kNewFeatureY` is in.

For more detailed explanation on the options you have, read the comment in enum
[RuntimeFeatureEnableOptions][EnableOptions].
### 3) Set by a command line switch to enable or disable:
Add your code to the function
[SetRuntimeFeaturesFromCommandLine()][SetRuntimeFeaturesFromCommandLine].

If your Blink feature has a custom enabler function, add a new entry to
`switchToFeatureMapping`. For example, a new entry like this:
```
{wrf::EnableNewFeatureX, switches::kNewFeatureX, false},
```
will call `wf::EnableNewFeatureX` to disable it only if that
`switches::kNewFeatureX` exists on the command line.

### 4) Controlled by parameters from a field trial:
Add your code to the function
[SetRuntimeFeaturesFromFieldTrialParams()][SetRuntimeFeaturesFromFieldTrialParams].

### 5) Combination of the previous options or not covered:
For example, you Blink feature could be controlled by both a base::Feature and a
command line switch. In this case, your custom logic should live here in
[`SetCustomizedRuntimeFeaturesFromCombinedArgs()`][SetCustomizedRuntimeFeaturesFromCombinedArgs].


[EnableOptions]:<https://chromium.googlesource.com/chromium/src/+/HEAD/content/child/runtime_features.cc#135>
[runtime_features]:<https://chromium.googlesource.com/chromium/src/+/HEAD/content/child/runtime_features.cc>
[RuntimeEnabledFeatures]:
<https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/platform/RuntimeEnabledFeatures.md>
[WebRuntimeFeatures.h]:
<https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/platform/exported/web_runtime_features.h>
[WebRuntimeFeatures.cc]:
<https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/platform/exported/web_runtime_features.cc>
[EnableFeatureFromString]:<https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/public/platform/web_runtime_features.h#56>
[SetRuntimeFeatureDefaultsForPlatform]:<https://chromium.googlesource.com/chromium/src/+/HEAD/content/child/runtime_features.cc#46>
[SetCustomizedRuntimeFeaturesFromCombinedArgs]:<https://chromium.googlesource.com/chromium/src/+/HEAD/content/child/runtime_features.cc#487>
[SetRuntimeFeaturesFromChromiumFeatures]:<https://chromium.googlesource.com/chromium/src/+/HEAD/content/child/runtime_features.cc#160>
[SetRuntimeFeaturesFromCommandLine]:<https://chromium.googlesource.com/chromium/src/+/HEAD/content/child/runtime_features.cc#390>
[SetRuntimeFeaturesFromFieldTrialParams]:<https://chromium.googlesource.com/chromium/src/+/HEAD/content/child/runtime_features.cc#448>
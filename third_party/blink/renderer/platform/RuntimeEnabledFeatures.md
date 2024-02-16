# Runtime Enabled Features
## Overview
Runtime flags enable Blink to control access to new features. Features that are hidden behind a runtime flag are known as Runtime Enabled Features. It is a requirement of the Blink Launch Process to implement new web exposed features behind a runtime flag until an Intent To Ship has been approved. Additionally, all changes with non-trivial compatibility risk [should be guarded](/docs/flag_guarding_guidelines.md) by a Runtime Enabled Feature (or other base::Feature) so that they can be disabled quickly.

## Adding A Runtime Enabled Feature
Runtime Enabled Features are defined in runtime_enabled_features.json5 in alphabetical order. Add your feature's flag to [runtime_enabled_features.json5] and the rest will be generated for you automatically.

Please add a descriptive comment, including a link to the spec or the chromestatus.com entry if either one is available. This allows readers to easily find more context about the feature.

Example:
```js
{
  // Amazing new feature! https://chromestatus.com/feature/123
  name: "AmazingNewFeature",
  status: "experimental",
}
```
The status of the feature controls when it will be enabled in the Blink engine.

| Status Value | Web feature enabled during [web tests] with content_shell [1] | Web feature enabled as part of web experimental features [2] | Web feature enabled in stable release | Non-web exposed feature enabled through a command line flag [3]
|:---:|:---:|:---:|:---:|:---:|
| <missing\> | No | No | No | Yes |
| `test` | Yes | No | No | No |
| `experimental` | Yes | Yes | No | No |
| `stable` | Yes | Yes | Yes | No |

\[1]: content_shell will not enable experimental/test features by default. The `--run-web-tests` flag used as part of running web tests enables this behaviour. The `--enable-blink-test-features` flag also enables this behavior in Chromium and content_shell's browser mode.

\[2]: Navigate to about:flags in the URL bar and turn on "Enable experimental web platform features" (formerly, "Enable experimental WebKit features") **or** run Chromium with `--enable-experimental-web-platform-features` (formerly, --enable-experimental-webkit-features).
Works in all Chromium channels: canary, dev, beta, and stable.

\[3]: For features that are not web exposed features but require code in Blink to be triggered. Such feature can have a about:flags entry or be toggled based on other signals. Such entries should be called out in a comment to differentiate them from stalled entries.

### Platform-specific Feature Status
For features that do not have the same status on every platform, you can specify their status using a dictionary value.

For example in the declaration below:
```js
{
  name: "NewFeature",
  status: {
    "Android": "test",
    "ChromeOS": "experimental",
    "Win": "stable",
    "default": "",
  }
}
```
the feature has the status `test` on Android, `experimental` on Chrome OS and `stable` on Windows and no status on the other platforms.
The status of all the not-specified platforms is set using the `default` key. For example, the declaration:
```js
status: {
  "Android": "stable",
  "default", "experimental",
}
```
will set the feature status to  `experimental` on all platforms except on Android (which will be set to `stable`).

**Note:** Omitting `default` from the status dictionary will be treated the same as writing `"default": ""`.

**Note:** You can find the list of all supported platforms in [runtime_enabled_features.json5 status declaration][supportedPlatforms].

### Guidelines for Setting Feature Status
Any in-development feature can be added with no status, the only requirement is that code OWNERS are willing to have the code landed in the tree (as for any commit).

* For a feature to be marked `status: "test"`, it must be in a sufficient state to permit internal testing.  For example, enabling it should not be known to easily cause crashes, leak memory, or otherwise significantly effect the reliability of bots.  Consideration should also be given to the potential for loss of test coverage of shipping behavior.  For example, if a feature causes a new code path to be taken instead of an existing one, it is possible that some valuable test coverage and regression protection could be lost by setting a feature to `status: "test"`.  Especially, using `status: "test"` for features that have substantially different code paths from the shipped product is strongly discouraged.  Consider using a [virtual test suite] or setting up a [flag-specific] [trybot (example)] when it's important to keep testing both old and new code paths.  [LayoutNG] and [BlinkGenPropertyTrees] are examples of features where we ensured test coverage of both new and old code paths until they were fully launched, without using `status: "test"`.  See the linked document/bug for how we achieved that.

* For a feature to be marked `status: "experimental"`, it should be far enough along to permit testing by early adopter web developers.  Many chromium enthusiasts run with `--enable-experimental-web-platform-features`, and so promoting a feature to experimental status can be a good way to get early warning of any stability or compatibility problems.  If such problems are discovered (e.g. major websites being seriously broken when the feature is enabled), the feature should be demoted back to no status or `status: "test"` to avoid creating undue problems for such users.  It's notoriously difficult to diagnose a bug report from a user who neglects to mention that they have this flag enabled.  Often a feature will be set to experimental status long before it's implementation is complete, and while there is still substantial churn on the API design.  Features in this state are not expected to work completely, just do something of value which developers may want to provide feedback on.

   **Note:** features set to "experimental" should **not** be expected to cause significant breakage of existing major sites. The primary use case is new APIs or features that are not expected to cause compat issues. If your feature could be reasonably expected to cause compat issues, please keep it marked no status or `status:"test"` [4], and instead use the Finch system, which is better suited to detect and disable such features in case of problems.

\[4]: In this case, "no status" is preferred to `status:"test"` unless you can ensure test coverage of the code paths with the feature disabled. See the `status:"test"` section for more details.

* For a feature to be marked `status: "stable"`, it must be complete and ready for use by all chrome users. Often this means it has gotten approval via the [blink launch process]. However, for features which are not web observable (e.g. a flag to track a large-scale code refactoring), this approval is not needed. In rare cases a feature may be tested on canary and dev channels by temporarily setting it to `status: "stable"`, with a comment pointing to a bug marked `Release-Block-Beta` tracking setting the feature back to `status: "experimental"` before the branch for beta.

When a feature has shipped and is no longer at risk of needing to be disabled, its associated RuntimeEnableFeatures entry should be removed entirely.  Permanent features should generally not have flags.

If a feature is not stable and no longer under active development, remove `status: "test"/"experimental"` on it (and consider deleting the code implementing the feature).

### Relationship between a Chromium Feature and a Blink Feature

In some cases, e.g. for finch experiment, you may need to define a Chromium
feature for a blink feature. If you need a Chromium feature just for finch
experiment for a blink feature, see the next section. Otherwise, you should
specify `base_feature: "none"`, and their relationship is defined in
[content/child/runtime_features.cc]. See the [initialize blink features] doc
for more details.

**Note:** `base_feature: "none"` is strongly discouraged if the feature
doesn't have an associated base feature because the feature would lack a
killswitch controllable via finch.

**Note:** If a feature is implemented at both Chromium side and blink side, as the blink feature doesn't fully work by itself, we normally don't set the blink feature's status so that the Chromium feature can fully control the blink feature ([example][controlled by chromium feature]).

If you need to update or check a blink feature status from outside of blink,
with dedicated methods (instead of `WebRuntimeFeatures::EnableFeatureFromString()`),
you can generate methods of `WebRuntimeFeatures` by adding `public: true,` to
the feature entry in `runtime_enabled_features.json5`. This should be
rare because `WebRuntimeFeatures::EnableFeaturesFromString()` works in
most cases.

### Generate a `base::Feature` instance from a Blink Feature

A Blink feature entry generates a corresponding `base::Feature` instance with
the same name in `blink::features` namespace by default.  It's helpful for a
Finch experiment for the feature, including a kill switch.

Specify `base_feature: "AnotherFlagName"` if you'd like to generate a
`base::Feature` with a different name.

Specify `base_feature: "none"` to disable `base::Feature` generation
(see the note above about in what situation `base_feature: "none"` is strongly
discouraged).

The name specified by `base_feature` or `name` is used for the feature
name which is referred in `--enable-features=` flag and Finch configurations.

The generated `base::Feature` is enabled by default if the status of the blink
feature is `stable`, and disabled by default otherwise. This behavior can be
overridden by `base_feature_status` field.

### Introducing dependencies among Runtime Enabled Features

The parameters of `implied_by` and `depends_on` can be used to specify the relationship to other features.

* "implied_by": With this field specified, this feature is enabled automatically if any of the implied_by features is enabled.

* "depends_on": With this field specified, this feature is enabled only if all of the depends_on features are enabled.

**Note:** Only one of `implied_by` and `depends_on` can be specified.

### Runtime Enabled CSS Properties

If your feature is adding new CSS Properties you will need to use the runtime_flag argument in [renderer/core/css/css_properties.json5][cssProperties].

## Using A Runtime Enabled Feature

### C++ Source Code
Add this include:
```cpp
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
```
This will provide following static methods to check/set whether your feature is enabled:
```cpp
bool RuntimeEnabledFeatures::AmazingNewFeatureEnabled();
void RuntimeEnabledFeatures::SetAmazingNewFeatureEnabled(bool enabled);
```
**Note:** MethodNames and  FeatureNames are in UpperCamelCase. This is handled automatically in code generators, and works even if the feature's flag name begins with an acronym such as "CSS", "IME", or "HTML".
For example "CSSMagicFeature" becomes `RuntimeEnabledFeatures::CSSMagicFeatureEnabled()` and `RuntimeEnabledFeatures::SetCSSMagicFeatureEnabled(bool)`.


### IDL Files
Use the [Blink extended attribute] `[RuntimeEnabled]` as in `[RuntimeEnabled=AmazingNewFeature]` in your IDL definition.

**Note:** FeatureNames are in UpperCamelCase; please use this case in IDL files.

You can guard the entire interface, as in this example:
```
[
    RuntimeEnabled=AmazingNewFeature  // Guard the entire interface.
] interface AmazingNewObject {
    attribute DOMString amazingNewAttribute;
    void amazingNewMethod();
};
```
Alternatively, you can guard individual definition members:
```
interface ExistingObject {
    attribute DOMString existingAttribute;
    // Guarded attribute.
    [RuntimeEnabled=AmazingNewFeature] attribute DOMString amazingNewAttribute;
    // Guarded method.
    [RuntimeEnabled=AmazingNewFeature] void amazingNewMethod();
};
```
**Note:** You *cannot* guard individual arguments, as this is very confusing and error-prone. Instead, use overloading and guard the overloads.

For example, instead of:
```
interface ExistingObject {
    foo(long x, [RuntimeEnabled=FeatureName] optional long y); // Don't do this!
};
```
do:
```
interface ExistingObject {
    // Overload can be replaced with optional if [RuntimeEnabled] is removed
    foo(long x);
    [RuntimeEnabled=FeatureName] foo(long x, long y);
};
```

**Warning:** You will not be able to change the enabled state of these at runtime as the V8 object templates definitions are created during start up and will not be updated during runtime.

## Web Tests (JavaScript)

In [web tests], you can test whether a feature is enabled using:
```javascript
internals.runtimeFlags.amazingNewFeatureEnabled
```
This attribute is read only and cannot be changed, unless `settable_from_internals: true` is specified for the feature.

**Note:** The `internals` JavaScript API is only available in content_shell for use by web tests and does not appear in Chromium. In content_shell's browser mode, `--expose-internals-for-testing` is needed to have the `internals` JavaScript API.

**Note:** If your runtime feature is called `AmazingNewFeature`, the Javascript variable name is `internals.runtimeFlags.amazingNewFeatureEnabled`.

### Running Web Tests
When content_shell is run for web tests with `--stable-release-mode` flag, test-only and experimental features (ones listed in [runtime_enabled_features.json5] with `status: "test"` or `status: "experimental"`) are turned off. The [virtual/stable] suite runs with the flag, which is one of the ways to ensure test coverage of production code path for these features.

## Generated Files
[renderer/build/scripts/make_runtime_features.py][make_runtime_features.py] uses [runtime_enabled_features.json5] to generate:
```
<compilation directory>/gen/third_party/blink/renderer/platform/runtime_enabled_features.h
<compilation directory>/gen/third_party/blink/renderer/platform/runtime_enabled_features.cc
```
[renderer/build/scripts/make_internal_runtime_flags.py][make_internal_runtime_flags.py] uses [runtime_enabled_features.json5] to generate:
```
<compilation directory>/gen/third_party/blink/renderer/core/testing/internal_runtime_flags.idl
<compilation directory>/gen/thrid_party/blink/renderer/core/testing/internal_runtime_flags.h
```
[renderer/bindings/scripts/code_generator_v8.py][code_generator_v8.py] uses the generated `internal_runtime_flags.idl` to generate:
```
<compilation directory>/gen/third_party/blink/renderer/bindings/core/v8/v8_internal_runtime_flags.h
<compilation directory>/gen/third_party/blink/renderer/bindings/core/v8/v8_internal_runtime_flags.cc
```
## Command-line Switches
`content` provides two switches which can be used to turn runtime enabled features on or off, intended for use during development. They are exposed by both `content_shell` and `chrome`.
```
--enable-blink-features=SomeNewFeature,SomeOtherNewFeature
--disable-blink-features=SomeOldFeature
```
After applying most other feature settings, the features requested feature settings (comma-separated) are changed. "disable" is applied later (and takes precedence), regardless of the order the switches appear on the command line. These switches only affect Blink's state. Some features may need to be switched on in Chromium as well; in this case, a specific flag is required.

**Announcement**
https://groups.google.com/a/chromium.org/d/msg/blink-dev/JBakhu5J6Qs/re2LkfEslTAJ


[web tests]: <https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_tests.md>
[supportedPlatforms]: <https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/platform/runtime_enabled_features.json5#36>
[cssProperties]: <https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/css/css_properties.json5>
[virtual test suite]: <https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_tests.md#testing-runtime-flags>
[flag-specific]: <https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_tests.md#testing-runtime-flags>
[trybot (example)]: <https://chromium-review.googlesource.com/c/chromium/src/+/1850255>
[LayoutNG]: <https://docs.google.com/document/d/17t6HjA5X8T5xq1LlKoLEGTn_MioGCdEPpijpJeLalK0/edit#heading=h.guvbepjyp0oj>
[BlinkGenPropertyTrees]: <https://crbug.com/836884>
[blink launch process]: <https://www.chromium.org/blink/launching-features>
[Blink extended attribute]: <https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/bindings/IDLExtendedAttributes.md>
[make_runtime_features.py]: <https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/build/scripts/make_runtime_features.py>
[runtime_enabled_features.json5]: <https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/platform/runtime_enabled_features.json5>
[make_internal_runtime_flags.py]: <https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/build/scripts/make_internal_runtime_flags.py>
[code_generator_v8.py]: <https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/bindings/scripts/code_generator_v8.py>
[virtual/stable]: <https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/VirtualTestSuites;drc=9878f26d52d32871ed1c085444196e5453909eec;l=112>
[content/child/runtime_features.cc]: <https://source.chromium.org/chromium/chromium/src/+/main:content/child/runtime_features.cc>
[initialize blink features]: <https://chromium.googlesource.com/chromium/src/+/main/docs/initialize_blink_features.md>
[controlled by chromium feature]: <https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/platform/runtime_enabled_features.json5;drc=70bddadf50a14254072cf7ca0bcf83e4331a7d4f;l=833>

# Deprecation

This README serves as documentation of the way chrome developers can alert web developers to their use of deprecated/removed features.
These alerts can include information on the deprecated feature, alternate features to use, and the removal timeline.
Follow the steps below to dispatch alerts via the [DevTools Issues](https://developer.chrome.com/docs/devtools/issues/) system.

### (1) [WebFeature](/third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom)

This should be named `kFeatureName` and placed at the bottom of the file.
If you have an existing `WebFeature` that can be used instead.

## (2) [deprecation.json5](third_party/blink/renderer/core/frame/deprecation/deprecation.json5)

Add a new dictionary to `data` like follows:
```
{
  name: "FeatureName",
  message: "This string is translated for the user.",
  translation_note: "This provides context to the translator.",
  web_features: [
    "kFeatureName",
  ],
  chrome_status_feature: 0123456789101112,
  milestone: 123,
},
```
Both `chrome_status_feature` and `milestone` are optional.
More than one `web_features` can be added if needed.

## (3) Count the deprecation

Pick one (or both if needed) of the following methods.

### (3a) Call `Deprecation::CountDeprecation`

This function requires a subclass of `ExecutionContext` and your new `WebFeature` to be passed in.
If you're already counting use with an existing `WebFeature`, you should swap `LocalDOMWindow::CountUse` with `ExecutionContext::CountDeprecation` as it will bump the counter for you.
If you only care about cross-site iframes, you can call `Deprecation::CountDeprecationCrossOriginIframe`.

### (3b) Add `DeprecateAs` to the relevant IDL

The [`DeprecateAs`](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/third_party/blink/renderer/bindings/IDLExtendedAttributes.md#DeprecateAs_m_a_c) attribute can be added to the IDL as follows:

```
[DeprecateAs=FeatureName] void myDeprecatedFunction();
```

## (4) Test

Please do not skip this step! Examples can be found in:
(/third_party/blink/web_tests/http/tests/inspector-protocol/issues/deprecation-issue.js)

Tests in this folder can be run like:
```
third_party/blink/tools/run_web_tests.py http/tests/inspector-protocol/issues
```

## (5) Merge steps 1-4 in a `chromium/src` CL

Please tag deprecation-devtool-issues@chromium.org for review.

## (6) Manually roll dependencies from `chromium/src` to `devtools/devtools-frontend`

[Check out](https://chromium.googlesource.com/devtools/devtools-frontend/+/refs/heads/main/docs/workflows.md) `devtools/devtools-frontend` on the same dev machine where you have `chromium/src` checked out.
Check new branch out in `devtools/devtools-frontend`, and run (adjusting directories as needed):
```
./scripts/deps/roll_deps.py ~/chromium/src ~/devtools/devtools-frontend
npm run generate-protocol-resources
```
This pushes the change from (5) into `devtools/devtools-frontend` so you can use it in (9).

## (7) Merge step 6 in a `devtools/devtools-frontend` CL

Please tag deprecation-devtool-issues@chromium.org for review.

## (8) Wait for automatic roll dependencies from `devtools/devtools-frontend` to `chromium/src`

This will be done by the [AutoRoller](https://autoroll.skia.org/r/devtools-frontend-chromium) within a few hours.

## (9) Build Chrome from tip-of-trunk

Verify everything is working as expected.
If something is broken and you can't figure out why, reach out to deprecation-devtool-issues@chromium.org.

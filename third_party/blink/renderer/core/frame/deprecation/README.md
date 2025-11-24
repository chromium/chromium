# Deprecation

This README serves as documentation of the way chrome developers can
alert web developers to their use of deprecated/removed features.
These alerts can include information on the deprecated feature,
alternate features to use, and the removal timeline.
Follow the steps below to dispatch alerts via the
[DevTools Issues](https://developer.chrome.com/docs/devtools/issues/) system.

### (1) [WebFeature](/third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom)

This should be named `kFeatureName` and placed at the bottom of the file.
If you have an existing `WebFeature` that can be used instead.

This step should be skipped for deprecations only reported via the browser process.

## (2) [deprecation.json5](/third_party/blink/renderer/core/frame/deprecation/deprecation.json5)

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

At least one `web_features` must be listed, unless an exemption for browser process only reporting is added via [`EXEMPTED_FROM_RENDERER_GENERATION`](/third_party/blink/renderer/core/frame/deprecation/PRESUBMIT.py).

## (3) Count the deprecation

Pick one (or all if needed) of the following methods.

### (3a) Call `Deprecation::CountDeprecation`

This function requires a subclass of `ExecutionContext` and your new
`WebFeature` to be passed in. If you're already counting use with an existing
`WebFeature`, you should swap `LocalDOMWindow::CountUse` with
`ExecutionContext::CountDeprecation` as it will bump the counter for you. If
you only care about cross-site iframes, you can call
`Deprecation::CountDeprecationCrossOriginIframe`.

### (3b) Add `DeprecateAs` to the relevant IDL

The [`DeprecateAs`](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/third_party/blink/renderer/bindings/IDLExtendedAttributes.md#DeprecateAs_m_a_c) attribute can be added to the IDL as follows:

```
[DeprecateAs=FeatureName] void myDeprecatedFunction();
```

### (3c) Browser Process Reporting

Add the type you need to [`DeprecationIssueType`](/third_party/blink/public/mojom/devtools/inspector_issue.mojom) and [`DeprecationIssueTypeToProtocol`](/content/browser/devtools/devtools_instrumentation.cc), then report the issue by building an `InspectorIssueInfo` and passing it to `RenderFrameHost::ReportInspectorIssue` as this [example](https://chromium-review.googlesource.com/c/chromium/src/+/7165867) does.

## (4) Test

Please do not skip this step! Examples can be found in:
(/third_party/blink/web_tests/http/tests/inspector-protocol/issues/deprecation-issue.js)

Tests in this folder can be run like:
```
third_party/blink/tools/run_web_tests.py http/tests/inspector-protocol/issues
```

For deprecations that require a c++ browsertest to trigger, see this [example](https://chromium-review.googlesource.com/c/chromium/src/+/7165867).

## (5) Merge steps 1-4 in a `chromium/src` CL

Please tag deprecation-devtool-issues@chromium.org for review.

## (6) Wait for automatic roll dependencies from `chromium/src` to `devtools/devtools-frontend`

Roll CLs are created automatically by an infra bot and then manually reviewed by
the DevTools waterfall gardener rotation.

You can find roll CLs [in Gerrit](https://chromium-review.googlesource.com/q/owner:devtools-ci-autoroll-builder@chops-service-accounts.iam.gserviceaccount.com)
with the subject line "Roll browser-protocol".

## (7) Wait for automatic roll dependencies from `devtools/devtools-frontend` to `chromium/src`

This will be done automatically within a few hours, you should see the roll CL
from (6) in `chromium/src/third_party/devtools-frontend`.

## (8) Build Chrome from tip-of-trunk

Verify everything is working as expected.
If something is broken and you can't figure out why, reach out to
deprecation-devtool-issues@chromium.org.

## (9) Mark deprecation as obsolete

Once the deprecation has hit stable and enough time has passed that the message
no longer needs to be dispatched you can mark the metadata as obsolete using
`obsolete_to_be_removed_after_milestone` in
[deprecation.json5](/third_party/blink/renderer/core/frame/deprecation/deprecation.json5).
You should pick a milestone at least 12 past the current branch cut to ensure
the metadata isn't removed when active clients on older versions might still
be dispatching the deprecation message. Please tag
deprecation-devtool-issues@chromium.org for review. See the note in (5) for how to do this.

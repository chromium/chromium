# Deprecation

This README serves as documentation of the way chrome developers can alert web developers to their use of deprecated/removed features.
These alerts can include information on the deprecated feature, alternate features to use, and the removal timeline.
Follow the steps below to dispatch alerts via the [DevTools Issues](https://developer.chrome.com/docs/devtools/issues/) system.

## (1) Add new enum values

The three enums below should have consistent naming, and are all required to implement a new deprecation.

### (1a) [WebFeature](/third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom)

This should be named `kFeatureName` and placed at the bottom of the file.
If you have an existing `WebFeature` that can be used instead.

### (1b) C++ [DeprecationIssueType](/third_party/blink/renderer/core/inspector/inspector_audits_issue.h)

This should be called `kFeatureName`, and the list should be kept alphabetical.

### (1c) Browser Protocol [DeprecationIssueType](/third_party/blink/public/devtools_protocol/browser_protocol.pdl)

This should be called `FeatureName`, and the list should be kept alphabetical.

## (2) Count the deprecation

Pick one (or both if needed) of the following methods.

### (2a) Call `Deprecation::CountDeprecation`

This function requires a subclass of `ExecutionContext` and your new `WebFeature` to be passed in.
If you're already counting use with an existing `WebFeature`, you should swap `LocalDOMWindow::CountUse` with `ExecutionContext::CountDeprecation` as it will bump the counter for you.
If you only care about cross-site iframes, you can call `Deprecation::CountDeprecationCrossOriginIframe`.

### (2b) Add `DeprecateAs` to the relevant IDL

The [`DeprecateAs`](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/third_party/blink/renderer/bindings/IDLExtendedAttributes.md#DeprecateAs_m_a_c) attribute can be added to the IDL as follows:

```
[DeprecateAs=FeatureName] void myDeprecatedFunction();
```

## (3) Update [GetDeprecationInfo](/third_party/blink/renderer/core/frame/deprecation/deprecation.cc)

The new case statement should look like:
```
case WebFeature::kFeatureName:
  return DeprecationInfo::WithTranslation(feature, DeprecationIssueType::kFeatureName);
```

## (4) Update [AuditsIssue::ReportDeprecationIssue](/third_party/blink/renderer/core/inspector/inspector_audits_issue.cc)

The new case statement should look like:
```
case DeprecationIssueType::kFeatureName:
  type = protocol::Audits::DeprecationIssueTypeEnum::FeatureName;
  break;
```

## (5) Test

Please do not skip this step! Examples can be found in:
(/third_party/blink/web_tests/http/tests/inspector-protocol/issues/deprecation-issue.js)

Tests in this folder can be run like:
```
third_party/blink/tools/run_web_tests.py http/tests/inspector-protocol/issues
```

## (6) Merge steps 1-5 in a `chromium/src` CL

Please tag deprecation-devtool-issues@chromium.org for review.

## (7) Manually roll dependencies from `chromium/src` to `devtools/devtools-frontend`

[Check out](https://chromium.googlesource.com/devtools/devtools-frontend/+/refs/heads/main/docs/workflows.md) `devtools/devtools-frontend` on the same dev machine where you have `chromium/src` checked out.
Check new branch out in `devtools/devtools-frontend`, and run (adjusting directories as needed):
```
./scripts/deps/roll_deps.py ~/chromium/src ~/devtools/devtools-frontend
npm run generate-protocol-resources
```
This pushes the change from (1c) into `devtools/devtools-frontend` so you can use it in (9).

## (8) Merge step 7 in a `devtools/devtools-frontend` CL

Please tag deprecation-devtool-issues@chromium.org for review.

## (9) Update [DeprecationIssue](/third_party/devtools-frontend/src/front_end/models/issues_manager/DeprecationIssue.ts)

You'll need to add a new string and [description](https://chromium.googlesource.com/devtools/devtools-frontend/+/refs/heads/main/docs/localization/descriptions.md) to `UIStrings` with your deprecation message, for example:
```
/**
  *@description Additional information for translator on how and when this string is used
  */
  featureName: 'This is the message shown to the web developer in the issue.',
```

You'll also need to handle the new case in `DeprecationIssue::getDescription`, for example:
```
case Protocol.Audits.DeprecationIssueType.FeatureName:
  messageFunction = i18nLazyString(UIStrings.featureName);
  break;
```

If your deprecation has an associated [chrome platform status](https://chromestatus.com/features) and/or [chrome milestone](https://chromiumdash.appspot.com/schedule) be sure to set `feature` and/or `milestone` to include the relevant links, for example:
```
case Protocol.Audits.DeprecationIssueType.FeatureName:
  messageFunction = i18nLazyString(UIStrings.featureName);
  feature = 5684289032159232;
  milestone = 100;
  break;
```

## (10) Test

Please do not skip this step! Examples can be found in:
(/third_party/devtools-frontend/src/test/e2e/issues/deprecation-issues_test.ts)

Tests in this folder can be run like:
```
node scripts/test/run_test_suite.js --test-suite-path=gen/test/e2e --test-suite-source-dir=test/e2e --test-file-pattern=issues/*
```

## (11) Merge steps 9 and 10 in a `devtools/devtools-frontend` CL

Please tag deprecation-devtool-issues@chromium.org for review.

## (12) Wait for automatic roll dependencies from `devtools/devtools-frontend` to `chromium/src`

This will be done by the [AutoRoller](https://autoroll.skia.org/r/devtools-frontend-chromium) within a few hours.

## (13) Build Chrome from tip-of-trunk

Verify everything is working as expected.
If something is broken and you can't figure out why, reach out to deprecation-devtool-issues@chromium.org.

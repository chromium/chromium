# Deprecation

This README serves as documentation of the way chrome developers can alert web developers to their use of deprecated/removed features.
These alerts can include information on the deprecated feature, alternate features to use, and the removal timeline.
Follow the steps below to dispatch alerts via the [DevTools Issues](https://developer.chrome.com/docs/devtools/issues/) system.

## (1) Add new enum values

The three enums below should have consistent naming, and are all required to implement a new deprecation.

### (1a) [WebFeature](/third_party/blink/public/mojom/web_feature/web_feature.mojom)

This should be named `kFeatureName` and placed at the bottom of the file.
If you have an existing `WebFeature` that can be used instead.

### (1b) C++ [DeprecationIssueType](/third_party/blink/renderer/core/inspector/inspector_audits_issue.h)

This should be called `kFeatureName`, and the list should be kept alphabetical.

### (1c) Browser Protocol [DeprecationIssueType](/third_party/blink/public/devtools_protocol/browser_protocol.pdl)

This should be called `FeatureName`, and the list should be kept alphabetical.

## (2) Call `Deprecation::CountDeprecation`

This function requires a subclass of `ExecutionContext` and your new `WebFeature` to be passed in.
If you're already counting use with an existing `WebFeature`, you should swap `LocalDOMWindow::CountUse` with `ExecutionContext::CountDeprecation` as it will bump the counter for you.
If you only care about cross-site iframes, you can call `Deprecation::CountDeprecationCrossOriginIframe`.

## (3) Update [GetDeprecationInfo](/third_party/blink/renderer/core/frame/deprecation/deprecation.cc)

The new case statement should look like:
```
case WebFeature::kFeatureName:
  return DeprecationInfo::WithTranslation(DeprecationIssueType::kFeatureName);
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

## (6) Merge steps 1-5 in a `chromium/src` CL

Please tag deprecation-devtool-issues@chromium.org.

## (7) Manually roll dependencies from `chromium/src` to `devtools/devtools-frontend`

Check out `devtools/devtools-frontend` on the same dev machine where you have `chromium/src` checked out.
Check new branch out in `devtools/devtools-frontend`, and run (adjusting directories as needed):
```
./scripts/deps/roll_deps.py ~/chromium/src ~/devtools/devtools-frontend
npm run generate-protocol-resources
```
This pushes the change from (1c) into `devtools/devtools-frontend` so you can use it in (9).

## (8) Merge step 7 in a `devtools/devtools-frontend` CL

Please tag deprecation-devtool-issues@chromium.org as a reviewer.

## (9) Update [DeprecationIssue](/third_party/devtools-frontend/src/front_end/models/issues_manager/DeprecationIssue.ts)

You'll need to add a new string to `UIStrings` with your deprecation message, for example:
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

## (10) Test

Please do not skip this step! Examples can be found in:
(/third_party/devtools-frontend/src/test/e2e/issues/deprecation-issues_test.ts)

## (11) Merge steps 9 and 10 in a `devtools/devtools-frontend` CL

Please tag deprecation-devtool-issues@chromium.org as a reviewer.

## (12) Wait for automatic roll dependencies from `devtools/devtools-frontend` to `chromium/src`

This will be done by the [AutoRoller](https://autoroll.skia.org/r/devtools-frontend-chromium) within a few hours.

## (13) Build Chrome from tip-of-trunk

Verify everything is working as expected.
If something is broken and you can't figure out why, reach out to deprecation-devtool-issues@chromium.org.

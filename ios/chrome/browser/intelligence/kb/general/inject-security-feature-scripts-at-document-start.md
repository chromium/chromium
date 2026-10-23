---
name: ios/inject-security-feature-scripts-at-document-start
description: When configuring FeatureScript instances that expose private JavaScript APIs or perform security checks, inject at FeatureScript::InjectionTime::kDocumentStart rather than kDocumentEnd
rubric: safety
severity: should_fix
citations:
  - path: ios/web/public/js_messaging/README.md
    heading: FeatureScript::InjectionTime
    quote: Scripts configured with `kDocumentStart` can expose functions, but the DOM will not yet be available. Overrides of functions in the page content world which the webpage may call should be done at this time.
---

**Rule.** When configuring `FeatureScript` instances (via `CreateWithFilename`
or `CreateWithString`) that expose private JavaScript APIs, override page
functions, or perform origin and security checks, specify
`FeatureScript::InjectionTime::kDocumentStart` rather than `kDocumentEnd`.
Reserve `kDocumentEnd` strictly for scripts that require the DOM to be fully
loaded (such as DOM scanning or post-load DOM manipulation).

**Why.** Injecting security checks or private APIs at `kDocumentEnd` allows
untrusted webpage JavaScript to execute before the feature script initializes,
exposing the environment to spoofing, prototype tampering, or unauthorized API
access. Injecting at `kDocumentStart` ensures security verifications and API
exposures run before any page scripts can execute.

## Examples

Bad:

```cpp
// BAD: Injecting private APIs or security verifications at kDocumentEnd
// allows page scripts to execute first.
FeatureScript::CreateWithFilename(
    kScriptName,
    FeatureScript::InjectionTime::kDocumentEnd,
    FeatureScript::TargetFrames::kMainFrame,
    FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)
```

Good:

```cpp
// GOOD: kDocumentStart ensures private APIs and security checks
// are established before untrusted page JavaScript executes.
FeatureScript::CreateWithFilename(
    kScriptName,
    FeatureScript::InjectionTime::kDocumentStart,
    FeatureScript::TargetFrames::kMainFrame,
    FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)
```

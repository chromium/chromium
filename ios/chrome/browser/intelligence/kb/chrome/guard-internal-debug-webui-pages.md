---
name: ios/guard-internal-debug-webui-pages
description: When registering internal debug or diagnostic WebUI pages in iOS Chrome, guard them behind InternalDebugPagesEnabled() returning NewWebUIIOS<InternalDebugPagesDisabledUI> when disabled
rubric: safety
severity: should_fix
citations:
  - path: chrome/browser/ui/webui/indigo_internals/README.md
    heading: Access
    quote: As a debug UI, this page is placed behind the `kInternalOnlyUisEnabled` preference.
---

**Rule.** When registering internal debug or diagnostic WebUI pages in
`ChromeWebUIIOSControllerFactory` (`GetWebUIIOSFactoryFunction`, such as
`chrome://*-internals` or `chrome://interstitials`), guard the factory function
behind `InternalDebugPagesEnabled()`. When the preference is disabled, return
`&NewWebUIIOS<InternalDebugPagesDisabledUI>` instead of unconditionally
returning the controller factory function.

**Why.** Internal debug and diagnostic WebUI pages expose sensitive internal
browser state, experimental settings, and developer diagnostic tools. Placing
them behind the `kInternalOnlyUisEnabled` preference (via
`InternalDebugPagesEnabled()`) enforces privilege boundaries and protects user
privacy by preventing unauthorized access on non-debug builds and
configurations.

## Examples

Bad:

```cpp
if (url_host == kChromeUIProfileInternalsHost) {
  return &NewWebUIIOS<ProfileInternalsUI>;
}
```

Good:

```cpp
if (url_host == kChromeUIProfileInternalsHost) {
  return InternalDebugPagesEnabled()
             ? &NewWebUIIOS<ProfileInternalsUI>
             : &NewWebUIIOS<InternalDebugPagesDisabledUI>;
}
```

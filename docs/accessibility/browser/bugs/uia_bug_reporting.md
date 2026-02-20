# UIA Bug Reporting

UI Automation (UIA) is now enabled by default for most Windows users in Chromium. The only exception is users on older versions of JAWS that still force the legacy IA2 path. For more info, see the feature flag `DisableUiaProviderWhenJawsIsRunning`.

This guide explains how to confirm whether an issue is caused by the UIA provider and how to file the bug correctly.

## 1. Check whether the issue depends on UIA

Test the behavior with UIA explicitly enabled and disabled. Launch the browser from the command line using:
```
--enable-features=UiaProvider
--disable-features=UiaProvider
```

### Example:
```
C:\<path-to-chrome>\chrome.exe --disable-features=UiaProvider
```

The browser must be restarted after toggling these flags; the feature state does not update at runtime.

If the bug reproduces only when UIA is enabled, it’s likely UIA-specific.

## 2. Filing the bug

If the issue is confirmed to be UIA-specific:
* Prefix the bug title with “UIA”.
* Use the `Chromium > UI > Accessibility` component (or use [this link](https://issues.chromium.org/issues/new?component=1457135&template=2012357) to create the bug on the right component directly)

This ensures the report is routed quickly to the correct owners.

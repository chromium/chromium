---
name: ios/exit-fullscreen-and-clear-forced-triggers-on-navigation
description: When handling completed web navigations in fullscreen observers, mediators, or browser agents, exit fullscreen mode and clear forced fullscreen state on cross-document navigations (!navigationContext->IsSameDocument())
rubric: safety
severity: should_fix
citations:
  - path: docs/security/fullscreen.md
    heading: Overview
    quote: Specifically, we will prioritize improvements to the fullscreen toast UI and ensure the trusted browser UI is shown in sensitive security scenarios to help users make more informed decisions.
---

**Rule.** When handling completed web navigations in fullscreen observers,
mediators, or browser agents, exit fullscreen mode and reset transient or forced
fullscreen state on cross-document navigations
(`!navigationContext->IsSameDocument()`). Do not preserve fullscreen state
across cross-document navigations.

**Why.** Exiting fullscreen upon cross-document navigation is a critical browser
security requirement to prevent URL and UI spoofing by ensuring the user can see
the omnibox, destination URL, and security indicators. Retaining fullscreen
across document navigations violates user expectations and allows untrusted
pages to mimic trusted browser interface elements.

## Examples

Bad:

```cpp
- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  if (!navigationContext->IsSameDocument()) {
    _startScrollingTime = std::nullopt;
    // Missing fullscreen exit on cross-document navigation:
    [self updateViewportInsets:_browserAgent->insets()];
  }
}
```

Good:

```cpp
- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  if (!navigationContext->IsSameDocument()) {
    _startScrollingTime = std::nullopt;
    _isScrollingTimeRecorded = NO;
    _startedDragAtBottom = NO;
    _browserAgent->ExitFullscreen(
        PassKey(), FullscreenModeTransitionTrigger::kForcedByCode,
        /*animated=*/true);
    [self updateViewportInsets:_browserAgent->insets()];
  }
}
```

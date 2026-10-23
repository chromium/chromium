---
name: ios/use-current-browser-provider-for-active-browser
description: When accessing the active Browser or dispatching browser-scoped commands in scene coordinators and agents, query currentBrowserProvider.browser and guard regular-only operations against incognito
rubric: correctness
severity: should_fix
citations:
  - path: docs/ios/objects.md
    heading: Browser
    quote: Browser is the model for a container of tabs for use by the UI. Each window can have multiple Browser associated (regular tabs, incognito tabs, inactive tabs). Additionally, iPads supports an arbitrary number of windows active at the same time. The Browser owns a WebStateList and thus indirectly owns all the tabs (aka WebState and their associated tab helpers). The Browser also owns the CommandDispatcher and ChromeBroadcaster used for dispatching UI commands and property synchronisation.
  - path: ios/chrome/browser/shared/model/browser/browser_provider_interface.h
    heading: '@protocol BrowserProviderInterface'
    quote: '@property(nonatomic, weak, readonly) id<BrowserProvider> currentBrowserProvider;'
---

**Rule.** When interacting with the active Browser from a SceneState (in
coordinators or scene agents) or dispatching browser-scoped commands, query
`currentBrowserProvider.browser` (or `self.currentBrowser`) directly rather than
hardcoding regular browser references (`_regularBrowser`,
`_regularBrowser.get()`, or `mainBrowserProvider.browser`) or inspecting
`incognitoState.incognitoContentVisible`. To verify that the regular browser is
active in scene agents, compare
`mainBrowserProvider.browser == currentBrowserProvider.browser` rather than
checking `browser->type() != Browser::Type::kRegular`. When handling UI
presentation or commands restricted to regular browsing in scene coordinators,
pass `self.currentBrowser` and verify or guard against incognito mode (e.g.
asserting `CHECK(!self.currentBrowser->GetProfile()->IsOffTheRecord())` when
continuing cannot be made safe, or using
`CHECK(currentBrowser == _regularBrowser.get(), base::NotFatalUntil::M<n>)` with
an early return when execution can safely recover) rather than omitting
incognito checks or silently defaulting to `_regularBrowser`.

**Why.** In iOS Chrome's multi-window architecture, each window (`SceneState`)
manages regular, incognito, and inactive `Browser` instances concurrently, each
owning distinct `WebStateList`s and `CommandDispatcher`s.
`currentBrowserProvider` dynamically tracks whichever browser is currently
active and visible. Hardcoding regular browser references or unconditionally
dispatching commands to `_regularBrowser` silently targets the wrong browser
session or masks invalid state transitions when operating in incognito mode or
the TabGrid. Conversely, omitting incognito guards when executing commands
restricted to non-incognito browsing presents disallowed UI in private sessions.

## Examples

Bad:

```cpp
// Bad: Hardcoding regular browser when dispatching commands or child coordinators.
id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
    _regularBrowser->GetCommandDispatcher(), BrowserCoordinatorCommands);
[handler setContainedLayoutActive:contained];

_signinCoordinator = [SigninCoordinator
    signinCoordinatorWithCommand:command
                         browser:_regularBrowser.get()
              baseViewController:baseViewController];

// Bad: Using mainBrowserProvider or checking incognitoContentVisible.
id<PictureInPictureCommands> pipHandler = HandlerForProtocol(
    sceneState.browserProviderInterface.mainBrowserProvider.browser
        ->GetCommandDispatcher(),
    PictureInPictureCommands);
```

Good:

```cpp
// Good: Query active browser, guard regular mode with NotFatalUntil + fallback when recoverable.
Browser* currentBrowser = self.currentBrowser;
CHECK(currentBrowser == _regularBrowser.get(), base::NotFatalUntil::M161);
if (currentBrowser != _regularBrowser.get()) {
  return;
}
id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
    currentBrowser->GetCommandDispatcher(), BrowserCoordinatorCommands);
[handler setContainedLayoutActive:contained];

// Good: Dispatch commands and coordinators using active browser.
id<PictureInPictureCommands> pipHandler = HandlerForProtocol(
    sceneState.browserProviderInterface.currentBrowserProvider.browser
        ->GetCommandDispatcher(),
    PictureInPictureCommands);

CHECK(!self.currentBrowser->GetProfile()->IsOffTheRecord());
_signinCoordinator = [SigninCoordinator
    signinCoordinatorWithCommand:command
                         browser:self.currentBrowser
              baseViewController:baseViewController];
```

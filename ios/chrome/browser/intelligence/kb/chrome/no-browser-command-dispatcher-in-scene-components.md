---
name: ios/no-browser-command-dispatcher-in-scene-components
description: Do not bind or dispatch via a Browser CommandDispatcher in scene-level components; store cross-browser layout and UI state on SceneState
rubric: design
severity: should_fix
citations:
  - path: docs/ios/objects.md
    heading: Browser
    quote: Browser is the model for a container of tabs for use by the UI. Each window can have multiple Browser associated (regular tabs, incognito tabs, inactive tabs)... The Browser owns a WebStateList and樁 indirectly owns all the tabs (aka WebState and their associated tab helpers). The Browser also owns the CommandDispatcher and ChromeBroadcaster used for dispatching UI commands and property synchronisation.
---

**Rule.** Do not inject, store, or bind a single `Browser`'s `CommandDispatcher`
(such as `_regularBrowser->GetCommandDispatcher()`) or browser-scoped command
handlers (`browserCoordinatorHandler`, `browserHandler`) into scene-scoped
components (scene coordinators, scene agents, or scene-level view controllers)
to manage window-wide state that spans multiple browsers. Store UI and layout
state affecting the scene across browsing modes (such as regular, incognito, and
TabGrid) on `SceneState` or an observable scene-scoped state holder (for
example, `SceneLayoutState`), and route scene-level UI actions from view
controllers to the scene coordinator (for example, via
`SceneViewControllerDelegate`).

**Why.** In iOS Chrome architecture, each window scene hosts multiple `Browser`
instances (regular, incognito, inactive tabs) and UI modes, whereas a
`CommandDispatcher` is scoped strictly to a single `Browser` instance and its
tabs. Binding a scene-level component directly to a single browser's dispatcher
causes commands and state updates to fail to reach other active browsing modes
(such as incognito), misplaces state ownership, and couples scene-level
controllers to specific browser lifecycles. Window-wide state must be held on
`SceneState`.

## Examples

Bad:

```cpp
// Bad: Binding scene-level view controller to a single browser's dispatcher.
_viewController.browserCoordinatorHandler = HandlerForProtocol(
    _regularBrowser->GetCommandDispatcher(), BrowserCoordinatorCommands);

// Bad: Scene view controller mutating layout state via browser command handler.
[self.browserCoordinatorHandler setContainedLayoutActive:shouldBeContained];
```

Good:

```cpp
// Good: View controller notifies coordinator via delegate protocol.
[self.delegate sceneViewController:self
    didChangeContainedLayoutActive:shouldBeContained];

// Good: Scene coordinator updates centralized state holder on `SceneState`.
- (void)sceneViewController:(SceneViewController*)viewController
    didChangeContainedLayoutActive:(BOOL)active {
  [self.sceneState.layoutState setContainedLayoutActive:active
                                           scenePassKey:passKey];
}
```

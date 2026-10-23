---
name: ios/call-class-methods-over-internal-storage-members
description: When accessing state or resources from wrapped objects, WebStateList, delegates, or child components, call the class's own accessors rather than reaching into internal members
rubric: readability
severity: should_fix
---

**Rule.** Within class member functions and Objective-C methods, call the
class's own accessor methods, helpers, or property getters (such as
`GetBrowserState()`, `delegate()`, `self.webState`, or `self.property`) directly
instead of bypassing them to reach into internal storage, delegates, child
components, or collaborator objects (such as `web_state_->GetBrowserState()`,
`_webStateList->GetActiveWebState()`, `save_card_delegate_->...`, or
`_containerViewController.property`). In mediators that already track the active
`WebState` via a property, use `self.webState` or `self.activeWebState` directly
rather than re-querying `_webStateList->GetActiveWebState()`. Reusing the
class's own encapsulating accessors ensures consistent access patterns,
preserves sequence verification and virtual dispatch, and avoids bypassing
internal invariants or downcasts.

**Why.** Reaching directly into underlying storage, collaborator objects, or
wrapped members bypasses sequence affinity checks, downcasts, and internal
invariants encapsulated by the class's own interface. In mediators observing
WebStateList, re-querying the list redundantly bypasses existing active tab
tracking and adds cognitive overhead. Calling the class's own accessors
maintains uniform encapsulation, prevents behavioral drift if storage management
changes, and ensures consistent access patterns across member functions.

## Examples

Bad:

```cpp
// Reaching directly into wrapped members, delegates, child ivars, or containers
if (!profile_attributes_storage_.HasProfileWithName(name)) { return false; }
bool is_otr = web_state_->GetBrowserState()->IsOffTheRecord();
if (save_card_delegate_) { save_card_delegate_->OnUpdated(); }
SafariDataImportStage currentStage = _containerViewController.importStage;

// In mediator tracking active WebState, redundantly re-querying WebStateList
web::WebState* webState = _webStateList->GetActiveWebState();
if (!webState) { return; }
UIView* view = webState->GetView();
```

Good:

```cpp
// Call the class's own encapsulating helpers, accessors, or properties
if (!HasProfileWithName(name)) { return false; }
bool is_otr = GetBrowserState()->IsOffTheRecord();
if (save_card_delegate()) { save_card_delegate()->OnUpdated(); }
SafariDataImportStage currentStage = self.importStage;

// Use the mediator's own tracked active WebState property directly
if (!self.webState) { return; }
UIView* view = self.webState->GetView();
```

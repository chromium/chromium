---
name: ios/avoid-on-prefix-for-objective-c-methods
description: When naming Objective-C methods, delegate protocols, or observer bridges, avoid on... prefixes, noun-participles, inverted phrasing, and did with past-tense verbs; follow Cocoa verb-object conventions
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Objective-C++ style matches the language
    quote: For code in an `@implementation` block, use the Objective-C naming rules.
---

**Rule.** In Objective-C and Objective-C++ methods, delegate protocols, and
observer bridges, do not use `on<Event>` prefixes (e.g.,
`onFileExistenceChecked:`), noun-participle event names (e.g., `groupAdded:`),
inverted phrasing with the verb at the end (e.g., `will<Aspect>Change:`), or
double past-tense verbs combining `did` with a past-tense verb (e.g.,
`didDismissed:`). Follow standard Cocoa naming conventions instead: use standard
verb-object order like `willChange<Aspect>:` or `did<Verb><Object>:` with the
base verb after `did` (e.g., `didDismiss:`, `didAddGroup:`,
`willChangeContainedLayout:`) for lifecycle and state notifications, and start
methods performing actions with an active verb describing what the method does
(e.g., `update...` or `present...`).

**Why.** In Chromium C++, `On...` prefixes conventionally denote observer
callbacks, but in Cocoa and Objective-C, methods must follow Cocoa naming
conventions. Standard Cocoa method names convey precise timing and intent:
delegate and observer notifications express lifecycle transitions using
verb-object `Will`/`Did` phrases with base verbs (e.g., `willChange...`,
`didDismiss:`) rather than interrogative-sounding inverted phrasing
(`will...Change:`) or ungrammatical double past tense (`didDismissed:`), while
action methods use active verbs (`update...`, `present...`) to clarify control
flow.

## Examples

Bad:

```cpp
// Non-idiomatic 'on...' prefixes, noun-participles, inverted phrasing, and double past tense:
- (void)onReturnFromSettingsWithPermission:(UNNotificationSettings*)settings;
- (void)onFileExistenceChecked:(const base::FilePath&)path exists:(BOOL)exists;
- (void)syncEncryptionTableViewControllerDidDismissed:
    (SyncEncryptionTableViewController*)viewController;

@protocol DataSharingServiceObserving <NSObject>
@optional
- (void)dataSharingServiceGroupAdded:(const data_sharing::GroupData&)groupData;
- (void)layoutState:(LayoutState*)layoutState
    willContainedLayoutChange:(BOOL)containedLayoutActive;
@end
```

Good:

```cpp
// Cocoa conventions: active verbs, base verb with 'did', and verb-object 'Did/Will':
- (void)updateNotificationForNotificationSetting:(UNNotificationSettings*)settings;
- (void)didCheckFileExistenceAtPath:(const base::FilePath&)path exists:(BOOL)exists;
- (void)syncEncryptionTableViewControllerDidDismiss:
    (SyncEncryptionTableViewController*)viewController;

@protocol DataSharingServiceObserving <NSObject>
@optional
- (void)dataSharingServiceDidAddGroup:(const data_sharing::GroupData&)groupData;
- (void)layoutState:(LayoutState*)layoutState
    willChangeContainedLayout:(BOOL)containedLayoutActive;
@end
```

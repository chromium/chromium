---
name: ios/prefer-unnamed-namespace-helpers-over-private-methods
rubric: design
severity: nit
---

**Rule.** In Objective-C++ (`.mm`) files, prefer implementing file-local helper
routines that do not require Objective-C instance state (`self`, ivars, or
properties)—such as icon/image derivations, data transformations, metric
logging, initial values, or C++ service queries—as free C++ functions in an
unnamed namespace (`namespace { ... }  // namespace`) using `PascalCase` naming,
over private Objective-C instance methods. Pass any required C++ context (such
as `Browser*` or `ProfileIOS*`) as explicit parameters rather than querying
`self`. When querying dynamic service availability without observer callbacks,
evaluate the helper on demand rather than caching results in instance variables.
Conversely, never store instance-specific state in file-scope or
anonymous-namespace global variables, and reserve Objective-C methods on
`@implementation` blocks for routines that require access to `self`, subclass
overrides, or public/protocol contracts. This rule does not apply to the private
helper methods a `UIView` subclass calls from `-init` to construct and lay out
its subview hierarchy, which stay Objective-C methods.

**Why.** Defining stateless helpers as private Objective-C instance methods
incurs dynamic dispatch overhead (`objc_msgSend`), bloats runtime class method
tables and metadata, risks colliding with declared properties, and misleadingly
suggests coupling to instance state. Placing free C++ functions in an unnamed
namespace restricts symbols to the translation unit, provides internal linkage,
optimizes call dispatch, ensures dependencies are passed explicitly, and
immediately communicates that the function is pure with no side effects on
instance state. Additionally, caching dynamic service availability in instance
variables without observers causes desynchronization when capabilities change,
while storing instance state in namespace globals introduces unsafe mutable
shared state across class instances.

## Examples

Bad:

```cpp
// Bad: Storing instance state in namespace global, defining pure data helpers
// and service queries as private methods, and caching dynamic state in ivars.
namespace {
NSString* g_search_subtitle;
}  // namespace

@implementation FileUploadPanelCoordinator {
  BOOL _isChooseFromDriveAvailable;
}

- (UIImage*)iconForItemIdentifier:(ItemIdentifier)itemIdentifier {
  return itemIdentifier == ItemIdentifier::kDrive ? GetDriveIcon() : nil;
}

- (void)start {
  _isChooseFromDriveAvailable = [self isChooseFromDriveAvailable];
}

- (BOOL)isChooseFromDriveAvailable {
  return drive::IsChooseFromDriveAvailable(self.browser->GetProfile());
}
@end
```

Good:

```cpp
// Good: Pure UI/data helpers and service queries defined in unnamed namespace
// using PascalCase with explicit context; dynamic availability checked on demand.
namespace {

UIImage* IconForItemIdentifier(ItemIdentifier item_identifier) {
  return item_identifier == ItemIdentifier::kDrive ? GetDriveIcon() : nil;
}

bool IsChooseFromDriveAvailable(Browser* browser) {
  return drive::IsChooseFromDriveAvailable(browser->GetProfile());
}

}  // namespace

@implementation FileUploadPanelCoordinator {
  NSString* _searchSubtitle;
}

- (void)showDrivePicker {
  if (!IsChooseFromDriveAvailable(self.browser)) {
    return;
  }
  UIImage* icon = IconForItemIdentifier(ItemIdentifier::kDrive);
  [self presentPickerWithIcon:icon];
}
@end
```

---
name: ios/synthesize-protocol-properties-over-trivial-getters
description: Omit redundant @synthesize directives and manual ivars for class properties, use @synthesize or class-extension redeclaration for protocol properties, and write explicit accessors only when bridging C++ smart-pointer ivars
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
---

**Rule.** Do not write explicit `@synthesize` directives or manually declare
backing instance variables (`_<property>`) in `@implementation` for properties
declared on an Objective-C class `@interface` or class extension, as Clang
automatically synthesizes both accessors and backing ivars. When adopting a
protocol with declared properties, do not declare manual backing ivars and
implement trivial getter or setter boilerplate (even when providing a custom
complementary accessor). Instead, either redeclare protocol properties in the
class `@interface` to enable auto-synthesis, or use
`@synthesize property = _property;` (adding an explanatory comment that the
property comes from a protocol and is not autosynthesized). Exception: a
property that exposes an unowned C++ pointer is declared with a bare `T*` and
backed by a manually declared `raw_ptr<T>` (or `base::WeakPtr<T>`) ivar in
`@implementation`, with an explicit getter (`return _cppBar.get();`) and setter;
this boilerplate is required because the ivar type differs from the property
type, and it must not be replaced by auto-synthesis or `@synthesize`.

**Why.** Modern Clang automatically synthesizes declared properties, accessors,
and backing `_<property>` ivars, making explicit `@synthesize` directives and
manual ivar declarations redundant boilerplate. For protocol properties, Clang
does not autosynthesize accessors by default; using
`@synthesize property = _property;` or redeclaring them in `@interface`
generates the backing ivar and unprovided accessors without boilerplate methods.
Documenting required `@synthesize` directives prevents maintainers from deleting
them as obsolete boilerplate. Unowned C++ pointers are the one case where
synthesis is wrong: a synthesized bare `T*` ivar is legacy usage that can
dangle, whereas the explicit `raw_ptr<T>`-backed accessors keep the pointer
protected and let `-disconnect` reset it to `nullptr`.

## Examples

Bad:

```cpp
// Redundant manual ivar and @synthesize for auto-synthesized property
@interface BookmarksItem : TableViewItem
@property(nonatomic, assign) BOOL shouldDisplayIcon;
@end
@implementation BookmarksItem {
  BOOL _shouldDisplayIcon;
}
@synthesize mutator = _mutator;

// Manual ivar and trivial getter for protocol property with custom setter
@interface BrowserViewController () {
  CGFloat _topToolbarInset;
}
@end
- (CGFloat)topToolbarInset {
  return _topToolbarInset;
}
- (void)setTopToolbarInset:(CGFloat)inset {
  if (_topToolbarInset == inset) return;
  _topToolbarInset = inset;
  [self updateToolbarState];
}
@end
```

Good:

```cpp
// Omit manual ivar and @synthesize: both are auto-synthesized
@interface BookmarksItem : TableViewItem
@property(nonatomic, assign) BOOL shouldDisplayIcon;
@end
@implementation BookmarksItem
// (backing _shouldDisplayIcon and mutator are auto-synthesized)
@end

// Redeclare protocol property in @interface for auto-synthesis
@interface FakeAlertConsumer : NSObject <AlertConsumer>
@property(nonatomic, copy) NSString* confirmationAccessibilityLabel;
@end

// Or synthesize protocol property to generate ivar and trivial getter
// Property is declared in a protocol and not autosynthesized.
@synthesize topToolbarInset = _topToolbarInset;

- (void)setTopToolbarInset:(CGFloat)inset {
  if (_topToolbarInset == inset) return;
  _topToolbarInset = inset;
  [self updateToolbarState];
}

// Exception: unowned C++ pointer property; the manual raw_ptr ivar and the
// explicit accessors are required, not boilerplate.
@interface BookmarksMediator : NSObject
@property(nonatomic, assign) PrefService* prefService;
@end
@implementation BookmarksMediator {
  raw_ptr<PrefService> _prefService;
}
- (PrefService*)prefService {
  return _prefService.get();
}
- (void)setPrefService:(PrefService*)prefService {
  _prefService = prefService;
}
@end
```

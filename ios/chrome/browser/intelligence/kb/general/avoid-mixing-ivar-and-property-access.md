---
name: ios/avoid-mixing-ivar-and-property-access
description: In Objective-C++, access plain implementation ivars directly as _ivar everywhere, and access @property-backed state via self.property outside -init, -dealloc, and custom accessors without mixing styles
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
---

**Rule.** This rule applies only to instance variables that back a declared
`@property` (the synthesized `_property` ivars). Plain ivars declared in the
`@implementation MyClass { ... }` block are not properties: access them directly
as `_ivar` everywhere, and do not wrap them in private `@property` declarations
just to hold internal state. For declared properties, outside of initializers
(`-init...`), `-dealloc`, and custom accessors, access the property using dot
syntax (`self.property`) or accessor calls (`[self property]`) rather than
referencing its backing instance variable (`_property`) directly. Conversely,
within custom property accessors (getters and setters), reference backing
instance variables directly (`_ivar`) rather than invoking property accessors on
`self` (`self.property` or `[self property]`), and use dot syntax for properties
on ivars (such as `_ivar.count`). Do not mix direct ivar access and property
accessors for the same property within the same method or scope; bind the
instance to a local variable during multi-step initialization and setup.

**Why.** Using property accessors preserves encapsulation and guarantees
consistent execution of accessor logic, such as lazy initialization, caching,
memory management, and KVO notifications. Directly accessing backing ivars
outside initializers and `-dealloc` bypasses this logic and risks inconsistent
state, whereas invoking `self.property` inside custom accessors risks redundant
getter dispatch or infinite recursion. Restricting direct ivar access to
`-init...`, `-dealloc`, and accessors avoids messaging partially initialized or
deallocated objects. Avoiding mixed ivar and accessor usage within the same
scope improves readability and prevents state desynchronization. Plain ivars
have no accessor logic to bypass, so direct access is the norm for them, and
wrapping them in private properties only bloats binary size with synthesized
accessors.

## Examples

Bad:

```cpp
@interface LocationBarSteadyView : UIView
@property(nonatomic, strong) NSMutableArray<UIView*>* accessibleElements;
@property(nonatomic, weak) id<PageInfoCommands> pageInfoHandler;
@end

// Bad: Backing ivars of declared properties accessed outside -init/-dealloc:
- (void)clearElements {
  [_accessibleElements removeAllObjects];
}
- (void)infoButtonTapped {
  [_pageInfoHandler showPageInfo];
}

// Bad: Mixing direct ivar access and property accessors in the same scope:
@interface BrowserCoordinator : ChromeCoordinator
@property(nonatomic, strong, readonly)
    BrowserLayoutViewController* viewController;
@end
- (void)start {
  _viewController = [[BrowserLayoutViewController alloc] init];
  _infobarCoordinator = [[OverlayContainerCoordinator alloc]
      initWithBaseViewController:self.viewController];
  _viewController.container = _infobarCoordinator.viewController;
}

// Bad: Invoking property getter on self inside custom setter:
- (void)setActions:(NSArray<AlertAction*>*)actions {
  _actions = [actions copy];
  if ([self.actions count] > 0) {
    [self updateActions];
  }
}
```

Good:

```cpp
@interface LocationBarSteadyView : UIView
@property(nonatomic, strong) NSMutableArray<UIView*>* accessibleElements;
@property(nonatomic, weak) id<PageInfoCommands> pageInfoHandler;
@end

// Good: Declared properties accessed via self.property or accessor call:
- (void)clearElements {
  [self.accessibleElements removeAllObjects];
}
- (void)infoButtonTapped {
  [self.pageInfoHandler showPageInfo];
}

// Good: Bind to a local variable to maintain consistent access:
- (void)start {
  BrowserLayoutViewController* viewController =
      [[BrowserLayoutViewController alloc] init];
  _viewController = viewController;
  _infobarCoordinator = [[OverlayContainerCoordinator alloc]
      initWithBaseViewController:viewController];
  viewController.container = _infobarCoordinator.viewController;
}

// Good: Access backing ivar directly and use dot syntax inside custom setter:
- (void)setActions:(NSArray<AlertAction*>*)actions {
  _actions = [actions copy];
  if (_actions.count > 0) {
    [self updateActions];
  }
}

// Good: Plain ivars declared in @implementation are not properties; access
// them directly everywhere instead of adding a private @property.
@implementation TabListMediator {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}
- (void)disconnect {
  _webStateListObserver.reset();
}
@end
```

---
name: ios/use-dot-syntax-for-property-access
description: In Objective-C and Objective-C++, use dot syntax for accessing and mutating declared properties, reserving bracket syntax for method calls, -copy, and custom getters.
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
---

**Rule.** In Objective-C and Objective-C++, access and mutate declared instance
and class properties using dot syntax (e.g., `view.subviews`,
`view.backgroundColor = color`, `self.property`, `Class.property`) rather than
bracketed message syntax (`[view subviews]`, `[view setBackgroundColor:color]`,
`[self property]`, `[Class property]`). Reserve bracket syntax for general
method calls: never use dot syntax on non-property methods such as `-copy`,
`-mutableCopy`, or `+class` (use `[object copy]` and `[ClassName class]`, not
`object.copy` or `ClassName.class`). For properties with custom getters (e.g.,
`@property BOOL viewLoaded` with getter `isViewLoaded`), use dot syntax with the
property name (`self.viewLoaded`) or bracket syntax with the getter
(`[self isViewLoaded]`). Declare dot-accessed accessors explicitly as
`@property`.

**Why.** Using dot syntax exclusively for declared property access and mutation
conforms to the Google Objective-C Style Guide adopted by Chromium, keeps
property access concise and consistent, and visually distinguishes attribute
state access from general method invocations. Invoking methods like `-copy`,
`+class`, or custom getter names with dot syntax violates style conventions,
while explicitly declaring `@property` ensures compiler validation rather than
relying on undeclared accessor tolerance.

## Examples

Bad:

```cpp
// Bracketed syntax for property access and mutation:
for (UIView* barSubview in [_stackView subviews]) {
  [barSubview setBackgroundColor:_barColor];
}
[controller setDelegate:self];

// Dot syntax for non-property method calls or custom getters:
UIButtonConfiguration* config = button.configuration.copy;
self.cellClass = LegacyTableViewCell.class;
CHECK(self.isViewLoaded);
```

Good:

```cpp
// Dot syntax for declared property access and mutation:
for (UIView* barSubview in _stackView.subviews) {
  barSubview.backgroundColor = _barColor;
}
controller.delegate = self;

// Bracket syntax for methods, and property name for custom getters:
UIButtonConfiguration* config = [button.configuration copy];
self.cellClass = [LegacyTableViewCell class];
CHECK(self.viewLoaded);  // or: CHECK([self isViewLoaded]);
```

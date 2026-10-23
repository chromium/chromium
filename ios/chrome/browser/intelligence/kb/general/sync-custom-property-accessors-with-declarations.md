---
name: ios/sync-custom-property-accessors-with-declarations
description: In Objective-C, ensure initializer backing ivar assignments (_<property>) and custom accessor methods (-getter/-setter) exactly match declared property names
rubric: correctness
severity: should_fix
---

**Rule.** When declaring, renaming, or implementing an Objective-C `@property`,
ensure that both custom accessor methods (getters and setters) and backing
instance variable assignments in initializers (`-init...`) exactly match the
declared property name (`_<propertyName>`). Specifically:

1. In initializers, assign directly to the compiler-synthesized backing ivar
   matching the declared property name (`_<propertyName>`) rather than an
   abbreviated, truncated, or mismatched ivar name. If a custom instance
   variable name is intentionally required, explicitly declare an
   `@synthesize propertyName = _customIvar;` directive. Ensure every property
   intended to be initialized has its corresponding backing ivar assigned.
2. When declaring or renaming a property in a header (`.h`), ensure any
   corresponding custom getter or setter method in the implementation file
   (`.mm`) is updated to match the declared property accessor name (or custom
   `getter=`/`setter=` attribute). Do not leave implementation accessor methods
   mismatched when changing property declarations.

**Why.** Clang automatically synthesizes property accessors and backing instance
variables matching declared properties with a leading underscore
(`_<propertyName>`). If an initializer assigns to an abbreviated or mismatched
ivar (such as `_colorID` instead of `_tabGroupColorID`), or if a custom getter
or setter method name fails to match the property declaration (such as
`-selectedIndex` instead of `-selectedSectionIndex`), the compiler does not
connect the implementation to the property. Instead, it synthesizes default
ivar-backed accessors returning zero or `nil`. This leaves the declared property
uninitialized and custom accessors orphaned and uncalled, causing silent runtime
defects, state desynchronization, or null dereference crashes when callers
invoke the property.

## Examples

Bad:

```cpp
// Header (.h)
@property(nonatomic, readonly) tab_groups::TabGroupColorId tabGroupColorID;
@property(nonatomic, readonly) NSInteger selectedSectionIndex;

// Implementation (.mm)
- (instancetype)initWithColorID:(tab_groups::TabGroupColorId)tabGroupColorID {
  self = [super init];
  if (self) {
    // BUG: Assigning to mismatched ivar leaves _tabGroupColorID uninitialized (0/nil).
    _colorID = tabGroupColorID;
  }
  return self;
}

// BUG: Method name does not match property name, so the compiler synthesizes
// a default getter returning 0 and leaves this custom method uncalled.
- (NSInteger)selectedIndex {
  return _collectionView.indexPathsForSelectedItems.firstObject.section;
}
```

Good:

```cpp
// Header (.h)
@property(nonatomic, readonly) tab_groups::TabGroupColorId tabGroupColorID;
@property(nonatomic, readonly) NSInteger selectedSectionIndex;

// Implementation (.mm)
- (instancetype)initWithColorID:(tab_groups::TabGroupColorId)tabGroupColorID {
  self = [super init];
  if (self) {
    // Assign directly to compiler-synthesized backing ivar matching property.
    _tabGroupColorID = tabGroupColorID;
  }
  return self;
}

// Matching getter correctly overrides property accessor synthesis.
- (NSInteger)selectedSectionIndex {
  return _collectionView.indexPathsForSelectedItems.firstObject.section;
}
```

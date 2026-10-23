---
name: ios/use-copy-for-properties-with-mutable-counterparts
description: Specify copy for Objective-C properties with mutable counterparts and explicitly assign [param copy] to backing ivars in custom accessors and initializers
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
  - path: docs/mac/arc.md
    heading: Objective-C Classes
    quote: '@property(nonatomic, copy) NSArray* childCats;'
---

**Rule.** When declaring Objective-C properties for types with mutable
counterparts (such as `NSString`, `NSArray`, `NSDictionary`, `NSSet`, or
`NSData`) or block types, specify the `copy` attribute rather than `strong` or
`assign`, even when `readonly`. Correspondingly, when implementing custom
setters, initializers, or helper methods that store such parameters or
properties declared with `copy` directly into backing instance variables
(`_ivar`) or C++ member variables, explicitly send `[param copy]`
(`_ivar = [param copy];`) rather than assigning the parameter directly.

**Why.** Types with mutable subclasses (such as `NSMutableString`,
`NSMutableArray`, `NSMutableDictionary`, `NSMutableSet`, or `NSMutableData`) can
be passed to parameters expecting their immutable base types. Retaining or
assigning arguments directly to backing instance variables or member variables
bypasses the property's `copy` attribute semantics and leaves internal state
vulnerable to unexpected external mutation. Sending `-copy` guarantees an
independent, immutable snapshot upon assignment.

## Examples

Bad:

```cpp
// Bad: Using strong/assign for property with mutable counterpart:
@property(nonatomic, strong, readonly) NSString* title;

// Bad: Assigning parameter directly in custom setter for a copy property:
- (void)setSaveButtonText:(NSString*)saveButtonText {
  [self.containerDelegate setAcceptButtonText:saveButtonText];
  _saveButtonText = saveButtonText;
}

// Bad: Direct assignment in helper method / setter without copying:
- (void)updateHeaders:(NSDictionary*)headers {
  _headers = headers;
}

// Bad: Assigning directly to C++ member variable in Objective-C++:
void NavigationItemImpl::SetSecurityScopedResource(NSData* data) {
  security_scoped_resource_ = data;
}
```

Good:

```cpp
// Good: Specify copy attribute for types with mutable counterparts:
@property(nonatomic, copy, readonly) NSString* title;

// Good: Explicitly send -copy when assigning to backing ivar in custom setter:
- (void)setSaveButtonText:(NSString*)saveButtonText {
  [self.containerDelegate setAcceptButtonText:saveButtonText];
  _saveButtonText = [saveButtonText copy];
}

// Good: Copy argument when storing in backing instance variable:
- (void)updateHeaders:(NSDictionary*)headers {
  _headers = [headers copy];
}

// Good: Copy argument before storing in C++ member variable:
void NavigationItemImpl::SetSecurityScopedResource(NSData* data) {
  security_scoped_resource_ = [data copy];
}
```

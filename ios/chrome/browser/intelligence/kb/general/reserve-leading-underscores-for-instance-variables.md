---
name: ios/reserve-leading-underscores-for-instance-variables
description: When declaring or referencing methods, properties, or local variables in C++ or Objective-C code and comments, reserve leading and trailing underscores strictly for members
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
  - path: styleguide/objective-c/objective-c.md
    heading: Disambiguating Symbols
    quote: Where needed to avoid ambiguity, use backticks to quote variable names and symbols in comments in preference to using quotation marks or naming the symbols inline.
  - path: styleguide/c++/c++.md
    heading: Chromium C++ style guide
    quote: Chromium follows the Google C++ Style Guide unless an exception is listed below.
---

**Rule.** In C++, Objective-C, and Objective-C++, do not name methods, selector
fragments, local variables, parameters, or block arguments with leading or
trailing underscores. Reserve leading underscores strictly for Objective-C
instance variables (`_lowerCamelCase`) and trailing underscores for C++ class
member variables (`member_`). Name Objective-C methods and locals in clean
`lowerCamelCase` without leading underscores, and C++ locals in clean
`snake_case`. In code comments, refer to Objective-C properties or local
variables by their exact declared identifier rather than adding a leading
underscore (e.g., use `trailingView`, not `_trailingView`), reserving underscore
prefixes in comments exclusively for actual instance variables that are declared
with an underscore.

**Why.** Leading underscores in Objective-C and trailing underscores in C++ are
reserved strictly for instance variables and class data members. Placing leading
underscores on Objective-C method names risks accidental collisions or
unintended overrides with Apple framework private methods. On local variables
and parameters, leading or trailing underscores obscure variable scope and
suggest persistent member state. In code comments, referencing declared
properties or local variables with a leading underscore (such as a synthesized
`_foo` ivar) misleads readers into assuming direct ivar access or non-existent
ivars and creates naming inconsistencies.

## Examples

Bad:

```cpp
// Bad: Referencing properties or local variables with leading underscores in comments.
@property(nonatomic, weak) UIView* trailingView;
// `_trailingView` is attached to the trailing edge of the leading view.

// Bad: Leading underscores on Objective-C method names, local variables, or parameters.
- (void)_updateButtonEnabledState:(UIButton*)_button {
  id<LensOverlayCommands> _lensOverlayCommands =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensOverlayCommands);
  NSString* _title = _button.titleLabel.text;
}

// Bad: Trailing underscores on C++ local variables or parameters.
void SetPermitted(bool newpermitted_) {
  bool permitted_changed_ = (permitted_ != newpermitted_);
  if (permitted_changed_) {
    permitted_ = newpermitted_;
  }
}
```

Good:

```cpp
// Good: Referencing declared properties and local variables without leading underscores.
@property(nonatomic, weak) UIView* trailingView;
// `trailingView` is attached to the trailing edge of the leading view.

// Good: lowerCamelCase without leading underscores for Objective-C methods and locals.
- (void)updateButtonEnabledState:(UIButton*)button {
  id<LensOverlayCommands> lensOverlayHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensOverlayCommands);
  NSString* title = button.titleLabel.text;
}

// Good: snake_case without trailing underscores for C++ locals.
void SetPermitted(bool new_permitted) {
  bool permitted_changed = (permitted_ != new_permitted);
  if (permitted_changed) {
    permitted_ = new_permitted;
  }
}
```

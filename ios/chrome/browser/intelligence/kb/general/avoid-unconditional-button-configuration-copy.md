---
name: ios/avoid-unconditional-button-configuration-copy
description: When updating UIKit button configurations or implementing configurationUpdateHandler blocks, verify whether properties changed before calling copy and reassigning button.configuration
rubric: readability
severity: nit
---

**Rule.** When dynamically updating a button's appearance (such as inside a
`configurationUpdateHandler` or a state change helper), do not unconditionally
allocate a copy of `button.configuration` using `[... copy]`. Instead, inspect
the existing `button.configuration` (or its child `background` configuration)
and verify that relevant properties (such as `baseForegroundColor` or
`backgroundColor`) differ from target values first. Only call
`[configuration copy]`, mutate the modified properties, and reassign
`button.configuration` when a property has actually changed.

**Why.** Handlers such as `configurationUpdateHandler` and button state updates
execute frequently during user interactions, control state transitions, and UI
layout passes. Unconditionally copying configuration objects on every pass
wastes memory by allocating redundant heap instances when the button's
appearance is already up to date, causing unnecessary memory churn.

## Examples

Bad:

```cpp
UIButtonConfiguration* configuration = [button.configuration copy];
if (!configuration) {
  return;
}
UIColor* color = ColorForActionStyle(style, button.enabled);
if (![configuration.baseForegroundColor isEqual:color]) {
  configuration.baseForegroundColor = color;
  button.configuration = configuration;
}
```

Good:

```cpp
UIButtonConfiguration* configuration = button.configuration;
if (!configuration) {
  return;
}
UIColor* color = ColorForActionStyle(style, button.enabled);
if (![configuration.baseForegroundColor isEqual:color]) {
  configuration = [configuration copy];
  configuration.baseForegroundColor = color;
  button.configuration = configuration;
}
```

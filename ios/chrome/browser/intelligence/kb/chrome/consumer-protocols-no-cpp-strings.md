---
name: ios/consumer-protocols-no-cpp-strings
description: When passing text over consumer protocols, pass localized NSString instead of std::string or std::u16string.
rubric: design
severity: should_fix
citations:
  - path: ios/architecture/mediators.md
    heading: Data Types Passed Over Consumer Protocols
    quote: 'C++ strings (std::string, std::u16string) should not be passed to a consumer: if a string is meant to be displayed, the mediator should convert it to NSString*, and if it represents a state used to control the UI, it should be an enum class instead.'
---

**Rule.** Do not pass `std::string` or `std::u16string` across consumer
protocols. The mediator must resolve or convert strings to `NSString*` before
passing them to the UI.

**Why.** The UI layer renders text using UIKit components expecting `NSString*`.
Passing C++ strings forces redundant conversions and leaks cross-platform model
representations. See
`ios/architecture/mediators.md#data-types-passed-over-consumer-protocols`.

## Examples

Bad:

```cpp
@protocol ToolbarConsumer <NSObject>
- (void)setPageTitle:(const std::u16string&)title;
@end
```

Good:

```cpp
@protocol ToolbarConsumer <NSObject>
- (void)setPageTitle:(NSString*)title;
@end
```

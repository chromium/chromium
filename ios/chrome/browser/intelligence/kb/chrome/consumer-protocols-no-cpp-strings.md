---
name: ios/consumer-protocols-no-cpp-strings
rubric: design
severity: should_fix
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

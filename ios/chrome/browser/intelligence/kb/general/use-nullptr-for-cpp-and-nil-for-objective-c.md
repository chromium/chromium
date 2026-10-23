---
name: ios/use-nullptr-for-cpp-and-nil-for-objective-c
description: In Objective-C, Objective-C++, and C++ code and comments, use nil for Objective-C objects, nullptr for C++ pointers and raw_ptr members, and nullopt for std::optional
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Code Formatting
    quote: Use `nil` for null pointers to Objective-C objects, and `nullptr` for C++ objects.
---

**Rule.** In Objective-C, Objective-C++ (.mm), and C++ code and comments, use
`nil` (never `nullptr` or `NULL`) strictly for Objective-C object pointers
(`id`, `NSObject*`, and Objective-C interfaces) and properties. Conversely, use
`nullptr` in code and 'null' or 'nullptr' in comments (never `nil`) when
assigning, initializing, comparing, or resetting null pointers to C++ objects,
structs, and smart pointers or wrappers (such as `raw_ptr<T>`,
`std::unique_ptr`, `scoped_refptr`, or `base::WeakPtr`), including inside
cleanup methods like `disconnect`, `tearDown`, or `dealloc`. Use `nullopt` or
`std::nullopt` (never `nil` or `nullptr`) when documenting or describing the
empty state of an `std::optional`.

**Why.** Chromium's Objective-C style guide mandates using `nil` exclusively for
Objective-C objects and properties, and `nullptr` for C++ objects and pointers.
In Objective-C++, mixing `nil`, `nullptr`, and `nullopt` blurs language
boundaries, violates style conventions, and obscures whether an entity is an
Objective-C instance, a C++ smart or raw pointer, or a disengaged optional.
Consistently reserving `nullptr` for C++ members and `nil` for Objective-C
references preserves type semantics and prevents subtle comparison or overload
mismatches across language boundaries.

## Examples

Bad:

```cpp
// Bad: nullptr for Objective-C object, nil for C++ raw_ptr, and nil for std::optional docs.
self.geminiHandler = nullptr;
- (void)disconnect {
  _consumer = nil;
  _authService = nil;        // C++ raw_ptr<AuthenticationService>
}

if (web_state == nil) {      // C++ pointer comparison
  return;
}
// Returns account info or nil if not found.
std::optional<AccountInfo> GetAccountInfo(const std::string& email);
```

Good:

```cpp
// Good: nil for Objective-C object, nullptr for C++ raw_ptr, and nullopt for std::optional docs.
self.geminiHandler = nil;
- (void)disconnect {
  _consumer = nil;
  _authService = nullptr;    // C++ raw_ptr<AuthenticationService>
}

if (web_state == nullptr) {  // C++ pointer comparison
  return;
}
// Returns account info or nullopt if not found.
std::optional<AccountInfo> GetAccountInfo(const std::string& email);
```

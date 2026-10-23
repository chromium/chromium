---
name: ios/use-cpp-naming-in-objective-cpp
description: In Objective-C++ (.h, .mm), match identifier naming and boolean types (BOOL with YES/NO vs. bool with true/false) to the enclosing Objective-C or C++ construct rather than the types being manipulated
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Objective-C++ style matches the language
    quote: Within an Objective-C++ source file, follow the style for the language of the function or method you're implementing. In order to minimize clashes between the differing naming styles when mixing Cocoa/Objective-C and C++, follow the style of the method being implemented. For code in an `@implementation` block, use the Objective-C naming rules. For code in a method of a C++ class, use the C++ naming rules. For C functions and constants defined in a namespace, use C++ style, even if most of the file is Objective-C. `TEST` and `TEST_F` macros expand to C++ methods, so even if a unit test is mostly testing Objective-C objects and methods, the test should be written using C++ style.
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
  - path: styleguide/c++/c++.md
    heading: Unnamed namespaces
    quote: Items local to a .cc file should be wrapped in an unnamed namespace.
---

**Rule.** Within Objective-C++ files (`.h`, `.mm`), follow the naming
conventions and boolean types of the enclosing construct being implemented
rather than the types being manipulated:

1. **Objective-C scopes (`@interface`, `@implementation`, protocols, categories,
   and method bodies)**:

- Use `UpperCamelCase` for class, category, and protocol names, and
  `lowerCamelCase` for method names, selector keywords, instance variables
  (`_lowerCamelCase`), properties, method parameters, and local variables, even
  when they declare, instantiate, or hold C++ types, closures, or objects (e.g.,
  `base::RepeatingClosure barrierClosure`,
  `std::unique_ptr<ScopedUIBlocker> UIBlocker`,
  `tab_groups::TabGroupColorId colorID`, `std::string requestID`,
  `ProfileIOS* profileManager`).
- Fully capitalize acronyms and initialisms (such as `URL`, `ID`/`IDs`, `CVC`,
  `FRE`, `NTP`, `MIME`, `OTP`, `PDF`, `XML`, `JSON`, `HTML`, `IPH`, `AI`, `PIP`,
  `QR`, `UI`, `HTTP`) in every Objective-C identifier position, including at the
  start of a name: selectors and properties (`openURL:`, `initWithFREType:`,
  `cardCVC`, `PIPHandler`, `AIEntityItem`), instance variables (`_URL`,
  `_NTPVisible`), and parameters (`(const GURL&)URL`, `(NSString*)CVC`,
  `setNTPVisible:(BOOL)NTPVisible`). Do not use `snake_case` (`blocked_url`),
  title-cased acronyms (`clientId`, `cardCvc`, `requestId`), or lowercased
  acronyms (`url`, `freType:`, `_ntpVisible`, `pipHandler`, `aiEntityItem`).
- Use `BOOL` with `YES`/`NO` for properties, instance variables, parameters,
  return types, and local variables; a local variable may be `bool` only when it
  directly holds or feeds a C++ API value. Never use `TRUE`/`FALSE` or integer
  literals (`1`/`0`) as booleans, and never compare against `YES`/`NO`
  (`if (flag)`, not `if (flag == YES)`).

2. **C++ scopes (unnamed namespaces, free functions, C++ classes, and GoogleTest
   macros such as `TEST`, `TEST_F`, `TEST_P`)**:

- Wrap file-scope C++ helper functions, classes, and constants in an unnamed
  namespace (`namespace { ... }`) for internal linkage.
- Name C++ free functions, namespace helpers, and member methods—including
  EarlGrey matcher functions returning `id<GREYMatcher>` or Objective-C
  objects—in `PascalCase` (e.g., `AsAttributedString()`,
  `QuickDeleteTitleMatcher()`), never in Objective-C `lowerCamelCase`.
- Use `snake_case` for all parameters and local variables, including inside
  `TEST`/`TEST_F`/`TEST_P` bodies and even when they hold Objective-C types or
  blocks (e.g., `UIViewController* base_view_controller`, `UIImage* icon_image`,
  `id<GREYMatcher> matcher`), and prefix compile-time constants with `k`
  (`constexpr CGFloat kZeroPointSize = 0.0;`).
- Use C++ `bool` with `true`/`false`, never `BOOL`/`YES`/`NO`.
- Name C++ methods after the action rather than embedding selector-style
  descriptors or prepositions (avoid
  `ShowFloatyIfInvokedWithAnimation(bool animated)`), and annotate call-site
  arguments with inline comments (`/*animated=*/true`). Acronym casing in
  existing C++ identifiers follows the surrounding code; do not request renames
  between `Id` and `ID` in C++.

3. **Boundaries between the two**: block and delegate signatures that wrap a C++
   service use that service's boolean type (`bool` or `BOOL`) consistently
   across all related callbacks rather than mixing the two; when calling a C++
   function pass `true`/`false`, when messaging an Objective-C method or
   assigning a `BOOL` property pass `YES`/`NO`.

**Why.** Per Chromium's Objective-C style guide ("Objective-C++ style matches
the language"), code within an Objective-C++ translation unit follows the style
of the construct being implemented, not of the types being manipulated. Cocoa
and the Google Objective-C Style Guide require `lowerCamelCase`, `BOOL`, and
fully capitalized acronyms across selectors, properties, ivars, parameters, and
locals so that code stays consistent with system APIs (`NSURL`, `openURL:`) and
synthesized ivars; title-casing or lowercasing acronyms introduces inconsistency
when bridging C++ and Objective-C, and `//ios/chrome` overwhelmingly uses the
capitalized form (`(const GURL&)URL`, `_URL`, `AIEntityItem`). Conversely, free
functions, C++ classes, and GoogleTest macros (which expand to C++ methods) are
C++ constructs, so `lowerCamelCase`, `BOOL`, or selector-style names there
violate Chromium C++ style, and file-scope helpers outside an unnamed namespace
risk duplicate-symbol linker collisions. `BOOL` and `bool` are implicitly
convertible, but `TRUE`/`FALSE` are non-standard, comparing against `YES` is
unreliable when a `BOOL` holds a non-zero value other than 1, and mixing `BOOL`
and `bool` across the callbacks of one C++ service produces confusing,
inconsistent signatures. See `styleguide/objective-c/objective-c.md`
("Objective-C++ style matches the language") and
`ios/best_practices.md#style-and-reference-guides`.

## Examples

Bad:

```cpp
// Bad: C++ snake_case, bool, lowercased/camelCased acronyms, and ivars in Obj-C:
@implementation ProfileCoordinator
- (void)openURL:(const GURL&)url freType:(GeminiFREType)freType withSync:(bool)sync_enabled {
  _ntpVisible = YES;
  tab_groups::TabGroupColorId colorId = tab_groups::TabGroupColorId::kGrey;
  std::string requestId = self.testServer->GetRequestId();
}
- (BOOL)isTabGroupAllowed {
  BOOL isEmpty = countryItem.detailText.length <= 1;
  return isEmpty == YES ? false : true;
}
@property(nonatomic, strong) NSString* cardCvc;
@property(nonatomic, weak) id<PictureInPictureCommands> pipHandler;
@property(nonatomic, readonly) bool hasInitialTabIDs;
@end
row.collapsed = false;
[handler showAnimated:FALSE];

// Bad: BOOL in a completion block wrapping a C++ service whose other callbacks use bool:
- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*, BOOL))completion;

// Bad: camelCase helper outside unnamed namespace; camelCase/BOOL/YES in TEST:
id<GREYMatcher> quickDeleteTitleMatcher(UIViewController* baseViewController) {
  return chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(IDS_TITLE);
}
TEST_F(GeminiTest, SymbolWithSize) {
  const CGFloat zeroPointSize = 0.0;
  UIImage* testImage = [Gemini symbolWithSize:zeroPointSize];
  BOOL shouldSync = YES;
  geminiBrowserAgent->ShowFloatyIfInvokedWithAnimation(YES);
}
```

Good:

```cpp
// Good: lowerCamelCase, BOOL, and fully capitalized acronyms in Obj-C:
@implementation ProfileCoordinator
- (void)openURL:(const GURL&)URL FREType:(GeminiFREType)FREType withSync:(BOOL)syncEnabled {
  _NTPVisible = YES;
  tab_groups::TabGroupColorId colorID = tab_groups::TabGroupColorId::kGrey;
  std::string requestID = self.testServer->GetRequestId();
}
- (BOOL)isTabGroupAllowed {
  BOOL isEmpty = countryItem.detailText.length <= 1;
  return !isEmpty;
}
@property(nonatomic, strong) NSString* cardCVC;
@property(nonatomic, weak) id<PictureInPictureCommands> PIPHandler;
@property(nonatomic, readonly) BOOL hasInitialTabIDs;
@end
row.collapsed = NO;
[handler showAnimated:NO];

// Good: the completion block matches the C++ service's bool like its siblings:
- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*, bool))completion;

namespace {
// Good: unnamed namespace, PascalCase matcher, and snake_case parameter:
id<GREYMatcher> QuickDeleteTitleMatcher(UIViewController* base_view_controller) {
  return chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(IDS_TITLE);
}
}  // namespace

// Good: snake_case variables, kCamelCase constant, bool, action-named C++ call in TEST:
TEST_F(GeminiTest, SymbolWithSize) {
  constexpr CGFloat kZeroPointSize = 0.0;
  UIImage* test_image = [Gemini symbolWithSize:kZeroPointSize];
  bool should_sync = true;
  geminiBrowserAgent->ShowFloatyIfInvoked(/*animated=*/true);
}
```

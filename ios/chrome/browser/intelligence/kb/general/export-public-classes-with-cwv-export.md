---
name: ios/export-public-classes-with-cwv-export
description: When declaring public Objective-C classes in ios/web_view/public/, annotate each @interface with CWV_EXPORT and import cwv_export.h to ensure visibility in the dynamic framework
rubric: correctness
severity: should_fix
citations:
  - path: styleguide/c++/c++.md
    heading: Exporting symbols
    quote: 'Symbols can be exported (made visible outside of a shared library/DLL) by annotating with a `<COMPONENT>_EXPORT` macro name (where `<COMPONENT>` is the name of the component being built, e.g. BASE, NET, CONTENT, etc.). Class annotations should precede the class name:'
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium C++ style
    quote: Where appropriate, the [Chromium C++ style](../c++/c++.md) style guide applies to Chromium Objective-C and (especially) Objective-C++
---

**Rule.** In `ios/web_view/public/`, annotate all public Objective-C class
interfaces (`@interface`) and exported global symbols with `CWV_EXPORT` and
`#import "cwv_export.h"`. Do not declare public classes without `CWV_EXPORT`,
and do not write visibility attributes like
`__attribute__((visibility("default")))` directly.

**Why.** In `ios/web_view`, `ChromeWebView.framework` is built as an Objective-C
dynamic framework with compiler symbol visibility hidden by default. Public
class interfaces and exported symbols must be annotated with `CWV_EXPORT` so
their symbols are exported with default visibility and can be linked by external
embedders.

## Examples

Bad:

```cpp
#import <Foundation/Foundation.h>

@interface CWVCardUnmaskChallengeOption : NSObject
@property(nonatomic, copy, readonly) NSString* identifier;
@end
```

Good:

```cpp
#import <Foundation/Foundation.h>

#import "cwv_export.h"

CWV_EXPORT
@interface CWVCardUnmaskChallengeOption : NSObject
@property(nonatomic, copy, readonly) NSString* identifier;
@end
```

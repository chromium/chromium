---
name: ios/use-import-instead-of-include
description: 'In Objective-C++ (.mm) files under ios/ and headers consumed only by Objective-C++ translation units, use #import instead of #include for all headers, with angle brackets for system headers and quotes for repo headers'
rubric: readability
severity: blocking
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: '#import and #include in the `ios/` directory'
    quote: '#import directive can be used to import C++ and Objective-C headers for all source code in the `ios/` directory. This differs from the Google Objective-C Style Guide, which requires using #include directive for C++ headers.'
  - path: styleguide/c++/c++.md
    heading: Platform-specific code
    quote: 'Repeat the standard `#include` order within this section: ```c++ #include "foo/foo.h" #include <stdint.h> #include <algorithm> #include "base/strings/utf_string_conversions.h" ```'
---

**Rule.** In Objective-C++ (`.mm`) files under `ios/`, and in headers that are
only consumed by Objective-C++ translation units, use the `#import` directive
instead of `#include` for all headers. This applies to Objective-C and C++
project headers (including Chromium components, internal headers, and Mojo
bindings like `*.mojom.h`) as well as C and C++ standard library headers. Do not
flag pure C++ files (`.cc`, and headers shared with cross-platform C++ code),
which may keep `#include`. When importing C++ standard library headers, use
system-style angle brackets without `.h` (e.g., `#import <string>`, not quoted
`#import "string.h"` or `#import "string"`). Group standard library and system
SDK headers at the top, followed by project headers, separated by a blank line.

**Why.** Chromium's Objective-C style guide explicitly provides an exception to
the Google Objective-C Style Guide, permitting and favoring `#import` for C++
and Objective-C headers across source code in the `ios/` directory, and
`ios/best_practices.md` requires it in `.mm` files (never `#include`, even for C
or C++ headers); `ios/PRESUBMIT.py` rejects `#include` directives in `.mm`
files. Consistently using `#import` prevents duplicate header inclusion in
Objective-C++ translation units (`.mm`) and ensures uniform header inclusion
style across the iOS codebase. Additionally, using angle brackets without `.h`
for standard library headers and separating them from project headers with a
blank line adheres to Google and Chromium include ordering standards, prevents
ambiguous relative lookups, and enables predictable sorting by automated
formatters such as clang-format.

## Examples

Bad:

```cpp
// Bad: Using #include for project headers and Mojo bindings in a .mm file
#include "components/enterprise/connectors/connectors_internals.mojom.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

// Bad: Using #include or quoted imports below project headers for stdlib
#import "ios/web/web_state/web_state_impl.h"
#import "string.h"
#include <memory>
```

Good:

```cpp
// Good (.mm): System/stdlib headers in angle brackets at top using #import,
// followed by #import for all project headers and Mojo bindings.
#import <memory>
#import <string>

#import "base/memory/raw_ptr.h"
#import "components/enterprise/connectors/connectors_internals.mojom.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
```

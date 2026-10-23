---
name: ios/sort-forward-declarations-alphabetically
description: In C++ and Objective-C headers, place imports before forward declarations, declare @class before @protocol on separate lines, and sort declarations and namespaces alphabetically
rubric: readability
severity: should_fix
citations:
  - path: agents/skills/browser-window-feature-refactor/references/bwf-ordering.md
    heading: Public Header
    quote: Keep top-level forward declarations sorted alphabetically within their existing unconditional group.
---

**Rule.** In C++ and Objective-C header and source files, place all #include and
#import directives at the top before any forward declarations; do not interleave
imports with forward declarations or place them below declarations. Place
forward declarations (`class`, `struct`, `enum class`, `@class`, `@protocol`,
including templated or export-macro-annotated declarations) in a contiguous
block directly after the #import/#include block (separated by a single blank
line) before any class, interface, or protocol definitions. Forward-declare each
Objective-C class and protocol on its own line; do not combine multiple types
into a single comma-separated declaration list (e.g., `@class Foo, Bar;`). In
Objective-C headers, place @class forward declarations before @protocol forward
declarations. Within declaration blocks, insert new declarations into their
strict lexicographical position rather than appending them out of order. Place
top-level declarations alphabetically before namespaced forward-declaration
blocks, sort namespace blocks alphabetically by namespace name, and sort types
within each namespace alphabetically without duplicates.

**Why.** Clang-format does not automatically reorder, sort, or deduplicate
forward declarations across directives or namespace blocks, nor does it split
comma-separated declaration lists. Grouping all #include and #import directives
at the top establishes a predictable header layout and ensures external
definitions are available before declarations. Comma-separated @class and
@protocol declarations format awkwardly under clang-format, impair
searchability, and create noisy diffs. Placing @class before @protocol and
maintaining strict alphabetical ordering within blocks and namespaces avoids
redundant declarations, minimizes merge conflicts, and makes symbols predictable
and easy to locate during navigation and code review.

## Examples

Bad:

```cpp
// Bad: Interleaved imports, comma-separated lists, @protocol before @class, and unsorted namespaces.
#import "components/autofill/core/data_model.h"

@protocol CRWFindSession, CRWFindInteraction;
@class SettingsViewController, AccountDetailCoordinator;

#import "ios/chrome/browser/settings/identity_docs.h"

namespace web {
class WebStateID;
}  // namespace web
namespace autofill {
class PersonalDataManager;
class AddressDataManager;
}  // namespace autofill
```

Good:

```cpp
// Good: Imports at top, single-line sorted @class before @protocol, and sorted namespaces.
#import "components/autofill/core/data_model.h"
#import "ios/chrome/browser/settings/identity_docs.h"

@class AccountDetailCoordinator;
@class SettingsViewController;

@protocol CRWFindInteraction;
@protocol CRWFindSession;

namespace autofill {
class AddressDataManager;
class PersonalDataManager;
}  // namespace autofill
namespace web {
class WebStateID;
}  // namespace web
```

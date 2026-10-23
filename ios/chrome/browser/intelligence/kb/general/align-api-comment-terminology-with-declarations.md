---
name: ios/align-api-comment-terminology-with-declarations
description: When documenting function declarations, integrate backtick-wrapped parameters into complete sentences, omit redundant parameter types, prune obsolete parameters, and qualify enclosing members
rubric: readability
severity: should_fix
citations:
  - path: docs/documentation_best_practices.md
    heading: Method and class comments
    quote: This documentation details what arguments the method takes, what it returns, any "gotchas" or restrictions, and what exceptions it can throw or errors it can return.
  - path: styleguide/objective-c/objective-c.md
    heading: Disambiguating Symbols
    quote: Where needed to avoid ambiguity, use backticks to quote variable names and symbols in comments in preference to using quotation marks or naming the symbols inline.
  - path: styleguide/c++/c++.md
    heading: Chromium C++ style guide
    quote: Chromium follows the Google C++ Style Guide unless an exception is listed below.
---

**Rule.** When documenting function or method declarations or updating
signatures, reference parameter names in backticks (never pipes such as
`|name|`, which `PRESUBMIT.py` rejects) and integrate them directly into
complete grammatical sentences rather than standalone tag-style fragments (e.g.,
`@param` or `|name| Description`). Prune references to deleted or obsolete
parameters, and avoid generic names, mismatched casing, isolated jargon (e.g.,
'ticket ID' for download ID), or redundantly restating declared parameter types
that are already visible in the signature. Additionally, keep comment
terminology aligned with declared types and contracts: when a signature changes
to accept an enclosing owner or context object instead of a direct member,
qualify the member via the enclosing object (e.g., the `Browser`'s
`WebStateList`) rather than leaving stale, unqualified references.

**Why.** Leaving standalone tag fragments or redundantly restating parameter
types adds noise and risks documentation drifting out of sync with declared
signatures. Documenting only a subset of parameters, retaining deleted parameter
references, or leaving unqualified member references after enclosing-object
refactorings creates ambiguity about object origins and contracts. Integrating
parameter references naturally into complete English sentences in backticks and
qualifying members via their enclosing objects maintains an accurate contract
link, avoids confusion with underlying types, and ensures comments remain
idiomatic, readable, and searchable.

## Examples

Bad:

```cpp
// Gets the validity notBefore and notAfter times.
bool GetTimes(base::Time* not_before, base::Time* not_after) const;

// `infoBarDelegate` was removed from the signature but retained in the comment.
// Designated Initializer. `infoBarDelegate` configures the Infobar.
- (instancetype)initWithBrowser:(Browser*)browser type:(InfobarType)type;

// Starts observing the WebStateList and installing dependencies.
void StartObserving(Browser* browser, Policy policy);

// |type| The `CWVAutofillProgressDialogType` that determines the dialog title.
// |cancelAction| An optional block to be executed if canceled.
- (void)showProgressDialogOfType:(CWVAutofillProgressDialogType)type
                    cancelAction:(ProceduralBlock)cancelAction;
```

Good:

```cpp
// Gets the validity `not_before` and `not_after` times.
bool GetTimes(base::Time* not_before, base::Time* not_after) const;

// Designated Initializer for the given `type`.
- (instancetype)initWithBrowser:(Browser*)browser type:(InfobarType)type;

// Starts observing the `Browser`'s `WebStateList` and installing dependencies.
void StartObserving(Browser* browser, Policy policy);

// `type` determines the dialog title. `cancelAction` is an optional block
// executed if the user cancels.
- (void)showProgressDialogOfType:(CWVAutofillProgressDialogType)type
                    cancelAction:(ProceduralBlock)cancelAction;
```

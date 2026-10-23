---
name: ios/match-parameter-names-between-declaration-and-definition
description: When declaring functions, virtual overrides, Objective-C methods, or forwarding blocks, keep parameter names consistent with declarations, base classes, and selector keywords
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium C++ style
    quote: Where appropriate, the [Chromium C++ style](../c++/c++.md) style guide applies to Chromium Objective-C and (especially) Objective-C++
  - path: styleguide/c++/c++.md
    heading: Chromium C++ style guide
    quote: Chromium follows the Google C++ Style Guide unless an exception is listed below.
---

**Rule.** Ensure parameter names remain consistent across declarations,
definitions, overrides, and forwarders:

1. Header declarations vs. definitions: Function and Objective-C method
   parameter names must match identically between header (.h) declarations and
   source (.cc, .mm) definitions.
2. Virtual method overrides: In derived class overrides, concrete
   implementations, and test fakes or mocks, keep parameter names consistent
   with the base class declaration and interface contract rather than
   introducing diverging names (e.g., do not rename `completion` to `closure`).
3. Objective-C selector keywords: Name each formal parameter after the noun of
   its selector keyword rather than introducing divergent prefixes or names
   (e.g., `pageContextAttached:(BOOL)pageContextAttached`, not
   `isPageContextAttached`). The only exception is a keyword that is a
   preposition or qualifier phrase (`withProgress:`, `animated:`), where the
   parameter may carry a more specific noun that still contains the keyword's
   noun (`applyLayoutWithProgress:(CGFloat)sidePanelProgress`). When selector
   keywords include qualifying suffixes such as `ForTest` or `ForTesting`,
   retain the qualifier on the parameter identifier (e.g.,
   `autofillClientForTest:(...)autofillClientForTest` rather than
   `autofillClient`).
4. Forwarding blocks: When declaring callback or block parameters that forward
   directly to downstream methods or delegates, align the block parameter name
   with the target method's selector keyword (e.g., use `canProceed` when
   forwarding to `didConfirmChangeProfileCanProceed:`).

When updating or renaming parameters in any signature, update declarations,
implementations, overrides, and forwarders consistently.

**Why.** Inconsistent parameter naming between header declarations, source
definitions, abstract interfaces, and concrete or test overrides harms code
readability and searchability, breaks symbol indexing and IDE refactoring tools,
and confuses readers regarding parameter roles. In Objective-C and
Objective-C++, selector keywords form part of the API's identity; matching
parameter identifiers to selector keywords—including preserving qualifiers like
`ForTest` or `ForTesting`—ensures self-documenting method signatures, prevents
confusion in method bodies, and visibly distinguishes test injection parameters
from production data throughout the implementation. Similarly, aligning
forwarding block arguments with downstream delegate selector keywords maintains
consistent domain vocabulary across architectural boundaries.

## Examples

Bad:

```cpp
// C++: Parameter names diverge between header declaration and source definition.
// Header (.h): void PopulateAPCNode(const base::DictValue& node_content, optimization_guide::proto::ContentNode* node);
// Implementation (.mm):
void PopulateAPCNode(const base::DictValue& node_content,
                     optimization_guide::proto::ContentNode* destination_node);

// Virtual override diverges from base class declaration (completion vs closure).
// In class TestMailtoHandlerService : public MailtoHandlerService
void HandleMailtoURL(NSURL* url, base::OnceClosure closure) override;

// Objective-C: Parameter names diverge from selector keywords or strip ForTest.
- (void)didSendQueryWithInputType:(QueryInputType)inputType
              pageContextAttached:(BOOL)isPageContextAttached;
- (instancetype)initWithWebState:(web::WebState*)webState
           autofillClientForTest:(Client*)autofillClient;

// Forwarding block parameter diverges from target selector keyword.
confirmChangeProfile(^(BOOL proceed) {
  [delegate didConfirmChangeProfileCanProceed:proceed];
});
```

Good:

```cpp
// C++: Parameter names match between header declaration and source definition.
void PopulateAPCNode(const base::DictValue& node_content,
                     optimization_guide::proto::ContentNode* destination_node);

// Virtual override matches parameter name from base interface.
// In class TestMailtoHandlerService : public MailtoHandlerService
void HandleMailtoURL(NSURL* url, base::OnceClosure completion) override;

// Objective-C: Parameter names match selector keywords and retain ForTest.
- (void)didSendQueryWithInputType:(QueryInputType)inputType
              pageContextAttached:(BOOL)pageContextAttached;
- (instancetype)initWithWebState:(web::WebState*)webState
           autofillClientForTest:(Client*)autofillClientForTest;
// A preposition keyword may take a more specific noun containing its own.
- (void)applyLayoutWithProgress:(CGFloat)sidePanelProgress;

// Forwarding block parameter matches target delegate selector keyword.
confirmChangeProfile(^(BOOL canProceed) {
  [delegate didConfirmChangeProfileCanProceed:canProceed];
});
```

---
name: ios/document-api-contracts-by-observable-behavior
description: When documenting header declarations, describe caller-facing contracts, actions, and observable behavior rather than internal implementation mechanics, helper calls, or caller trigger context
rubric: readability
severity: should_fix
citations:
  - path: docs/documentation_guidelines.md
    heading: Interface / class documentation
    quote: Non-trivial classes or sets of function declarations in header files should include comments with a brief description of the purpose of the class or set of functions as well as examples of expected usage.
  - path: docs/documentation_best_practices.md
    heading: Method and class comments
    quote: 'Method API documentation: The header / Javadoc / docstring comments that say what methods do and how to use them. This documentation is the contract of how your code must behave. The intended audience is future programmers who will use and modify your code.'
  - path: docs/documentation_guidelines.md
    heading: Guidelines
    quote: 'Interface / class documentation: This is documentation most often found in header files near class definitions. This may describe what a class or small set of classes are for and how they are intended to be used... Implementation documentation: This type of documentation is found next to implementations. In C++, this will tend to be embedded in .cc files. It is meant to help a reader understand *how the code works* or explain an implementation path that might not be obvious from the code itself.'
  - path: docs/ios/objects.md
    heading: WebState
    quote: WebState wraps a WKWebView and allows navigation. A WebState can have many tab helpers attached.
  - path: docs/documentation_guidelines.md
    heading: Implementation documentation
    quote: A good rule of thumb is that if a code reviewer had a clarifying question about something during review and the code couldn't be simplified to answer the question then a comment is probably appropriate.
---

**Rule.** In header and interface doc comments for functions, methods,
parameters, option structs, or virtual overrides, describe the caller-facing
contract: conceptual purpose, parameter semantics and constraints (bounds,
counts, test-only restrictions, role requirements such as needing a primary
identity), and observable behavior across conditions, including secondary
mutations (modifying arguments, updating UI, dispatching to an external SDK),
non-obvious initial states (such as a subview added hidden by default), delegate
prerequisites and caller cache-invalidation obligations, and intentional no-op
or early-return behavior (when the method takes effect versus does nothing, and
whether out-of-order, repeated, or post-completion calls are ignored or require
caller-satisfied lifecycle prerequisites). Use consistent terminology for
conceptual data sources (e.g., "persisted storage"). Describe what the operation
does rather than only the caller context or trigger (such as "Called when ..."),
and do not document internal implementation mechanics—specific helper calls
(e.g., "via ExecuteJavaScript()"), private state resets, helper policy checks,
parsing details, intermediate services, or speculative alternative
behaviors—which belong in the `.cc`/`.mm` next to the code. Reserve trigger-only
comments for delegate, observer, or lifecycle callbacks whose primary role is
reacting to external notifications.

**Why.** Public headers define the API contract for callers and override
interfaces, and method names convey only high-level intent; documenting what an
operation performs, what data it transforms, its side effects, default states,
mode limitations, and lifecycle boundaries lets callers understand
responsibilities without inspecting implementation bodies and prevents incorrect
assumptions, redundant defensive guards, and subtle regressions when
configurations or call orders change. Conversely, exposing internal execution
mechanics, private helper calls, state resets, intermediate services, or
describing only caller-side trigger conditions leaks implementation
abstractions, clutters interfaces, and creates brittle documentation that drifts
out of sync with code.

## Examples

Bad:

```cpp
// Variant 1: Internal execution mechanics and helper calls instead of observable contract
// If `origin_filter` is provided, the script will only be injected in pages from this origin.
// Notifies JS about a watcher event via ExecuteJavaScript() and re-arms watcher.
void OnWatcherCallback(int callback_id, int watch_id, MojoResult result);

// Variant 2: Documenting only caller context rather than function actions and side effects
// Called when the user confirms the export flow.
- (void)startExportWithPasswords:
            (std::vector<password_manager::CredentialUIEntry>)passwords
           securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets;

// Variant 3: Undocumented UI/SDK side effects, default states, and conditional no-ops
void UpdateGeminiPageContext() override;

// Adds a custom view to the left of the location label with specified spacing.
- (void)addCustomLeadingView:(UIView*)view targetWidth:(CGFloat)targetWidth;

- (void)setLargestUndimmedDetentIdentifier:
    (UISheetPresentationControllerDetentIdentifier)identifier;

void ForcePageContextGeneration();
```

Good:

```cpp
// Variant 1: Caller-facing contract, purpose, and observable behavior across conditions
// If `origin_filter` is provided, the script will have no effect in pages from other origins.
// Notifies when watched signals are raised. `callback_id` identifies the JS callback.
void OnWatcherCallback(int callback_id, int watch_id, MojoResult result);

// Variant 2: Stating what the function does and its side effects
// Starts credential export. Converts credentials to the export format and invokes export manager.
- (void)startExportWithPasswords:
            (std::vector<password_manager::CredentialUIEntry>)passwords
           securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets;

// Variant 3: Side effects, default visibility, mode limitations, and no-ops stated up front
// Pushes page context updates (partial or full) to the SDK, and updates UI.
void UpdateGeminiPageContext() override;

// Adds a custom view to the left of the location label. The view is added hidden;
// callers show it once `targetWidth` has been laid out.
- (void)addCustomLeadingView:(UIView*)view targetWidth:(CGFloat)targetWidth;

// Sets the largest undimmed detent. Only applies to the system presentation; with
// the Lens Overlay presentation this is a no-op.
- (void)setLargestUndimmedDetentIdentifier:
    (UISheetPresentationControllerDetentIdentifier)identifier;

// Forces generation of the page context immediately. No-op once the page has
// finished loading.
void ForcePageContextGeneration();
```

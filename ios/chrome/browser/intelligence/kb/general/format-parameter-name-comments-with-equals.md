---
name: ios/format-parameter-name-comments-with-equals
description: When annotating function arguments at call sites, format inline comments as /*param_name=*/value synchronized with declarations, and omit them for self-describing expressions
rubric: readability
severity: should_fix
citations:
  - path: styleguide/c++/c++.md
    heading: Chromium C++ style guide
    quote: Chromium follows the Google C++ Style Guide unless an exception is listed below.
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium C++ style
    quote: Where appropriate, the Chromium C++ style style guide applies to Chromium Objective-C and (especially) Objective-C++
  - path: https://google.github.io/styleguide/cppguide.html#Function_Argument_Comments
    heading: Implementation Comments > Function Argument Comments
    quote: Do not use parameter name comments when the argument is already self-describing (e.g., when the argument is a variable whose name matches the parameter, or when the expression makes the purpose clear).
---

**Rule.** When annotating function arguments at call sites, format inline
parameter name comments as `/*param_name=*/value` using single-asterisk block
comment syntax, an equals sign, and no whitespace before the argument value. Do
not omit the equals sign (`/*param_name*/ value`), insert whitespace after or
around the equals sign (`/*param_name= */ value`), or use documentation comment
syntax (`/**param_name=*/value`). Additionally, ensure inline comment names
match the formal parameter name in the function declaration, and when renaming
parameters in function signatures, update inline argument comments across all
call sites to prevent stale names. Conversely, do not use inline parameter name
comments when passing arguments whose variable name, accessor call, or
expression already clearly communicates its role and intent; reserve inline
parameter comments for ambiguous literals (such as bare booleans, numbers, or
`nullptr`).

**Why.** Per the Google C++ Style Guide, formatting parameter name annotations
as `/*param_name=*/value` distinguishes them from general comments and allows
static analysis tooling to verify argument names against function declarations.
Non-standard syntax (such as missing equals signs, extra whitespace, or double
asterisks) breaks tooling, while stale or mismatched argument comments mislead
readers because comments are ignored by the compiler. However, adding parameter
comments before self-describing variables, accessor calls, or clear expressions
creates redundant visual clutter without providing new information.

## Examples

Bad:

```cpp
// Bad: Missing '=', extra whitespace, or double-asterisk documentation syntax.
ExecuteJavaScript(web_view, script, /*wait_async*/ false, error, /*result=*/nil);
RegisterObserver(/*observer*/ observer_ptr, /*notify_immediately= */ true);
base::CollapseWhitespaceASCII(
    original, /**trim_sequences_with_line_breaks=*/true);

// Bad: Stale parameter comment after renaming formal parameter to use_rich_extraction.
feature()->ExtractPageContext(
    web_frame, /*include_anchors=*/false, /*use_apc_v2=*/false);

// Bad: Redundant parameter comment on self-describing accessor method call.
int subtitle_id = enterprise_idle::GetIdleTimeoutActionsSubtitleId(
    actions,
    /*is_data_cleared_on_signout=*/auth_service
        ->ShouldClearDataForSignedInPeriodOnSignOut());
```

Good:

```cpp
// Good: Formatted as /*param_name=*/value with single asterisk, '=', and no spaces.
ExecuteJavaScript(web_view, script, /*wait_async=*/false, error, /*result=*/nil);
RegisterObserver(/*observer=*/observer_ptr, /*notify_immediately=*/true);
base::CollapseWhitespaceASCII(
    original, /*trim_sequences_with_line_breaks=*/true);

// Good: Synchronized with renamed parameter in function declaration.
feature()->ExtractPageContext(
    web_frame, /*include_anchors=*/false, /*use_rich_extraction=*/false);

// Good: Parameter comment omitted for self-describing accessor call.
int subtitle_id = enterprise_idle::GetIdleTimeoutActionsSubtitleId(
    actions, auth_service->ShouldClearDataForSignedInPeriodOnSignOut());
```

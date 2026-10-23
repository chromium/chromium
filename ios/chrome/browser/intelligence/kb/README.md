# iOS Code Review Knowledge Base

> **Temporary location.** This copy of the knowledge base lives under
> `//ios/chrome/browser/intelligence/` for end-to-end testing of the review
> integration. Its canonical home is `//ios/agents/knowledge_base/`
> (crbug.com/576358854); once that has landed, this directory and the `mixins:`
> lines that point here are removed.

This directory holds the knowledge base (KB) of code review rules for `//ios`.
Each rule is a short Markdown file that an automated reviewer evaluates against
the added lines of a CL; findings cite the rule by its `name` (for example
`[ios/coordinator-teardown-disconnects-mediator-in-stop]`).

The rules restate what the iOS documentation already requires — primarily
[`//ios/best_practices.md`](/ios/best_practices.md) and
[`//ios/architecture/`](/ios/architecture/README.md) — in a form that can be
matched to concrete code. When a rule and the documentation disagree, the
documentation wins and the rule must be fixed.

## Layout

```
ios/chrome/browser/intelligence/kb/
  COMMON_METADATA      # agentic_review capability_groups that wire the rules
  README.md
  chrome/              # rules specific to the //ios/chrome application layer
                       # (coordinators, mediators, consumers, commands, profiles,
                       # browser-scoped services, assets)
  general/             # rules for all of //ios (Objective-C++ and C++ hygiene,
                       # lifetimes, GN/DEPS, testing, comments)
```

`COMMON_METADATA` groups the rules by the files they apply to
(`path_expression`, matched against the repository-relative paths modified by a
CL) and is generated from the rule metadata; edit it by hand only to adjust a
rule's scope.

## Opting a directory in

The KB is **import-only**: nothing under `//ios` is reviewed automatically. A
directory that opts in to automated review lists this file as a mixin next to
its `agentic_review` block:

```
# //ios/chrome/browser/<feature>/DIR_METADATA
mixins: "//ios/chrome/browser/intelligence/kb/COMMON_METADATA"

agentic_review {}
```

Rules then apply to the files of that directory (and its subdirectories) that
match their path expressions. A directory can suppress an inherited rule with
`exclude_rules` in its own `agentic_review` block. See the
[DIR_METADATA documentation](https://chromium.googlesource.com/infra/infra/+/main/go/src/infra/tools/dirmd/README.md)
for the schema.

## Rule format

````
---
name: ios/<slug>              # cited in findings; the file is <slug>.md
description: <one-sentence summary of when and how the rule applies>
rubric: design                # correctness | safety | security | design |
                              # readability | testing | perf | change_scope
severity: should_fix          # blocking | should_fix | nit
citations:                    # optional governing doc/header references
- path: ios/best_practices.md
  heading: <Section Heading>
  quote: <Verbatim excerpt supporting the rule>
---

**Rule.** What to do or not do, precisely enough to be checked.

**Why.** The rationale, ending with a `See ...` pointer to the governing
documentation when one exists.

## Examples

Bad:

```cpp
// A minimal snippet showing the anti-pattern.
```

Good:

```cpp
// The same snippet, fixed.
```
````

Conventions that every rule follows:

- One convention per rule; prose wrapped at 80 columns; symbols in backticks.
- `blocking` is reserved for rules the documentation states as must/never or
  that prevent crashes and use-after-free; style conventions are `should_fix` or
  `nit`.
- Examples use real `//ios` APIs (verified against `origin/main`) or obvious
  placeholders (`FooMediator`, `kFooConstant`); no internal project names.
- Objective-C examples follow the Objective-C conventions (`BOOL`, acronyms in
  capitals such as `URL`); C++ examples follow Chromium C++ style.

## Adding or changing a rule

1. Check that the documentation supports the rule; if the guidance is missing
   from the docs, land it there first.
2. Add `<chrome|general>/<slug>.md` in the format above and reference it from a
   `capability_groups` entry in `COMMON_METADATA` whose `path_expression`
   matches only the files the rule can apply to (for example
   `ios/(?:.*/)?[^/]*_egtest\.mm` for EarlGrey rules).
3. Keep the rule consistent with its neighbours: grep `rules/` for the topic and
   fold the new guidance into an existing rule when it only adds a sentence; two
   rules that fire on the same line confuse authors.

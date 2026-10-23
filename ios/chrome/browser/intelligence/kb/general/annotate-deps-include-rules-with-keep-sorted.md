---
name: ios/annotate-deps-include-rules-with-keep-sorted
description: When modifying lists bounded by keep-sorted directives (such as DEPS include_rules), insert unique entries inside the block in alphabetical order, keeping baseline exclusions outside
rubric: readability
severity: should_fix
citations:
  - path: agents/projects/code-health/keep-sorted/SKILL.md
    heading: 3. Inject Directives & Sort
    quote: Sort the block alphabetically. If available, you can run the local `keep-sorted` formatter over the file; otherwise, sort the items manually in your script or output.
  - path: buildtools/checkdeps/README.md
    heading: Directory structure
    quote: The rules are processed in order. This means you can explicitly allow a higher directory and then take away permissions from sub-parts, or the reverse.
---

**Rule.** When adding or modifying entries in lists, arrays, or configuration
blocks bounded by `keep-sorted` directives (such as in `DEPS` files, `BUILD.gn`,
or source files), insert new entries inside the `keep-sorted start` and
`keep-sorted end` bounding directives rather than outside, and verify that all
entries are strictly ordered alphabetically and unique, removing any duplicate
lines. In `DEPS` files, enclose `include_rules` and `specific_include_rules`
lists with `# keep-sorted start` and `# keep-sorted end` directives, and ensure
explanatory comments (such as `# Use <alternative> instead` for banned includes)
directly precede the specific entry they annotate. Directory-level
`include_rules` entries name directories; header-level entries belong only in
`specific_include_rules`. Additionally, in `DEPS` files, keep entries with
dedicated rationale—such as baseline negative rules (`"-ios/web"`, `"-ios"`)
establishing broad constraints—outside and above the `keep-sorted` block so
alphabetical sorting does not displace them or their documentation below
positive rules.

**Why.** DEPS files and other configuration lists are not automatically
formatted by `git cl format`. The `keep-sorted` directives delimit a block that
tooling keeps strictly sorted and unique, which eliminates duplicate entries and
prevents merge conflicts from entries appended at the end of a list. Placing
additions outside the demarcated directives leaves them unsorted and invites a
second, divergent ordering, while duplicate entries are exactly the noise the
sorted block exists to prevent. Keeping explanatory comments directly preceding
annotated entries prevents comments from detaching during sorting, while keeping
baseline negative rules outside the sorted block preserves checkdeps dependency
constraint precedence and rationale.

## Examples

Bad:

```cpp
# Case 1: Entry added outside directives and duplicate entry inside block.
include_rules = [
  "+components/account_settings",
  # keep-sorted start
  "+ios/chrome/browser/history/model",
  "+ios/chrome/browser/reading_list/model",
  "+ios/chrome/browser/reading_list/model",
  # keep-sorted end
]

# Case 2: Baseline negative rule inside sorted block; header-level entry in
# include_rules; trailing detached comment.
include_rules = [
  # keep-sorted start
  "+ios/web/common",
  "-ios/web",
  "-ios/chrome/browser/authentication/test/signin_earl_grey_app_interface.h",
  # Use ios/chrome/browser/authentication/test/signin_earl_grey.h instead
  # keep-sorted end
]
```

Good:

```cpp
# Case 1: Unique directory-level entries inside keep-sorted block in
# alphabetical order.
include_rules = [
  # keep-sorted start
  "+components/account_settings",
  "+ios/chrome/browser/history/model",
  "+ios/chrome/browser/reading_list/model",
  # keep-sorted end
]

# Case 2: Baseline rule outside; header-level entries in
# specific_include_rules; comments directly precede annotated entries.
include_rules = [
  # common interfaces cannot depend on private web code.
  "-ios/web",

  # keep-sorted start
  "+ios/web/common",
  # keep-sorted end
]

specific_include_rules = {
  ".*_egtest\\.mm": [
    # keep-sorted start
    # Use ios/chrome/browser/authentication/test/signin_earl_grey.h instead
    "-ios/chrome/browser/authentication/test/signin_earl_grey_app_interface.h",
    # keep-sorted end
  ],
}
```

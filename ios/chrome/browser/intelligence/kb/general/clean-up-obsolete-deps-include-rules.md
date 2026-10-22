---
name: ios/clean-up-obsolete-deps-include-rules
rubric: design
severity: should_fix
---

**Rule.** In in-tree `DEPS` files, only permit directory paths (in
`include_rules`) and single headers (in `specific_include_rules`) that are
actively included or imported by local sources; do not add speculative or unused
entries. Because include rules recursively permit all descendant paths, do not
add redundant child subdirectory entries when an enclosing ancestor directory is
already permitted within the file or inherited from an ancestor `DEPS` file.
When refactoring or migrating dependencies, prune obsolete or orphaned include
rules while keeping active entries until all referencing files are migrated. In
`specific_include_rules`, scope file exceptions to the narrowest required header
rather than allowing broad directories.

**Why.** In-tree `DEPS` files enforce architectural boundaries, module layering,
and isolation via `checkdeps`. Adding speculative or unused allow rules
unnecessarily widens a directory's dependency surface area, obscuring the true
dependency graph and risking improper architectural couplings (such as UI layers
importing coordinator headers) going undetected. Furthermore, specifying
redundant child subdirectories when an ancestor is already allowed clutters
configuration without expanding access, while failing to prune orphaned rules
leaves stale exceptions in place.

## Examples

Bad:

```cpp
# Redundant child subfolder when the parent directory is already allowed, an
# unused entry, and a single header listed at directory level:
# In ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/DEPS:
include_rules = [
  "+ios/chrome/browser/authentication/ui_bundled",
  "+ios/chrome/browser/authentication/ui_bundled/signin",  # Redundant child path
  "+ios/chrome/browser/intelligence/features",  # Unused by sources in toolbars/
  "+ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_entrypoint_view.h",  # Used by one file
]

# Overly broad directory rule in specific_include_rules:
# In ios/chrome/browser/main/model/DEPS:
specific_include_rules = {
  "^browser_agent_util\\.mm": [
    "+ios/chrome/browser/toolbar/legacy/ui_bundled/fullscreen/toolbars_size_browser_agent.h",
    "+ios/chrome/browser/toolbar/legacy/ui_bundled",  # Broad directory rule when only one header is needed
  ],
}
```

Good:

```cpp
# Parent directory allows subdirectories recursively; unused rules omitted; the
# single header is scoped to the one file that imports it:
# In ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/DEPS:
include_rules = [
  "+ios/chrome/browser/authentication/ui_bundled",
]

specific_include_rules = {
  "^tab_grid_top_toolbar\\.mm": [
    "+ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_entrypoint_view.h",
  ],
}

# Retain only the narrowest required header exception:
# In ios/chrome/browser/main/model/DEPS:
specific_include_rules = {
  "^browser_agent_util\\.mm": [
    "+ios/chrome/browser/toolbar/legacy/ui_bundled/fullscreen/toolbars_size_browser_agent.h",
  ],
}
```

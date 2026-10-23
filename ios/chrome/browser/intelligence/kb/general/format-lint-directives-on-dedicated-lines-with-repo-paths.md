---
name: ios/format-lint-directives-on-dedicated-lines-with-repo-paths
description: When adding LINT synchronization guards, format directives on dedicated lines with repo-relative paths, explicit labels, remote companion files, comma-separated targets, and no redundant comments
rubric: readability
severity: should_fix
citations:
  - path: agents/projects/code-health/lint-sync/references/patterns.md
    heading: Syntax Rules
    quote: The linter links files by cross-referencing labels. Follow these syntax rules to format your `LINT.IfChange` and `LINT.ThenChange` guards correctly.
  - path: tools/metrics/histograms/README.md
    heading: Usage
    quote: The `LINT.*` comments point between the code and XML definitions of the enum, to encourage them to be kept in sync.
---

**Rule.** When adding or modifying LINT.IfChange and LINT.ThenChange
synchronization guards:

1. Place directives on their own dedicated comment lines rather than appending
   them inline to preceding code or prose comments.
2. Specify the target as a correct repository-root-relative file path
   (conventionally with a leading `//` or `/`, e.g.,
   `//tools/metrics/histograms/metadata/ios/enums.xml`), never as a path
   relative to the current file or a path that does not exist.
3. Provide explicit local labels on `LINT.IfChange(<LocalLabel>)` and remote
   target labels on `LINT.ThenChange(<RemoteFilePath>:<RemoteLabel>)` rather
   than leaving guards unlabelled.
4. Ensure `LINT.ThenChange(...)` points to the remote companion file (such as a
   header, XML metadata, or counterpart implementation) rather than
   self-referencing the file currently being edited.
5. When synchronizing a block with multiple files or labels, specify all targets
   as a comma-separated list inside `LINT.ThenChange(...)` (e.g.,
   `LINT.ThenChange(//path/to/file1:Label1, //path/to/file2:Label2)`) rather
   than listing extra targets in trailing comments or informal notes outside the
   directive.
6. Omit redundant manual prose comments stating that the code or definition must
   be kept in sync, relying entirely on the LINT directives to document and
   enforce synchronization.

**Why.** Presubmit linters match LINT.IfChange and LINT.ThenChange directives
using strict line-comment syntax patterns: - Appending directives inline
prevents linters from parsing the guard. - Paths relative to the current file,
or paths that do not resolve from the repository root, fail path resolution in
repo root checks. - Specifying explicit local and remote labels (`<LocalLabel>`
and `<RemoteFilePath>:<RemoteLabel>`) allows linters to disambiguate multiple
blocks within a file and bind directly to specific entries in large files like
enums.xml. - Pointing LINT.ThenChange to the current file creates a
self-referencing loop that fails to verify external companion files, allowing
companion definitions to silently drift. - Listing multiple targets in trailing
comments or notes outside LINT.ThenChange(...) leaves those additional targets
unguarded because linters only parse targets inside the argument list. - Because
LINT directives mechanically declare and enforce synchronization, redundant
manual 'keep in sync' prose comments add clutter and risk becoming stale when
target files or labels are updated.

## Examples

Bad:

```cpp
// 1. Inline directive, path relative to the current file, and self-reference:
enum class ScanCardOfferToSaveAction { ... }; // LINT.IfChange
// LINT.ThenChange(payments_scan_save_and_fill_edit_view_controller.h:ScanCardOfferToSaveAction)

// 2. Redundant sync comment and missing remote target label:
// Must be kept in sync with IOSDefaultBrowserPromoSurface in enums.xml.
// LINT.IfChange(IOSDefaultBrowserPromoSurface)
enum class IOSDefaultBrowserPromoSurface { ... };
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml)

// 3. Trailing comment for multiple targets instead of comma-separated list:
// LINT.IfChange(BestFeaturesItemType)
enum class BestFeaturesItemType { ... };
// LINT.ThenChange(/ios/chrome/browser/welcome_back/model/welcome_back_prefs.mm:IntToBestFeaturesItemType)
// and also /tools/metrics/histograms/metadata/ios/enums.xml:IOSBestFeaturesItemType
```

Good:

```cpp
// 1. Dedicated lines, repo-root-relative path, explicit labels, remote file:
// LINT.IfChange(ScanCardOfferToSaveAction)
enum class ScanCardOfferToSaveAction { ... };
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ScanCardOfferToSaveAction)

// 2. Omit redundant prose comment; rely on labelled LINT directive:
// LINT.IfChange(IOSDefaultBrowserPromoSurface)
enum class IOSDefaultBrowserPromoSurface { ... };
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSDefaultBrowserPromoSurface)

// 3. Multi-target sync as comma-separated list inside LINT.ThenChange:
// LINT.IfChange(BestFeaturesItemType)
enum class BestFeaturesItemType { ... };
// LINT.ThenChange(
// /ios/chrome/browser/welcome_back/model/welcome_back_prefs.mm:IntToBestFeaturesItemType,
// /tools/metrics/histograms/metadata/ios/enums.xml:IOSBestFeaturesItemType)
```

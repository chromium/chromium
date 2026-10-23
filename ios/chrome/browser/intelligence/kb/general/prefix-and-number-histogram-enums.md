---
name: ios/prefix-and-number-histogram-enums
description: When defining or modifying UMA histogram enums, add the persistence comment and logged histogram name, number entries sequentially from 0 with kMaxValue, update enums.xml in the same CL, and link LINT.IfChange/ThenChange guards
rubric: correctness
severity: blocking
citations:
  - path: tools/metrics/histograms/README.md
    heading: Requirements
    quote: 'Enums logged in histograms must: - be prefixed with the comment: ```c++ // These values are persisted to logs. Entries should not be renumbered and // numeric values should never be reused. ``` - be numbered starting from `0`. Note this bullet point does *not* apply for enums logged with sparse histograms. - have enumerators with explicit values (`= 0`, `= 1`, `= 2`) to make it clear that the actual values are important. This also makes it easy to match the values between the C++/Java definition and [histograms.xml](./histograms.xml). - not renumber or reuse enumerator values. When adding a new enumerator, append the new enumerator to the end. When removing an unused enumerator, comment it out, making it clear the value was previously used. If your enum histogram has a catch-all / miscellaneous bucket, put that bucket first (`= 0`). This makes the bucket easy to find on the dashboard if additional buckets are added later.'
  - path: tools/metrics/histograms/review_guidelines.md
    heading: Histogram is setup correctly.
    quote: Verify that for an enum histogram, the enum described in enums.xml and the enum defined in the client code matches. Furthermore, the enum in the code should have a comment mentioning that the values must not be changes and also that additions to the enum should be synced to enums.xml.
  - path: tools/metrics/histograms/README.md
    heading: Provide Clear Units or Enum Labels
    quote: For enumerated histograms, including boolean and sparse histograms, provide an enum= attribute mapping enum values to semantically contentful labels. Define the <enum> in enums.xml if none of the existing enums are a good fit.
  - path: tools/metrics/histograms/README.md
    heading: Usage
    quote: The `LINT.*` comments point between the code and XML definitions of the enum, to encourage them to be kept in sync.
  - path: tools/metrics/histograms/README.md
    heading: Enum Histograms > Requirements
    quote: This also makes it easy to match the values between the C++/Java definition and histograms.xml.
---

**Rule.** When defining or extending an enum persisted to histograms or logs
(such as for `base::UmaHistogramEnumeration` or `UMA_HISTOGRAM_ENUMERATION`):

1. Prefix the enum declaration with the standard persistence warning comment:

`// These values are persisted to logs. Entries should not be renumbered and`
`// numeric values should never be reused.`

2. Explicitly assign sequential numeric values to every enumerator starting from
   0 (e.g., `= 0`, `= 1`, `= 2`, unless logged with sparse histograms) rather
   than relying on implicit compiler incrementing.
3. Additionally, if the enum includes a catch-all, miscellaneous, or fallback
   bucket (such as `kOther`, `kUnknown`, `kMisc`, or `kFallback`), place that
   bucket first and assign it value 0 (`= 0`). Do not append catch-all buckets
   at the end or assign them non-zero values.
4. When extending the enum, append new enumerators to the end with new numeric
   values and update `kMaxValue`. Never insert entries in the middle, renumber
   or shift existing entries (such as sentinel or unknown values), or reuse
   numeric values previously assigned to obsolete entries; comment out or retain
   retired entries.
5. In the enum's comment, name the histogram(s) it is logged to, and update the
   matching `<enum>` in the relevant `enums.xml` (for `//ios` code, usually
   `tools/metrics/histograms/metadata/ios/enums.xml`) in the same CL. Wrap the
   source enum and the XML definition in reciprocal `LINT.IfChange(<label>)` /
   `LINT.ThenChange(<path>:<label>)` comments, where `<path>` is a correct
   repository-root-relative path (either the `//` or the `/` prefix is fine).

**Why.** Histogram and log metrics persist raw numeric values in telemetry
databases and dashboards rather than string identifiers. Omitting explicit
assignments risks silent shifts from compiler auto-incrementing when entries are
reordered or inserted. Renumbering, shifting, or reusing values retroactively
corrupts historical telemetry data across releases. Assigning 0 to a catch-all
or miscellaneous bucket keeps it at a predictable, fixed position on telemetry
dashboards and makes it easy to locate as new enumerators are appended over
time. Explicit numbering and the standard persistence warning comment ensure
developer awareness of telemetry immutability. `enums.xml` maps the numeric
buckets to human-readable labels on UMA dashboards, so an enumerator missing
from it shows up as an unlabeled raw number; naming the logged histogram(s) in
the code lets engineers trace a metric without guesswork, and reciprocal LINT
guards keep the code and the XML definition from drifting apart.

## Examples

Bad:

```cpp
// Bad: Catch-all placed at end with non-zero value, missing persistence comment.
enum class DismissReason {
  kUserDismissed = 0,
  kOther = 1,
  kMaxValue = kOther,
};

// Bad: Inserting in middle shifts existing values instead of appending.
// These values are persisted to logs. Entries should not be renumbered and...
enum class DragDropType {
  kUnknown = 0,
  kRawFile = 1,  // Inserted here: shifted the existing kText from 1 to 2!
  kText = 2,
  kMaxValue = kText,
};

// Bad: New bucket added without naming the logged histogram, without LINT
// guards, and with no matching enums.xml update in the CL, so the new value
// shows up as an unlabeled bucket on dashboards.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FooDismissReason {
  kOther = 0,
  kUserDismissed = 1,
  kSwipedAway = 2,
  kMaxValue = kSwipedAway,
};
```

Good:

```cpp
// Good: Catch-all placed first at 0 with standard persistence comment.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DismissReason {
  kOther = 0,
  kUserDismissed = 1,
  kMaxValue = kUserDismissed,
};

// Good: New entry appended to the end; existing values keep their numbers.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DragDropType {
  kUnknown = 0,
  kText = 1,
  kRawFile = 2,  // Appended to the end without shifting existing values.
  kMaxValue = kRawFile,
};

// Good: Names the logged histogram and links the enum to enums.xml with
// reciprocal LINT guards; enums.xml is updated in the same CL.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Logged as IOSFooDismissReason for the IOS.Foo.DismissReason histogram.
// LINT.IfChange(FooDismissReason)
enum class FooDismissReason {
  kOther = 0,
  kUserDismissed = 1,
  kSwipedAway = 2,
  kMaxValue = kSwipedAway,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSFooDismissReason)

<!-- In tools/metrics/histograms/metadata/ios/enums.xml, same CL: -->
<!-- LINT.IfChange(IOSFooDismissReason) -->
<enum name="IOSFooDismissReason">
  <int value="0" label="Other"/>
  <int value="1" label="User dismissed"/>
  <int value="2" label="Swiped away"/>
</enum>
<!-- LINT.ThenChange(//ios/chrome/browser/foo/model/foo_metrics.h:FooDismissReason) -->
```

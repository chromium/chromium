---
name: privacy/browsing-data-remover-time-range-filtering
description: >-
  Checks that BrowsingDataRemover and BrowsingDataRemoverDelegate integrations
  respect the [delete_begin, delete_end) time range for timestamped data stores
  and handle timeless stores explicitly.
rubric: privacy
severity: should_fix
---

**Rule.** Flag `BrowsingDataRemover` and `BrowsingDataRemoverDelegate`
integrations (`content/public/browser/browsing_data_remover.h`) that do not
respect the `[delete_begin, delete_end)` time range:

1. **Timestamped stores:** Deletions MUST be filtered using the **last
   modified** timestamp (or the **creation** timestamp if last modified is not
   tracked). `delete_begin` and `delete_end` MUST NOT be ignored when the
   underlying data store tracks timestamps.
2. **Timeless stores:** Only when a store genuinely has no timestamps (e.g.,
   aggregate statistics or timeless state) may the integration either delete all
   data unconditionally or gate deletion on "All time" (`delete_begin.is_null()`
   and `delete_end == base::Time::Max()`). The chosen behavior MUST be explicit
   in the code.

**Anchor.** Anchor the finding on the deletion call in
`BrowsingDataRemoverImpl`, `StoragePartitionImpl`, or
`ChromeBrowsingDataRemoverDelegate` that omits or ignores `delete_begin` and
`delete_end`.

**Why.** Ignoring `delete_begin` and `delete_end` on timestamped data causes
time-scoped deletions (such as "Last hour") to wipe all historical user data.

## Examples

Bad:

```cpp
// The entire database is wiped even when the user only requested deleting the
// last hour of browsing data.
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_HISTORY) {
  media_history_service->ClearAllData();
}
```

Good:

```cpp
// Deletion is restricted to entries modified within [delete_begin, delete_end).
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_HISTORY) {
  media_history_service->ClearDataBetween(
      delete_begin, delete_end,
      CreateTaskCompletionClosure(TracingDataType::kMediaHistory));
}
```

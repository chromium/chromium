---
name: ios/batch-expensive-bookmark-model-observer-updates
description: When implementing model observers that perform expensive recomputations or reloads on models with batch-operation callbacks (such as BookmarkModelObserver::ExtensiveBookmarkChangesBeginning/Ended), suspend updates during batch operations and reload once on completion
rubric: design
severity: should_fix
---

**Rule.** When implementing model observers whose callbacks perform expensive
recomputations or full reloads (such as full tree traversals, aggregate metrics,
or UI data-source rebuilds) on models that expose batch-operation brackets (such
as `BookmarkModelObserver::ExtensiveBookmarkChangesBeginning()` /
`ExtensiveBookmarkChangesEnded()` or
`WebStateListObserver::WillBeginBatchOperation()` / `BatchOperationEnded()`),
suppress per-item updates while a batch operation is in progress and execute the
consolidated update once when the batch operation ends.

**Why.** During initial sync, bulk imports, tab restoration, or mass deletions,
models such as `BookmarkModel` or `WebStateList` fire per-item observer
callbacks (such as `BookmarkNodeAdded()` or `BookmarkNodeRemoved()`) repeatedly
for each individual mutation. Re-traversing the model or recalculating expensive
state on every per-item callback during a bulk operation causes O(N^2) work and
main-thread UI jank.

## Examples

Bad:

```cpp
void BookmarkModelMetricsService::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  // Re-traversing the entire tree on every added node during bulk sync.
  UpdateBookmarkNodesCrashKey();
}
```

Good:

```cpp
void BookmarkModelMetricsService::UpdateBookmarkNodesCrashKey() {
  if (in_extensive_changes_) {
    return;
  }
  crash_keys::SetBookmarkNodesCount(
      bookmark_model_->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes());
}

void BookmarkModelMetricsService::ExtensiveBookmarkChangesBeginning() {
  in_extensive_changes_ = true;
}

void BookmarkModelMetricsService::ExtensiveBookmarkChangesEnded() {
  in_extensive_changes_ = false;
  UpdateBookmarkNodesCrashKey();
}
```

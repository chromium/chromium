---
name: privacy/browsing-data-remover-history-deletion-observation
description: >-
  Checks that features storing history-like data observe HistoryService
  deletions (including individual URL deletions and expiration) instead of
  relying only on DATA_TYPE_HISTORY in ChromeBrowsingDataRemoverDelegate.
rubric: privacy
severity: should_fix
---

**Rule.** Flag features that store history-like data (data automatically
collected while browsing URLs, origins, or domains) but only clear it in
`ChromeBrowsingDataRemoverDelegate` under
`chrome_browsing_data_remover::DATA_TYPE_HISTORY`. Such features MUST observe
`history::HistoryService` instead:

1. **Observe `HistoryService`:** The feature MUST implement
   `history::HistoryServiceObserver::OnHistoryDeletions(history::HistoryService*, const history::DeletionInfo&)`
   (`components/history/core/browser/history_service_observer.h`) on a service
   created with the `BrowserContext`, or add its cleanup to
   `BrowsingDataHistoryObserverService::OnHistoryDeletions`
   (`chrome/browser/browsing_data/browsing_data_history_observer_service.cc`).
   Because `DATA_TYPE_HISTORY` already triggers `OnHistoryDeletions`, a separate
   `DATA_TYPE_HISTORY` deletion in `ChromeBrowsingDataRemoverDelegate` is
   generally unnecessary.
2. **Time-range and per-URL deletions:** `OnHistoryDeletions` MUST handle both
   time-range deletions (`deletion_info.IsAllHistory()` and
   `deletion_info.time_range().IsValid()`) and per-URL deletions
   (`deletion_info.deleted_rows()`).
3. **Origin- or domain-keyed data:** If the data is keyed by origin, the
   implementation MUST inspect `deletion_info.deleted_urls_origin_map()` (and
   `deletion_info.favicon_urls()` if applicable) and delete the origin's data
   when `count_and_last_visit.first == 0`. If the data is keyed by domain
   (eTLD+1) or host, it MUST delete the domain's data when all URLs on that
   domain are removed from local history (or over-delete when any URL on that
   domain is deleted if per-domain counts are not tracked).

**Anchor.** Anchor the finding on the `DATA_TYPE_HISTORY` deletion block in
`ChromeBrowsingDataRemoverDelegate` or on the incomplete `OnHistoryDeletions`
implementation.

**Why.** Clearing history-like data only in `BrowsingDataRemover` leaves records
of visited URLs, origins, and domains intact when users delete individual items
in `chrome://history` or when history expires automatically.

## Examples

Bad:

```cpp
// URL/origin metadata is only deleted when BrowsingDataRemover runs with
// DATA_TYPE_HISTORY, leaving entries intact when individual URLs are deleted
// from chrome://history or expire from HistoryService.
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_HISTORY) {
  origin_visit_tracker->ClearVisits(delete_begin, delete_end,
                                    filter_builder->BuildUrlFilter());
}
```

Good:

```cpp
// HistoryServiceObserver::OnHistoryDeletions is observed (on a service created
// with the profile, or via BrowsingDataHistoryObserverService), which handles
// Clear Browsing Data (DATA_TYPE_HISTORY), individual URL deletions, and
// history expiration.
void OriginVisitTracker::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    ClearAllVisits();
    return;
  }
  if (deletion_info.time_range().IsValid()) {
    ClearVisitsBetween(deletion_info.time_range().begin(),
                       deletion_info.time_range().end());
    return;
  }
  for (const auto& [origin, count_and_last_visit] :
       deletion_info.deleted_urls_origin_map()) {
    // No URLs for this origin remain in local history.
    if (count_and_last_visit.first == 0) {
      ClearOrigin(url::Origin::Create(origin));
    }
  }
}
```

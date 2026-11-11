---
name: privacy/browsing-data-remover-origin-domain-filtering
description: >-
  Checks that deletions of FILTERABLE_DATA_TYPES (Cookies/Site Data and Cache)
  honor BrowsingDataFilterBuilder for per-origin and per-domain deletions and
  never ignore the filter.
rubric: privacy
severity: should_fix
---

**Rule.** Flag deletions of datatypes cleared with Cookies and Site Data
(`DATA_TYPE_SITE_DATA`, `DATA_TYPE_COOKIES`, `DATA_TYPE_DOM_STORAGE`,
`DATA_TYPE_EMBEDDER_DOM_STORAGE`) or Cache (`DATA_TYPE_CACHE`), which make up
`FILTERABLE_DATA_TYPES`, that do not support
`content::BrowsingDataFilterBuilder`
(`content/public/browser/browsing_data_filter_builder.h`). Evaluate the
integration against this order of preference:

1. **PREFERRED (Support per-origin/per-domain deletions):** When the store is
   keyed by URL, origin, domain, or `blink::StorageKey`, the deletion MUST pass
   a filter built by `filter_builder` (`BuildStorageKeyFilter()`, which is
   preferred over the deprecated `BuildUrlFilter()`;
   `BuildNetworkServiceFilter()`; `BuildCookieDeletionFilter()`; or
   `browsing_data::CreateWebsiteSettingsFilter(filter_builder)`). Filters MUST
   NOT be manually reconstructed from `filter_builder->GetOrigins()` or
   `GetRegisterableDomains()`, which misses `Mode::kPreserve`, domain matching,
   and partitioned `StorageKey`s.
2. **ACCEPTABLE (Only when the store has no origin/domain keys, such as
   profile-global metadata):** The deletion MUST be gated on
   `filter_builder->MatchesMostOriginsAndDomains()` or
   `filter_builder->MatchesAllOriginsAndDomains()` so that single-origin
   deletions (e.g., `Clear-Site-Data` headers) do not reset global state across
   all sites. Before accepting this option, inspect the surrounding code to
   verify that option #1 is not possible.
3. **DO NOT DO (Ignore filters):** A deletion MUST NEVER ignore `filter_builder`
   and unconditionally delete data across all origins.

**Anchor.** Anchor the finding on the deletion call or `filter_builder`
condition inside `BrowsingDataRemoverImpl`, `StoragePartitionImpl`, or
`ChromeBrowsingDataRemoverDelegate`.

**Why.** Ignoring `filter_builder` wipes data across all websites during a
single-site `Clear-Site-Data` header or Site Settings deletion, while skipping
filters on origin-keyed stores leaves tracking data behind.

## Examples

Bad:

```cpp
// DO NOT DO: `filter_builder` is ignored and data for all sites is wiped even
// when triggered by a single site's Clear-Site-Data header.
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_SITE_DATA) {
  site_data_service->ClearData(delete_begin, delete_end);
}
```

Bad:

```cpp
// Per-origin/per-domain deletion is skipped completely for origin-keyed data
// even though per-origin filtering is possible (option #1 applies).
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_SITE_DATA) {
  if (filter_builder->MatchesAllOriginsAndDomains()) {
    origin_keyed_service->ClearAll(delete_begin, delete_end);
  }
}
```

Bad:

```cpp
// GetOrigins() is inspected manually instead of using a Build*Filter() method,
// missing Mode::kPreserve and registrable domain filters.
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_SITE_DATA) {
  for (const url::Origin& origin : filter_builder->GetOrigins()) {
    third_site_service->ClearOrigin(origin);
  }
}
```

Good:

```cpp
// PREFERRED (origin/domain-keyed data): The filter predicate built by
// `filter_builder` is passed to the service.
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_SITE_DATA) {
  site_data_service->ClearData(
      delete_begin, delete_end, filter_builder->BuildUrlFilter(),
      CreateTaskCompletionClosure(TracingDataType::kSiteDataService));
}
```

Good:

```cpp
// ACCEPTABLE (strictly profile-global unkeyed state where option #1 is not
// possible): The reset is gated on MatchesMostOriginsAndDomains() /
// MatchesAllOriginsAndDomains() so single-origin Clear-Site-Data headers do not
// reset profile-wide state.
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_SITE_DATA) {
  if (filter_builder->MatchesMostOriginsAndDomains()) {
    RegenerateProfileGlobalNoiseToken(profile_);
  }
}
```

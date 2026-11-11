---
name: privacy/browsing-data-remover-datatype-selection
description: >-
  Checks that BrowsingDataRemover integrations use the DataType bitmask that
  matches the data's privacy semantics (site data, cache, history, or explicit
  user data) and that BrowsingDataCounter results stay accurate.
rubric: privacy
severity: should_fix
---

**Rule.** Flag `BrowsingDataRemover` integrations whose `DataType` bitmask
(`content/public/browser/browsing_data_remover.h`,
`chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h`) does
not match the data's privacy semantics:

- **Cookies and Site Data** (`content::BrowsingDataRemover::DATA_TYPE_COOKIES`,
  `DATA_TYPE_DOM_STORAGE`, `DATA_TYPE_EMBEDDER_DOM_STORAGE`, or
  `chrome_browsing_data_remover::DATA_TYPE_SITE_DATA`): Covers website storage
  (e.g., storage APIs, workers, DRM licenses) and browser state affecting
  website behavior (e.g., site/media engagement, permission blocking,
  anti-fingerprinting tokens). Web-platform storage features should also
  integrate with `BrowsingDataModel`
  (`components/browsing_data/content/browsing_data_model.h`); however, because
  that integration may land in a separate CL, you MUST NOT flag a missing
  `BrowsingDataModel` integration unless the CL modifies `BrowsingDataModel`
  with an incorrect integration.
- **Cache** (`content::BrowsingDataRemover::DATA_TYPE_CACHE`): Covers ONLY
  optimization caches that do not alter functional site behavior when cleared
  (e.g., HTTP/code caches, DNS, HSTS, prefetch/prerender state).
- **History and Downloads** (`chrome_browsing_data_remover::DATA_TYPE_HISTORY`,
  `content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS`): `DATA_TYPE_HISTORY`
  covers passive browsing traces that are not web-platform storage or caches
  (e.g., visit/media history, favicons, auto-discovered search engines);
  `DATA_TYPE_DOWNLOADS` covers download history records.
- **Explicit User Data and URL-Keyed Settings** (`DATA_TYPE_PASSWORDS`,
  `DATA_TYPE_ACCOUNT_PASSWORDS`, `DATA_TYPE_FORM_DATA`,
  `DATA_TYPE_CONTENT_SETTINGS`, `DATA_TYPE_SITE_USAGE_DATA`): Data created by
  explicit user action (such as passwords, autofill form data, per-site
  permissions, or zoom levels) MUST NOT be deleted under the three pre-checked
  basic Clear Browsing Data categories (`DATA_TYPE_HISTORY`, `DATA_TYPE_CACHE`,
  or `DATA_TYPE_COOKIES` / `DATA_TYPE_SITE_DATA`); it MUST use its dedicated
  datatype. Global browser settings (not tied to URLs or browsing activity) MUST
  NEVER be deleted in Clear Browsing Data.

**Anchor.** Anchor the finding on the `if (remove_mask & ...)` condition or
deletion call using the wrong `DataType` bitmask, or on the modified
`BrowsingDataModel` / `BrowsingDataCounter` line.

**Why.** Mapping a data store to the wrong `DataType` either destroys explicit
user data and site storage during routine cache or history clearing, or leaves
tracking state intact when users clear site data.

## Examples

Bad:

```cpp
// Cookies & Site Data: State that affects website behavior or cross-site
// tracking is cleared under DATA_TYPE_CACHE (or DATA_TYPE_HISTORY).
if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_CACHE) {
  site_behavior_token_service->ClearTokens(
      delete_begin, delete_end, filter_builder->BuildUrlFilter());
}
```

Bad:

```cpp
// Cache: A pure network/computation optimization cache is cleared under
// DATA_TYPE_HISTORY instead of DATA_TYPE_CACHE.
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_HISTORY) {
  dictionary_preload_cache->ClearCache(
      delete_begin, delete_end, filter_builder->BuildUrlFilter());
}
```

Bad:

```cpp
// History: Passive navigation/visit history traces are cleared under
// DATA_TYPE_CACHE instead of DATA_TYPE_HISTORY (or a HistoryServiceObserver).
if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_CACHE) {
  page_visit_metadata_service->ClearHistory(delete_begin, delete_end);
}
```

Bad:

```cpp
// Explicit User Data / Settings: Explicit user-configured per-site settings
// (likewise passwords or autofill) are wiped under a pre-checked basic category
// (DATA_TYPE_HISTORY, DATA_TYPE_CACHE, or DATA_TYPE_SITE_DATA).
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_SITE_DATA) {
  host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE, delete_begin, delete_end,
      website_settings_filter);
}
```

Good:

```cpp
// Cookies & Site Data: Website storage and browser state affecting website
// behavior are cleared under DATA_TYPE_SITE_DATA / DATA_TYPE_COOKIES.
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_SITE_DATA) {
  site_behavior_token_service->ClearTokens(
      delete_begin, delete_end, filter_builder->BuildUrlFilter());
}
```

Good:

```cpp
// Cache: Optimization caches that do not alter functional site behavior are
// cleared under DATA_TYPE_CACHE.
if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_CACHE) {
  dictionary_preload_cache->ClearCache(
      delete_begin, delete_end, filter_builder->BuildUrlFilter());
}
```

Good:

```cpp
// History: Passive browsing traces are cleared with history.
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_HISTORY) {
  page_visit_metadata_service->ClearHistory(delete_begin, delete_end);
}
```

Good:

```cpp
// Explicit User Data / Settings: Explicit per-site user settings are cleared
// under DATA_TYPE_SITE_USAGE_DATA / DATA_TYPE_CONTENT_SETTINGS (and passwords
// or autofill under DATA_TYPE_PASSWORDS / DATA_TYPE_FORM_DATA).
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_SITE_USAGE_DATA) {
  host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE, delete_begin, delete_end,
      website_settings_filter);
}
```

---
name: privacy/browsing-data-remover-content-vs-chrome-layer
description: >-
  Checks that BrowsingDataRemover integrations are layered correctly between
  //content and //chrome, delegate feature-specific deletion to the owning
  service, and represent deletion targets as DataType constants rather than
  custom Remove() parameters.
rubric: design
severity: should_fix
---

**Rule.** Flag `BrowsingDataRemover`
(`content/public/browser/browsing_data_remover.h`) integrations that violate any
of the following layering and modularity constraints:

1. **Layering:** `//content`-level and web-platform data types MUST be deleted
   in `content::BrowsingDataRemoverImpl::RemoveImpl`
   (`content/browser/browsing_data/browsing_data_remover_impl.cc`) or
   `content::StoragePartitionImpl::ClearDataImpl`. They MUST NOT be deleted in
   `ChromeBrowsingDataRemoverDelegate::RemoveEmbedderData`
   (`chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.cc`),
   which is reserved strictly for `//chrome`-specific features.
2. **Constants:** `//chrome`-specific `DataType` constants MUST be defined in
   `chrome_browsing_data_remover`
   (`chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h`,
   beyond `DATA_TYPE_CONTENT_END`), NEVER in `content::BrowsingDataRemover`.
3. **Thin orchestrators:** `BrowsingDataRemoverImpl` and
   `ChromeBrowsingDataRemoverDelegate` MUST remain thin orchestrators that
   delegate feature-specific deletion, filtering, and callback chaining to a
   single helper or method on the owning service or component. Multi-step
   feature-specific filtering or async callback chains MUST NOT be inlined in
   the remover.
4. **No custom parameters:** New deletion targets MUST be represented as
   `DataType` bitmask constants (or scoped via `BrowsingDataFilterBuilder` /
   `StoragePartitionConfig`). Custom parameters or ad-hoc `Remove*()` methods
   MUST NOT be added to `BrowsingDataRemover`.

**Anchor.** Anchor the finding on the misplaced `DataType` constant definition,
the misplaced or inlined deletion block in `ChromeBrowsingDataRemoverDelegate`
or `BrowsingDataRemoverImpl`, or the custom `BrowsingDataRemover::Remove*`
method declaration.

**Why.** Deleting web-platform data in `//chrome` leaves other `//content`
embedders (like Android WebView) unable to clear it, while inlining subsystem
logic or overloading `Remove()` breaks modularity and shared deletion entry
points.

## Examples

Bad:

```cpp
// content/public/browser/browsing_data_remover.h: Remove() is overloaded with
// feature-specific parameters instead of using a DataType bitmask constant (or
// a StoragePartitionConfig on BrowsingDataFilterBuilder).
virtual void RemoveForIsolatedApp(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    DataType remove_mask,
    const url::Origin& isolated_app_origin) = 0;
```

Bad:

```cpp
// chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.cc: A
// //content-level feature is cleared in ChromeBrowsingDataRemoverDelegate
// instead of BrowsingDataRemoverImpl, and multi-step feature-specific filtering
// and async callbacks are inlined in the remover instead of being encapsulated
// in the feature service.
if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_TOKEN_STORAGE) {
  token_service->GetIssuers(base::BindOnce(
      [](base::WeakPtr<TokenService> service,
         base::RepeatingCallback<bool(const GURL&)> url_filter,
         base::OnceClosure done, std::vector<url::Origin> issuers) {
        std::vector<url::Origin> to_delete;
        for (const auto& issuer : issuers) {
          if (url_filter.Run(issuer.GetURL())) {
            to_delete.push_back(issuer);
          }
        }
        service->DeleteTokensForIssuers(std::move(to_delete), std::move(done));
      },
      token_service->GetWeakPtr(), filter_builder->BuildUrlFilter(),
      CreateTaskCompletionClosure(TracingDataType::kTokenStorage)));
}
```

Good:

```cpp
// A new deletion target is represented as a DataType constant and scoped via
// BrowsingDataFilterBuilder::SetStoragePartitionConfig() (as done for
// DATA_TYPE_ISOLATED_WEB_APP_COOKIES) rather than by overloading
// BrowsingDataRemover::Remove().
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_ISOLATED_WEB_APP_COOKIES) {
  auto iwa_filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kPreserve);
  iwa_filter_builder->SetStoragePartitionConfig(iwa_partition);
  profile_->GetBrowsingDataRemover()->RemoveWithFilter(
      delete_begin, delete_end, content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      origin_type_mask, std::move(iwa_filter_builder));
}
```

Good:

```cpp
// chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.cc (for a
// //chrome-specific feature using an embedder DataType constant; the same shape
// applies in content/browser/browsing_data/browsing_data_remover_impl.cc for
// //content features): A single call delegates feature-specific logic to the
// owning service.
if (remove_mask & chrome_browsing_data_remover::DATA_TYPE_TOKEN_STORAGE) {
  token_service->ClearData(
      delete_begin, delete_end, filter_builder->BuildUrlFilter(),
      CreateTaskCompletionClosure(TracingDataType::kTokenStorage));
}
```

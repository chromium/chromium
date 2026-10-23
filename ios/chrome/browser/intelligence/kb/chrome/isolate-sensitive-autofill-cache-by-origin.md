---
name: ios/isolate-sensitive-autofill-cache-by-origin
description: When caching sensitive autofill data across frames, isolate and validate entries by security origin (url::Origin) rather than clearing the cache on frame changes
rubric: safety
severity: should_fix
---

**Rule.** In any tab-scoped sensitive autofill or credential cache (such as
unmasked virtual cards or CVCs stored on a `WebState`), associate each cached
entry with the security origin (`url::Origin`) where it was unmasked and
validate that origin on retrieval. Do not invalidate or clear the cache when the
active frame changes (e.g., via `GetFrameId()`); clear cache entries on
cross-document navigation instead (e.g., in `DidFinishNavigation` when
`!navigation_context->IsSameDocument()`).

**Why.** Clearing cached card or credential data on frame changes breaks
multi-frame payment forms where card numbers and CVC inputs reside in separate
same-origin iframes, while returning cached entries without checking the caller
frame's `url::Origin` risks cross-origin data disclosure. Isolating cache access
by security origin preserves multi-frame form flows while strictly preventing
cross-origin leaks.

## Examples

Bad:

```cpp
// Bad: Returning cached card data without verifying the requesting frame's origin.
const autofill::CreditCard* ManualFillVirtualCardCache::GetUnmaskedCard(
    const std::string& server_id) const {
  auto it = card_map_.find(server_id);
  if (it != card_map_.end()) {
    return &it->second.card;
  }
  return nullptr;
}
```

Good:

```cpp
// Good: Isolate cached data by origin and verify same-origin on retrieval.
void ManualFillVirtualCardCache::CacheUnmaskedCard(
    const autofill::CreditCard& card,
    url::Origin origin) {
  card_map_[card.server_id()] = {card, std::move(origin)};
}

const autofill::CreditCard* ManualFillVirtualCardCache::GetUnmaskedCard(
    const std::string& server_id,
    const url::Origin& current_origin) const {
  auto it = card_map_.find(server_id);
  if (it != card_map_.end() &&
      it->second.origin.IsSameOriginWith(current_origin)) {
    return &it->second.card;
  }
  return nullptr;
}
```

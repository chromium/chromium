---
name: net/device_bound_sessions/configure-every-fetch-and-recheck-redirects
rubric: security
severity: should_fix
---

**Rule.** List every place a DBSC `URLFetcher` is created: registration,
refresh, transient-error retry and the `.well-known` discovery requests. Each
one must go through `SetupCommonFetchMetadata()` (via `ConfigureRequest()` for
registration and refresh POSTs, or `ConfigureWellKnownRequest()` for
credentialless `.well-known` GETs), which sets the `Sec-Fetch-*` headers and
takes the isolation info, site for cookies and initiator from the triggering
request. Check redirects too: anything derived from the fetched URL (the
`Sec-Fetch-Site` value, same-origin or same-site checks, `.well-known`
derivation, scope) must be recomputed for the URL after redirects, unless the
fetch blocks redirects (as federated registration does through
`URLFetcher::SetShouldFollowRedirectCallback()`), and redirects to non-secure
URLs must be refused.

**Why.** Servers use the `Sec-Fetch-*` headers and the request's cookie and
isolation context to decide how to answer a DBSC request, so a fetch that skips
the shared setup changes what servers see and which checks apply. Values derived
from the original URL are stale after a redirect, and following a redirect to a
non-secure URL breaks the requirement that DBSC traffic stays on secure origins.

## Examples

Bad:

```cpp
// A new fetch path builds its own request and skips the shared setup.
well_known_fetcher_ = std::make_unique<URLFetcher>(...);
well_known_fetcher_->Start(base::BindOnce(...));
```

Good:

```cpp
// Registration and refresh POSTs go through ConfigureRequest(), and
// .well-known GETs go through ConfigureWellKnownRequest(); both call
// SetupCommonFetchMetadata().
url_fetcher_ = std::make_unique<URLFetcher>(...);
ConfigureWellKnownRequest(url_fetcher_->request());
url_fetcher_->Start(base::BindOnce(...));
```

# Device Bound Session Credentials

This directory implements the DBSC protocol, as described in
https://github.com/w3c/webappsec-dbsc/blob/main/README.md.

## High-level structure

On an outgoing HTTP request, the `URLRequestHttpJob` checks
`SessionService::ShouldDefer` to understand whether the request should
be deferred while we perform a DBSC refresh. Within
`//net/device_bound_sessions`, we wrap the request in `DbscRequest` to
ensure certain normalization for WebSockets is performed on the URLs
involved. `ShouldDefer` is implemented by `SessionServiceImpl`, which
looks at each `Session` object associated with the request's site and
checks:
- If the request goes to an in-scope endpoint
- If any of the session's bound cookies are expired, but would otherwise
  be included on the request (determined by `CookieCraving`). In this
  case, the request is deferred while a blocking refresh happens.
- Whether any of the session's bound cookies are about to expire. In
  this case, the request is allowed to continue, but we perform a
  refresh in parallel in an attempt to avoid blocking future requests.

Both proactive and blocking refreshes are handled by the
`RegistrationFetcher`, so named because it also handles new DBSC session
registration (covered later). This class is responsible for setting up
an HTTP request with the appropriate headers for either registration or
refresh. Most importantly, we sign the most recently-provided DBSC
challenge with the session's key and include that on the
`Secure-Session-Response` header.

When a refresh is completed, the `SessionServiceImpl` is
notified. Requests deferred for a blocking refresh are now allowed to be
resumed. The `SessionServiceImpl` will also notify any observers about
session activity with a `SessionAccess`. In Chrome, this is used to
track which site data is being used on a given tab. We also notify
observers with `SessionEvent` structs containing various `Display`
classes such as `SessionDisplay`. This is primarily used for DevTools.

After an HTTP request is completed, the `URLRequestHttpJob` again checks
in with the `SessionService` to handle any DBSC headers on the
response - in particular, the `Secure-Session-Registration` header,
which triggers new session registration, and the
`Secure-Session-Challenge` header, which sets the challenge for an
existing session.

Any usage of DBSC is noted on the `URLRequest` in
`device_bound_session_usage_`. This includes DevTools data and Use
Counters.

Session data is stored on disk through the `SessionStoreImpl`, using a
SQL database that maps site to the protobuf message `SiteSessions` in
`//net/device_bound_sessions/proto/storage.proto`. Note that the
`wrapped_key` field is not restored immediately to avoid significant
load on the TPM at startup. This requires additional complexity in the
`SessionServiceImpl` to restore keys on-demand before using them.

The DBSC spec leaves some decisions up to the user agent. In particular,
Chrome makes two decisions for the overall stability of the system:
- Sites have a limited number of TPM signing operations. The exact
  parameters here are not particularly important. Our goal is to stop a
  malicious site from overwhelming the TPM without blocking any
  legitimate use cases. Because we cache signed challenges, this only
  applies to refreshes with fresh challenges or new session
  registration. Signing operations are tracked by
  `SessionServiceImpl::refresh_times_`, and quota is checked by
  `SessionServiceImpl::RefreshQuotaExceeded`.
- Chrome attempts to proactively refresh a session if a request is made
  where a bound cookie would expire within 2 minutes. This is identical
  to a blocking refresh, but without deferring the triggering
  request. This aims to mitigate the latency spikes of DBSC on a site in
  near-continuous use. It will not mitigate the extra latency of DBSC on
  the first requests after Chrome starts up. Proactive refresh being
  triggered by a request to the site is an important privacy property
  because we don't want DBSC to be used as a mechanism for background
  tracking (e.g. knowing a user has Chrome open even after they've
  closed your tab). The implementation is parallel to deferring
  refreshes, starting with
  `SessionServiceImpl::MaybeStartProactiveRefresh`.

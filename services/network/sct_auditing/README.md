# SCT Auditing

`//services/network/sct_auditing` contains the core of Chrome's implementation
of Signed Certificate Timestamp (SCT) auditing. SCT auditing is an approach to
verify that server certificates are being properly logged via Certificate
Transparency (CT).

The current implementation is described in the
[Opt-in SCT Auditing design doc][1].

[1]: https://docs.google.com/document/d/1G1Jy8LJgSqJ-B673GnTYIG4b7XRw2ZLtvvSlrqFcl4A/edit

The SCT auditing code is composed of three main components:

* `SCTAuditingCache`: This is the main entrypoint for new SCT auditing
  reports. There is one cache instance owned by the NetworkService that is
  shared across all NetworkContexts.
* `SCTAuditingHandler`: The SCTAuditingHandler keeps track of pending reports
  for each NetworkContext. It is also responsible for persisting and restoring
  pending reports across restarts.
* `SCTAuditingReporter`: This class owns a single report and manages its
  lifetime (sending to the server, handling retries, etc.).

The `SCTAuditingDelegate` interface is defined in `//net` with a concrete
implementation (of the same name) in `NetworkContext`. This exposes an interface
for SCT auditing collection points in `//net` to enqueue reports with the SCT
auditing system in the network service by passing it along via the
NetworkContext. Collection points outside of `//net` (that are aware of the
network service) instead use `NetworkContext::MaybeEnqueueSCTReport()` directly.

SCT auditing is disabled by default, and is enabled and initialized by the
embedder calling `NetworkService::ConfigureSCTAuditing()` (which is exposed via
Mojo). Separately, each NetworkContext tracks whether it has SCT auditing
enabled, which can be configured by `NetworkContext::SetSCTAuditingEnabled()`
(which is also exposed via Mojo).

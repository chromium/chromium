# WebHID Browser Broker Security Notes

Supplemental security facts for `//content/browser/hid/*` (excluding test-only
code), supplementing
[services/device/SECURITY.md](/services/device/SECURITY.md) for general Web
Device APIs security, and Chromium's
[Security for Agents](/docs/security/security-for-agents.md). Before filing a
bug, review Security for Agents to double-check your findings.

## Threat Model & Problem Patterns

- **Compromised Renderer & Protected Report Filtering**: Caller origins and
  execution-context eligibility must come from browser-side frame or extension
  service-worker state, never IPC fields. Sensitive HID collections and reports
  (such as system keyboards, mice, and FIDO security keys) must be stripped via
  the HID blocklist before returning device info or opening connections.
- **Bind-Time Context Eligibility**: Enforce Permissions Policy, fenced-frame
  and guest-view restrictions, and non-opaque committed origins across the
  ancestor chain when binding the service rather than in individual IPC
  handlers.
- **Revocation & Synchronous Re-Entrancy**: Close active connections and fail
  pending requests when permission is revoked, complete state updates before
  invoking Mojo or observer callbacks, and guard modal chooser creation with a
  `base::WeakPtr` check before assigning member state in case fullscreen exit or
  window teardown synchronously destroys the service.

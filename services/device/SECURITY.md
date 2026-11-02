# Device Service & Web Device APIs Security Notes

Supplemental security facts for `//services/device/*` and Chrome's Web Device
APIs across `//device/*`, `//content/browser/*`, and `//chrome/browser/*`
(excluding test-only code), supplementing Chromium's
[Security for Agents](/docs/security/security-for-agents.md). Before filing a
bug, review Security for Agents to double-check your findings.

## Threat Model

- **Process & Sandbox**: `//services/device/*`, `//device/*`,
  `//content/browser/*`, and `//chrome/browser/*` run in the unsandboxed browser
  process. `//third_party/blink/renderer/modules/*` runs in the sandboxed
  Renderer process.
- **Compromised Renderer & Broker Layering**: Browser-process Mojo brokers in
  `//content/browser/*` and permission contexts in `//chrome/browser/*` defend
  against compromised Renderers; caller origins and execution-context state must
  be derived from browser-side frame or worker hosts, never from untrusted IPC
  message fields. `//services/device/*` endpoints receive requests brokered by
  the browser process and enforce hardware-level isolation (such as protected
  report/interface blocklists, interface-state invalidation across OS device
  resets, and sensor sampling/precision limits).
- **Untrusted Hardware Inputs & Rule of Two**: Hardware descriptors and packet
  payloads from external USB, HID, Serial, and Bluetooth peripherals are
  untrusted inputs into the unsandboxed browser process and must be validated
  with memory-safe Rust or bounds-checked `base::span` readers before exposure.

## Security & Memory-Safety Problem Patterns

- **Bind-Time Frame & Ancestor Origin Eligibility**: A compromised renderer can
  bypass Blink exposure checks and request Mojo interfaces directly. Browser
  brokers must enforce immutable context restrictions (secure/isolated context,
  Permissions Policy, fenced-frame and guest-view restrictions, and non-opaque
  committed origins across the full ancestor chain up to the outermost main
  frame) when binding the interface rather than scattering partial checks across
  individual IPC methods.
- **Stale Data & Dangling Callbacks on Permission Revocation or Downgrade**:
  Closing active connections without clearing cached readings, stopping active
  notification/advertisement streams, and draining queued asynchronous requests
  allows pre-revocation data to leak or queued requests to complete after access
  is revoked. Queued Mojo response callbacks for revoked devices must be
  extracted and invoked with an explicit error status rather than destroyed
  uninvoked.
- **Cross-Origin & Cross-Partition Permission Leakage**: In chooser permission
  contexts, synthesizing fake persistent identifiers for ephemeral devices or
  keying device lookup maps globally instead of per `(origin, device)` causes
  single-origin revocations to orphan shared device tokens or corrupt the
  profile-wide connected hardware inventory. Profile-wide connection cleanup
  must iterate all loaded `StoragePartition`s rather than only the default
  partition.
- **Re-Entrant Callback & Container Iteration Use-After-Free**: Invoking Mojo
  callbacks, observer notifications, or transfer completion callbacks while
  internal maps are partially updated or inside a `std::erase_if` predicate
  allows synchronous re-entry (such as frame detach or connection close) to
  mutate the container mid-iteration or destroy `this`, causing iterator
  invalidation or use-after-free. Complete all state mutations and move pending
  callbacks to stack locals before invoking any callback.
- **Synchronous Destruction During Modal Chooser/Prompt Creation**: Displaying a
  device chooser or permission prompt can synchronously exit fullscreen or
  close a Picture-in-Picture window, destroying the requesting frame and its
  document-scoped service before the creation call returns. Callers retaining
  member state must guard post-call assignments with a `base::WeakPtr` check.

## Explicit Non-bugs

- **Benign async teardown races**: Returning early or running an error callback
  instead of calling `mojo::ReportBadMessage()` when a frame navigates/detaches
  or a physical device disconnects asynchronously while an IPC is in flight.
- **Permitted physical peripheral application data**: Once a user grants access
  to a physical peripheral via the browser chooser and protected report/interface
  blocklists are enforced, anomalous application-layer reports from that
  peripheral to the permitted origin are not a browser privilege escalation
  unless they corrupt browser memory.
- **Disabled-by-default experimental features**: Issues reachable only when an
  off-by-default `base::Feature` or experimental `RuntimeEnabledFeature` is
  manually enabled.

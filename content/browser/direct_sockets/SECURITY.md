# Direct Sockets Security Notes

Canonical security notes for the Direct Sockets API (`TCPSocket`,
`TCPServerSocket`, `UDPSocket`, `MulticastController`), covering
`//third_party/blink/renderer/modules/direct_sockets/`,
`//content/browser/direct_sockets/` and `//chrome/browser/direct_sockets/`. Before filing a bug, review [Security for Agents](/docs/security/security-for-agents.md) and the non-bugs below.

## Threat Model

- Available to Isolated Web Apps (and other isolated contexts) with the
  `direct-sockets` permissions policy and the `DIRECT_SOCKETS` content setting.
  The embedder also allows Chrome Apps with a `sockets` manifest entry,
  allowlisted extensions, and ChromeOS `chrome-untrusted://terminal`.

## Security Boundaries

- IWA local ports: bound `UDPSocket` must use 0 or >= 1024. `TCPServerSocket`
  must use 0 or >= 32768.
- Outgoing traffic to local, loopback or multicast addresses requires the
  `LOCAL_NETWORK` / `LOOPBACK_NETWORK` permission. This covers `TCPSocket` and
  connected `UDPSocket` (checked after DNS resolution). Bound `UDPSocket`
  always needs both permissions.
- In frames, the `direct-sockets-multicast` permissions policy is required for
  multicast options, for joining groups, and for sending or connecting to a
  multicast address (`ERR_MULTICAST_NOT_ALLOWED`).

## Explicit Non-bugs

- **`TCPServerSocket` does not request local network permissions** (unlike
  `OpenBoundUDPSocket`), for any bind address, including `0.0.0.0` and
  loopback. `Accept()` does not filter peers. This is deliberate. Local Network
  Access protects against app-initiated traffic, such as scanning. It does not
  cover inbound connections.
- **Shared and service workers skip permissions policy checks.** This matches
  the rest of the web platform (crbug.com/393539884). Workers cannot prompt.
  They only use an existing permission grant for their own origin.
  (b/490642821)
- **`multicastAllowAddressSharing` with a non-multicast bind address.** Binding
  `0.0.0.0` with address sharing is how one socket joins several groups.
  (b/513176680)
- **ChromeOS firewall hole on all interfaces** for unspecified-address binds.
  ChromeOS security judged this low impact.
  (b/497984230)
- **Connection-Allowlist is not applied.** Direct Sockets does not take part in
  Connection-Allowlist enforcement. (b/552882464)
- **Embedder-allowed contexts** (Chrome Apps, allowlisted extensions, terminal)
  follow the `chrome.sockets` manifest model. They get no local network
  permission prompt and no multicast.

# Blob Storage Security Model

This document describes the security model for Chrome's Blob Storage system.

## UUID-based Access Control

The primary security mechanism for accessing blobs in Chrome is the secrecy of their **UUIDs** (Universally Unique Identifiers).

*   **Unguessable Secrets:** Blob UUIDs are generated using a cryptographically secure random number generator (UUID v4) and are considered unguessable.
*   **WAI (Working As Intended):** If a process or context possesses a Blob's UUID, it is assumed to have legitimate access to that Blob. The browser process does not enforce origin-based access controls on the lookup of Blobs by UUID in the global registry.
*   **No Confused Deputy:** Gaining access to a Blob via its UUID is WAI. Therefore, APIs that resolve Blob remotes by querying their UUID (e.g., `BlobStorageContext::GetBlobDataFromBlobRemote`) or look up Blobs by UUID (e.g., `BlobStorageContext::GetBlobDataFromUUID`) are not considered confused deputies, even if they are triggered by a compromised renderer.

## Security Boundaries

The security of the blob system relies on:
1.  **Preventing UUID Leaks:** UUIDs must not be leaked to unauthorized processes or origins.
2.  **Secure Transmission:** When Blobs are shared (e.g., via `postMessage` or stored in IndexedDB), they are serialized into structures like `SerializedBlob` which contain the UUID. These structures must only be sent to authorized recipients.

If the UUID is obtained via a legitimate channel (e.g., it was sent to the attacker origin), then accessing it is WAI.

## Storage Partitioning

Accessing same-origin data from a different storage partitioning (e.g. top-level
a.com accessing data stored by 3rd-party-subframe a.com) is a privacy problem,
not a security problem. If this access occurs purely via Javascript and HTML
then this is a serious privacy problem. If it requires a compromised renderer,
then this is not considered a security problem or a privacy problem and no bug
should be filed.

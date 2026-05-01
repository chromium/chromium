# Remoting Protocol Instructions

Instructions for working in `//remoting/protocol`.

## Architecture Overview

The `protocol` directory handles session authentication, establishment, and data
transfer (audio, video, input, and control channels).

### Transport

Chrome Remote Desktop uses WebRTC for all data transport (P2P connections).
*   `WebrtcConnectionToClient` / `WebrtcConnectionToHost`
*   `WebrtcTransport`

### Authentication

Remoting supports several authentication mechanisms, coordinated by the
`Authenticator` base interface and `NegotiatingAuthenticator`:
*   `SessionAuthzAuthenticator`: The primary authenticator used for
    enterprise/corp connections, leveraging backend SessionAuthz services.
*   `Spake2Authenticator`: A standard SPAKE2-based authenticator using a shared
    secret (e.g., PIN).
*   `PairingAuthenticator`: Uses a PIN and a persistent pairing registry to
    avoid requiring PIN entry on every connection.

### Session Management & Signaling

Session establishment uses Jingle semantics (via XMPP or FTL).
*   `Session`: Represents a single connection session.
*   `SessionManager`: Manages incoming and outgoing sessions.
*   `JingleSession` / `JingleSessionManager`: The active implementations for
    session signaling over the FTL signaling channel.

## Key Files to Read

*   `remoting/protocol/authenticator.h`: Interface for authentication.
*   `remoting/protocol/session.h`: Interface for a session.
*   `remoting/protocol/webrtc_connection_to_client.h`: The main entry point for
    host-side WebRTC connection logic.
*   `remoting/protocol/webrtc_transport.h`: WebRTC transport implementation.

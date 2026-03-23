# Remoting Protocol Instructions

Instructions for working in `//remoting/protocol`.

## Architecture Overview

Remoting protocol handles session authentication, establishment, and data
transfer (audio, video, input, and control channels).

### Authentication

Remoting supports various authentication mechanisms.
- `Authenticator`: The base interface for host/client authentication.
- `NegotiatingAuthenticator`: Negotiates the best authentication method.
- `PairingAuthenticator`: Uses a PIN and a pairing registry.
- `Spake2Authenticator`: A modern SPAKE-based authenticator.

### Session Management

- `Session`: Represents a single connection session.
- `SessionManager`: Manages incoming and outgoing sessions.
- `JingleSession`: Implementation of `Session` using Jingle signaling.

### Transport and Channels

Remoting uses WebRTC for data transfer.
- `Transport`: Negotiates and maintains the data transport.
- `Channel`: Individual streams (audio, video, control, input, data).
- `WebRtcTransport`: The standard WebRTC-based transport.

## Key Files to Read

- `remoting/protocol/authenticator.h`: Interface for authentication.
- `remoting/protocol/session.h`: Interface for a session.
- `remoting/protocol/transport.h`: Interface for data transport.
- `remoting/protocol/webrtc_transport.h`: WebRTC implementation.


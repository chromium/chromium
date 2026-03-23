# Remoting Signaling Instructions

Instructions for working in `//remoting/signaling`.

## Architecture Overview

Remoting signaling handles the initial connection setup (handshake) between a
client and a host. It uses Jingle (XMPP-based) for session negotiation.

### SignalStrategy

The `SignalStrategy` interface is the core of signaling. It abstracts the
underlying transport (XMPP, FTL).
- `FtlSignalStrategy`: The modern signaling strategy used in production.
- `FakeSignalStrategy`: Used in tests to simulate signaling.

### FTL (Faster Than Light)

FTL is the proprietary signaling protocol used by Chrome Remote Desktop.
- `FtlMessagingClient`: Handles sending/receiving FTL messages.
- `FtlRegistrationManager`: Handles registering the host/client with the FTL
  backend.

### Jingle and Session Negotiation

Even when using FTL as a transport, the actual session negotiation (ICE,
SDP, authentication) is still based on Jingle logic.
- `JingleMessage`: The data structure for Jingle messages.
- `SessionConfig`: Defines the negotiated session parameters (codecs,
  transports).

## Key Files to Read

- `remoting/signaling/signal_strategy.h`: The base interface.
- `remoting/signaling/ftl_signal_strategy.h`: FTL implementation.
- `remoting/signaling/jingle_message.h`: Jingle message structure.
- `remoting/signaling/session_config.h`: Session negotiation parameters.


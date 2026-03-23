# Remoting Signaling Instructions

Instructions for working in `//remoting/signaling`.

## Architecture Overview

Remoting signaling handles the initial connection setup (handshake) between a
client and a host. It uses Jingle (XMPP-based) concepts for session negotiation,
though the actual transport mechanisms have evolved.

### SignalStrategy & Transports

The `SignalStrategy` interface is the core of signaling. We currently support
two primary transport backends:

1.  **FTL (Faster Than Light)**
    *   The standard proprietary signaling protocol.
    *   Uses `FtlMessagingClient`, `FtlRegistrationManager`, and
        `FtlSignalStrategy`.

2.  **Corp Signaling**
    *   A newer messaging service used for enterprise/internal connections.
    *   Uses `CorpMessagingClient`, `CorpMessageChannelStrategy`, and
        `CorpSignalStrategy`.
    *   **IMPORTANT (Internal Protos):** Corp signaling relies on protobufs from
        `//remoting/internal/` which are **not publicly accessible** (requires
        `src-internal` access to build).
    *   To bridge the gap between public and internal builds, the codebase uses
        `//remoting/base/internal_headers.h`. This header determines whether to
        use the real internal implementation or a stubbed-out public
        implementation. **Always** use the wrapper structs defined in this
        header (e.g., `internal::PeerMessageStruct`, `internal::IqStanzaStruct`)
        rather than trying to directly include the internal protos.

### Jingle, XML, and Struct Conversions

Historically, CRD signaling was entirely XMPP/XML-based.

*   Currently, the signal strategies might receive serialized XML stanzas or
    structured protobufs. They immediately convert these into a
    strategy-agnostic C++ struct (`JingleMessage` or `JingleMessageReply`).
*   The rest of the Jingle-related classes (in `//remoting/protocol`) operate
    entirely on these structs.
*   **Migration in progress:** The codebase is moving away from raw XML to
    structured payloads. When adding new signaling fields, ensure they are added
    to the C++ structs (`jingle_data_structures.h`) and their respective struct
    converters (`jingle_message_struct_converter.cc`), not just the legacy XML
    converters.

### Message Channel

*   `MessageChannel` handles the lifetime, reconnect logic, and exponential
    backoff of the server-side stream.
*   Transport-specific logic is injected via the `MessageChannelStrategy`
    interface (`FtlMessageChannelStrategy` or `CorpMessageChannelStrategy`).

### SignalingAddress

*   Represents an endpoint and its routing channel (`FTL`, `CORP`, or `XMPP`).

## Key Files to Read

*   `remoting/signaling/signal_strategy.h`: The base signaling interface.
*   `remoting/signaling/corp_signal_strategy.h` / `ftl_signal_strategy.h`:
    The transport implementations.
*   `remoting/signaling/jingle_data_structures.h`: The C++ structs
    representing Jingle messages.
*   `remoting/signaling/jingle_message_struct_converter.h`: Conversions
    between structured payloads and Jingle messages.
*   `remoting/signaling/message_channel.h`: Stream lifecycle and reconnect
    logic.

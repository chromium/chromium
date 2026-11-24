# Chrome Remote Desktop (CRD) Signaling Compatibility & Test Scenarios

## 1. Overview & Architectural Principle

### 1.1 Scope
This document defines the exhaustive compatibility specification, wire format
mappings, golden test vectors, and state transition test cases for CRD
signaling across three production implementations:
1. **Chromium Native C++** (`remoting/signaling/`, `remoting/protocol/`)
2. **Web Client** (TypeScript)
3. **Android Client & Host** (Kotlin/Java/C++ implementations)

### 1.2 Core Principle: Chromium C++ as the Canonical Ground Truth
**The Chromium C++ implementation is the reference source of truth.**
- **Rationale**: Chromium C++ is the most distributed, native binary-packaged
  codebase with strict release schedules, extensive platform dependencies
  (Windows, macOS, Linux, ChromeOS), and high regression risk. Modifying
  Chromium's signaling behavior carries significant risk and long rollout
  cycles.
- **Direction of Alignment**: Test data, Web Client (TypeScript), and Android
  (Kotlin/Java) implementations must conform directly to Chromium's existing
  wire format, serialization behaviors, field encodings, and state
  machines--**not** the reverse.
- **Golden Vector Generation**: All golden test vectors in this specification
  are directly validated against Chromium's `JingleMessageToXml`,
  `JingleMessageFromXml`, `JingleMessageToProto`, and `JingleMessageFromProto`
  converter functions.

---

## 2. Compatibility Test Suite Assessment & Gap Analysis

### 2.1 Existing Chromium Test Coverage

- **`jingle_message_proto_converter_unittest.cc`**:
  - *Scope*: `JingleMessage` <-> `ftl::IqStanza`
  - *Gaps & Risks*: Exercises Protobuf roundtrips across Scenarios 1-8
    (multi-m-line bundle SDPs, IPv4/IPv6/TCP/srflx/relay ICE candidates,
    SPAKE2/SessionAuthz tokens, `PairingFailed` empty-error `"unknown"`
    normalization, and comma-joined `host_attributes` splitting;
    https://crrev.com/c/8829951), but tests Protobuf in isolation from XML
    (`XML` <-> `JingleMessage` <-> `Proto` cross-format parity is not yet
    validated in a unified suite).
- **`jingle_message_xml_converter_unittest.cc`**:
  - *Scope*: `JingleMessage` <-> XML `XmlElement`
  - *Gaps & Risks*: Tests XML in isolation from Protobuf (including empty
    `<pairing-failed>` `"unknown"` normalization added in
    https://crrev.com/c/8829951). Does not test cross-format translation
    fidelity (`XML` -> `Model` -> `Proto` or `Proto` -> `Model` -> `XML`).
- **`jingle_message_struct_converter_unittest.cc`**:
  - *Scope*: `JingleMessage` <-> `internal::IqStanzaStruct`
  - *Gaps & Risks*: Tests internal C++ memory representations; does not
    validate wire serialization parity against TypeScript or Kotlin parsers.
- **`ftl_signal_strategy_unittest.cc`**:
  - *Scope*: Format negotiation (`BOTH`, `PROTOBUF`, `XML`) and peer cache
    isolation (https://crrev.com/c/8834622)
  - *Gaps & Risks*: Uses mocked callbacks; does not test full real-world
    multi-round handshakes or concurrent multi-platform traffic.
- **`jingle_session_unittest.cc`**:
  - *Scope*: `JingleSession` state machine
  - *Gaps & Risks*: Critical Gap. Uses `FakeSignalStrategy` with in-memory
    structs and `FakeAuthenticator`; completely bypasses wire serialization
    and Protobuf encoding.

### 2.2 Exhaustive Gap Breakdown

1. **Cross-Format Parity Across Realistic Payloads**:
   While `jingle_message_proto_converter_unittest.cc` exercises complex
   Protobuf payloads in isolation, unified cross-format (`XML` <->
   `JingleMessage` <-> `Proto`) equivalence is not yet validated across:
   - **SDP Descriptions**: Bundle groups (`a=group:BUNDLE 0 1 2`), multiple
     media m-lines (`audio`, `video`, `application`), DTLS certificate
     fingerprints (`a=fingerprint:sha-256 ...`), ICE credentials (`a=ice-ufrag`,
     `a=ice-pwd`), and RTP header extensions.
   - **ICE Candidates**: Multiple transport protocols (UDP, TCP), candidate
     types (`host`, `srflx`, `relay`), and IPv4 / IPv6 addresses with priority
     levels and generation counters.
   - **Authentication Data**: Multi-round SPAKE2 Curve25519 public points (32
     bytes), SHA256 verification hashes (32 bytes), and SessionAuthz bearer
     tokens (Base64 strings in serialized XML vs decoded raw bytes in Protobuf).
   - **Attachment Configurations**: Host attributes (`Debug-Build`, `HWEncoder`,
     `SupportsIceDatagramTransport`, `CapturesAudio`, `SupportsDirectPeer`) and
     configuration key-value pairs (`Av1-Encoder-Speed: 11`,
     `VideoCodecPreset: high_quality`, `EnableMultimon: true`).
2. **Binary vs Base64 Encoding Mismatches**:
   - XML serializes binary data as Base64 strings. Protobuf defines these
     fields as `bytes`.
   - Differences in Base64 padding, URL-safe vs standard Base64 encoding, or
     token string wrappers must be verified across C++, TypeScript, and Kotlin
     against Chromium's parser rules.
3. **Missing Cross-Format Equivalence Tests**:
   - No automated tests verify that an incoming XML stanza from a legacy client
     produces an identical `JingleMessage` model and downstream Protobuf
     representation as a modern client would generate natively.
4. **State Machine Wire Replay**:
   - Session state transitions (`INITIALIZING` -> `CONNECTING` ->
     `AUTHENTICATED` -> `ACCEPTED` -> `CONNECTED` -> `CLOSED`) have never been
     exercised against serialized Protobuf wire messages in Chromium unit
     tests.

---

## 3. Wire Format Specification & Mapping Matrix

### 3.1 Envelope & Addressing

- **Stanza ID**:
  - XML: `<iq id="...">`
  - Proto: `optional string id = 1;`
  - Model: `message.message_id`
  - **1-Based Sequencing Rule (`<prefix>_<seq>`, Requests Only)**:
    - Sequencing applies **exclusively to Jingle request stanzas** (`type="set"`
      / `jingle` payload). IQ response stanzas (`type="result"` / `reply` and
      `type="error"` / `error`) MUST echo the exact `id` of the request they
      acknowledge and bypass `OrderedMessageQueue`.
    - Outgoing Jingle request stanzas MUST format `id` as `<prefix>_<seq>`,
      where `<prefix>` is a random per-session alphanumeric string containing
      **no underscores** (`_`) and `<seq>` is a monotonically increasing
      base-10 integer starting at **`1`** (`1, 2, 3, ...`, matching Chromium's
      `++next_outgoing_id_` and avoiding JavaScript `0` falsiness bugs).
    - **Ingress `GetSequentialId()` Parsing (`jingle_session.cc`)**:
      Implementations split `id` on `_`, trim ASCII whitespace from each
      segment, and **drop empty segments** (`base::SPLIT_WANT_NONEMPTY`). Any
      request `id` that does not split into **exactly two non-empty `_`-separated
      tokens**, or whose second token is not a valid base-10 integer, is
      unsequenced and delivered immediately without queueing. (Note: because
      empty tokens are dropped, an `id` like `a__5` yields `["a", "5"]` and
      **is** sequenced--TypeScript/Kotlin parsers MUST filter out empty tokens
      rather than using a raw `id.split("_")`, whereas 3-token fixture IDs such
      as `msg_initiate_001` are unsequenced.)
    - **Initial Expected Sequence Number (`next_incoming_`)**:
      - *Responder (Host)*: `InitializeIncomingConnection` calls
        `SetInitialId(initiate.message_id)`. If `session-initiate` carries
        sequential ID `<seq>` (normally `1`), `next_incoming_` is initialized to
        `<seq> + 1` (expecting `2` next). If `session-initiate` is unsequenced,
        `next_incoming_` remains unset (`kAny`) until the first sequenced
        incoming request arrives, at which point it latches onto that request's
        `<seq>`.
      - *Initiator (Client)*: `next_incoming_` starts unset (`kAny`), latches
        onto the `<seq>` of the responder's first sequenced request (normally
        `1` on `session-accept`), and expects `<seq> + 1` thereafter.
- **From Address**:
  - XML: `<iq from="user@example.com/chromoting_ftl_uuid">`
  - Proto: `optional JabberId sender = 2;`
  - Model: `message.from` (`SignalingAddress`)
- **To Address**:
  - XML: `<iq to="host@example.com/chromoting_ftl_uuid">`
  - Proto: `optional JabberId receiver = 3;`
  - Model: `message.to` (`SignalingAddress`)
- **Session ID**:
  - XML: `<jingle sid="...">`
  - Proto: `jingle.session_id = 1;`
  - Model: `message.sid`
- **Action**:
  - XML: `<jingle action="...">`
  - Proto: `jingle.action` (`oneof`)
  - Model: `message.action()`

> **Addressing Note**: When Chromium C++ converts a `SignalingAddress` to
> `ftl::JabberId`, `local_part` and `domain_part` are split from the username
> (`user@example.com`), and `resource_part` contains the raw registration UUID
> (without the `chromoting_ftl_` prefix). When parsing incoming
> `ftl::JabberId`, Chromium strips `chromoting_ftl_` if present or accepts the
> raw UUID. Signaling adapters (converters and signal strategy layers) serve
> as the normalization boundary on both ingress and egress: higher-level
> application components (e.g. host directory listings, OAuth endpoints, legacy
> UI) may provide JIDs containing the `chromoting_ftl_` prefix, but adapters
> must normalize them to raw UUIDs on egress into Protobuf
> `JabberId.resource_part` and normalize on ingress when constructing
> `SignalingAddress`.

### 3.2 Detailed Field & Type Mapping

- **Initiator JID**:
  - XML: `<jingle initiator="...">`
  - Proto: `jingle.session_initiate.initiator` (`JabberId`)
- **Auth Method**:
  - XML: `<authentication method="...">`
  - Proto: `authentication.method` (`enum AuthenticationMethod`)
- **Supported Methods**:
  - XML: `<authentication supported-methods="...">` (comma-separated)
  - Proto: `repeated AuthenticationMethod supported_methods`
- **SPAKE Message**:
  - XML: `<spake-message>` (Base64 string)
  - Proto: `authentication.spake_message` (raw `bytes`)
- **Verification Hash**:
  - XML: `<verification-hash>` (Base64 string)
  - Proto: `authentication.verification_hash` (raw `bytes`)
- **Certificate**:
  - XML: `<certificate>` (Base64 DER cert string)
  - Proto: `authentication.certificate` (raw DER `bytes`)
- **Host Token**:
  - XML: `<host-token>` (Base64 string returned by
    `SessionAuthzService.GenerateHostToken`, written verbatim to XML without
    additional Base64 encoding)
  - Proto: `authentication.session_authz_host_token` (Base64-decoded raw
    `bytes`; converters Base64-decode when writing to Protobuf `bytes` and
    Base64-encode back to the token string when reading from Protobuf `bytes`)
- **Session Token**:
  - XML: `<session-token>` (Base64 string returned by
    `SessionAuthzService.AuthorizeSession`, written verbatim to XML without
    additional Base64 encoding)
  - Proto: `authentication.session_authz_session_token` (Base64-decoded raw
    `bytes`; converters Base64-decode when writing to Protobuf `bytes` and
    Base64-encode back to the token string when reading from Protobuf `bytes`)
- **Pairing Info Client ID**:
  - XML: `<pairing-info client-id="...">`
  - Proto: `authentication.pairing_info.client_id` (`string`)
- **Pairing Error** (wire support and `"unknown"` normalization added in
  https://crrev.com/c/8829951):
  - XML: `<pairing-failed error="...">` nested inside
    `<authentication xmlns="google:remoting">`. Egress XML serializers MUST
    always nest `<pairing-failed>` inside `<authentication>` with a non-empty
    `error` attribute; direct `<jingle><pairing-failed/></jingle>` children are
    invalid (Chromium `SessionInfoFromXml` and Web `JingleMessage.fromXml`
    reject them in `session-info` with `UNSUPPORTED_INFO` and ignore them in
    `session-accept`). Ingress XML parsers MUST treat `<pairing-failed>`
    presence inside `<authentication>` as a pairing failure and normalize a
    missing or empty `error` attribute to the exact string **`"unknown"`**.
  - Proto: `authentication.pairing_failed.error` (`string`, `PairingFailed` at
    field `9` in `xmpp.proto`). Senders MUST populate a non-empty `error`
    string. Ingress Protobuf parsers MUST treat `has_pairing_failed()` presence
    as a pairing failure and normalize an unset or empty `error` field to the
    exact string **`"unknown"`**.
  - Model: `authentication.pairing_error` (`string`; treated as an opaque
    diagnostic reason and never switched on programmatically)
- **SDP Offer / Answer Encapsulation**:
  - XML: `<transport xmlns="google:remoting:webrtc"><session-description
    type="offer|answer">` embedded within `<jingle action="transport-info">`.
    Note that XML `session-initiate` and `session-accept` stanzas carry no
    `<session-description>`, but MUST still include an empty
    `<transport xmlns="google:remoting:webrtc" />` child inside `<content>`
    (required by Chromium's `ParseInitiateOrAccept`).
  - Proto: Dedicated `transport_info.session_description.sdp` + `type` enum
    sent in a `transport_info` Jingle action
- **SDP Signature**:
  - XML: `<session-description signature="...">` (Base64 HMAC-SHA256)
  - Proto: `transport_info.session_description.signature` (32-byte raw `bytes`)
- **ICE Candidate Line**:
  - XML: `<candidate>candidate:...</candidate>`
  - Proto: `transport_info.candidates[i].candidate` (`string`, starting with
    `candidate:` without an SDP `a=` prefix)
- **ICE Candidate Mid**:
  - XML: `<candidate sdpMid="...">`
  - Proto: `transport_info.candidates[i].sdp_mid` (`string`, mandatory;
    Chromium's `JingleTransportInfoFromProto` skips candidates missing
    `sdp_mid` or `sdp_m_line_index`)
- **ICE Candidate Index**:
  - XML: `<candidate sdpMLineIndex="...">`
  - Proto: `transport_info.candidates[i].sdp_m_line_index` (`int32`, mandatory)
- **Host Attributes**:
  - XML: `<attachments><host-attributes>attr1,attr2,...</host-attributes>`
    (tokens are whitespace-trimmed and MUST NOT contain `,`. Egress XML
    serializers MUST emit this body-text form because deployed Chromium hosts
    prior to M160 only parse body text and drop `<attribute>` child elements;
    ingress parsers should also accept `<attribute>` child elements, which take
    precedence over body text when both are present)
  - Proto: `repeated Attachment.host_attributes.attribute` (string; egress
    emits one discrete attribute token per element, never comma-joined; ingress
    parsers split elements on `,` and trim whitespace to normalize M156-M159
    hosts, added in https://crrev.com/c/8829951)
- **Host Config Map**:
  - XML: `<attachments><host-configuration>key1:val1,key2:val2</host-configuration>`
    (keys and values are whitespace-trimmed and MUST NOT contain `,` or `:`.
    Egress XML serializers MUST emit this body-text form because deployed
    Chromium hosts prior to M160 only parse body text and drop XML-attribute
    `<host-configuration>`; ingress parsers should also accept non-`xmlns` XML
    attributes on `<host-configuration>`, which take precedence over body text
    when both are present)
  - Proto: `repeated Attachment.host_config.settings` (map<string, string>)
- **Terminate Reason**:
  - XML: `<reason><[reason-tag]/></reason>`
  - Proto: `session_terminate.reason` (`enum Reason`)
- **Terminate Error Code**:
  - XML: `<error-code xmlns="google:remoting">`
  - Proto: `session_terminate.error_code` (`ErrorCodeToString(code)`)
- **Terminate Details**:
  - XML: `<error-details xmlns="google:remoting">`
  - Proto: `session_terminate.error_details` (string)
- **Terminate Location**:
  - XML: `<error-location xmlns="google:remoting">`
  - Proto: `session_terminate.error_location` (`file.cc:line`)
- **IQ Result Reply**:
  - XML: `<iq type='result'><jingle xmlns='urn:xmpp:jingle:1'/></iq>`
  - Proto: `ftl.IqStanza.reply = JingleReply {}` (Note: converting to XML
    requires emitting the empty `<jingle xmlns="urn:xmpp:jingle:1" />` tag)
- **IQ Error Reply**:
  - XML: `<iq type='error'><error type='...'>...</error></iq>`
  - Proto: `ftl.IqStanza.error = ErrorStanza` (`Condition` enum + `text`)

> **`SessionTerminate` `ErrorCode` & `Reason` Egress / Ingress Mappings**:
> Because multiple `ErrorCode` values collapse onto `DECLINE` or
> `GENERAL_ERROR`, egress and ingress mappings are defined separately per
> `jingle_session.cc`:
>
> **1. Egress (`JingleSession::Close(error)` -> `(reason, error_code)`)**:
> Implementations MUST populate `session_terminate.reason` on every close, and
> MUST populate `error_code` (`ErrorCodeToString(error)`) for every code except
> `UNKNOWN_ERROR`:
> - `OK` -> `SUCCESS` (`<success/>`) + `error_code: "OK"`
> - `SESSION_REJECTED`, `AUTHENTICATION_FAILED`, `INVALID_ACCOUNT` -> `DECLINE`
>   (`<decline/>`) + `error_code: ErrorCodeToString(error)`
> - `INCOMPATIBLE_PROTOCOL` -> `INCOMPATIBLE_PARAMETERS`
>   (`<incompatible-parameters/>`) + `error_code: "INCOMPATIBLE_PROTOCOL"`
> - `HOST_OVERLOAD` -> `CANCEL` (`<cancel/>`) + `error_code: "HOST_OVERLOAD"`
> - `MAX_SESSION_LENGTH` -> `EXPIRED` (`<expired/>`) +
>   `error_code: "MAX_SESSION_LENGTH"`
> - `HOST_CONFIGURATION_ERROR` -> `FAILED_APPLICATION`
>   (`<failed-application/>`) + `error_code: "HOST_CONFIGURATION_ERROR"`
> - `UNKNOWN_ERROR` -> `GENERAL_ERROR` (`<general-error/>`); **`error_code`
>   omitted**
> - All other `ErrorCode` values (`PEER_IS_OFFLINE`,
>   `CHANNEL_CONNECTION_ERROR`, `SIGNALING_ERROR`, `SIGNALING_TIMEOUT`, etc.) ->
>   `GENERAL_ERROR` (`<general-error/>`) + `error_code: ErrorCodeToString(error)`
>
> **2. Ingress (`JingleSession::OnTerminate` -> `ErrorCode`)**:
> - *When `error_code` is present and recognized (not `UNKNOWN_ERROR`)*:
>   - `error_code == "SESSION_REJECTED"` -> rewritten to `AUTHENTICATION_FAILED`
>     (legacy WebApp compatibility, https://crbug.com/618036)
>   - Any other recognized `error_code` -> mapped directly to that `ErrorCode`
>     (takes precedence over `reason`)
> - *When `error_code` is omitted or unrecognized (`UNKNOWN_ERROR`)*, derive
>   `ErrorCode` from `reason`:
>   - `SUCCESS` -> `SESSION_REJECTED` if `state == CONNECTING`, else `OK`
>   - `DECLINE` -> `AUTHENTICATION_FAILED`
>   - `CANCEL` -> `HOST_OVERLOAD`
>   - `EXPIRED` -> `MAX_SESSION_LENGTH`
>   - `INCOMPATIBLE_PARAMETERS` -> `INCOMPATIBLE_PROTOCOL`
>   - `FAILED_APPLICATION` -> `HOST_CONFIGURATION_ERROR`
>   - `GENERAL_ERROR` -> `CHANNEL_CONNECTION_ERROR`
>   - Unspecified or unrecognized `reason` -> `UNKNOWN_ERROR`

> **Legacy Error Condition Mapping & Namespace Note**: Chromium C++
> historically maps `JingleMessageReply` error types to legacy XMPP error
> elements with inverted condition tags compared to strict RFC 6120:
> - `ErrorStanza.Condition.NOT_IMPLEMENTED` ⟷ `<feature-bad-request/>`
> - `ErrorStanza.Condition.UNSUPPORTED_INFO` ⟷ `<feature-not-implemented/>`
> - `ErrorStanza.Condition.INVALID_SID` ⟷ `<item-not-found/>` (`"Invalid SID"`)
> - `ErrorStanza.Condition.BAD_REQUEST` ⟷ `<bad-request/>`
> - `ErrorStanza.Condition.UNEXPECTED_REQUEST` ⟷ `<unexpected-request/>`
> - `ErrorStanza.Condition.CONDITION_UNSPECIFIED` ⟷ `<unspecified-error/>`
>
> **Egress XML serializers MUST emit `<error>`, the condition child element,
> and optional `<text>` in the `jabber:client` namespace** (never
> `google:remoting` or `urn:ietf:params:xml:ns:xmpp-stanzas`). While RFC 6120
> §8.3.2 specifies `urn:ietf:params:xml:ns:xmpp-stanzas`, deployed Chromium C++
> parsers (`JingleMessageReplyFromXml`) look up `QName("jabber:client", ...)`
> exclusively and collapse any other namespace to `UNSPECIFIED` with empty
> text. Ingress XML parsers in Web and Android should match condition child
> tags namespace-agnostically or accept both namespaces. All 6 `Condition`
> values (including `BAD_REQUEST`) must be mapped without altering or inverting
> these tags.

### 3.3 Signaling Format Negotiation & Dual-Payload Rules

Chrome Remote Desktop supports both legacy XML stanzas and structured Protobuf
(`ftl.IqStanza`) payloads over FTL signaling. To maintain seamless
bidirectional compatibility between native Chromium hosts/clients, the Web
Client, and Android implementations:

1. **Client Initiation Version Thresholds**:
   Clients determine whether to send a **Dual-Payload** (`BOTH`) or
   **XML-only** `session-initiate` from the target host's `HostInfo.host_version`
   major milestone (`remoting/proto/remoting/v1/host_info.proto`) according to
   the following normative table:

   | Client Implementation | Target Host Class & Classification Signal | `BOTH` `session-initiate` Requirement | `XML`-only `session-initiate` Requirement |
   | :--- | :--- | :--- | :--- |
   | **Chromium `RemotingClient`** (`remoting/client/common/remoting_client.cc` -- ChromeOS Class Tools enterprise IT2Me client; 12-digit access code / `SPAKE2_CURVE25519`, never Me2Me pairing) | Managed ChromeOS enterprise IT2Me hosts returned by `DirectoryServiceClient::GetManagedChromeOsHost` (`GetManagedChromeOsHostResponse.chrome_os_host`, where `host_os_name == "ChromeOS"`, including managed Android-based ChromeOS devices for which Directory synthesizes `host_os_name = "ChromeOS"` and `host_version = "156.0.0.0"`) | **MUST** send `BOTH` when `host_version` is valid and **`>= 156`** | **MUST** send `XML`-only when `host_version < 156` or is unset/invalid |
   | **Web Client & Android Client** (Me2Me and general IT2Me clients) | All hosts returned by `GetHostList`, `GetHost`, or `GetSupportHost` (`apis.v1.HostInfo`, regardless of `host_os_name`) | **MUST** send `BOTH` when `host_version` is valid and **`>= 160`** (M160 adds `PairingFailed` wire support in https://crrev.com/c/8829951 and `FtlSignalStrategy` negotiation fixes in https://crrev.com/c/8834622; see https://crbug.com/359620500) | **MUST** send `XML`-only when `host_version < 160` or is unset/invalid (including unversioned / Android-to-Android `GetSupportHost` peers) |

2. **Dual-Payload Mode (`BOTH`) Scope & Negotiation**:
   - **Strict Scope & Pre-Reply Stanzas**: Dual-Payload (`BOTH`: setting both
     `ftl.ChromotingXmppMessage.stanza` and
     `ftl.ChromotingXmppMessage.iq_stanza`) MUST NOT be sent on any message
     other than `session-initiate`. Any non-initiate message sent by the
     initiator while `session-initiate` is still pending (before the
     responder's `JingleReply` ACK or `session-accept` arrives, e.g., an
     immediate `session-terminate` cancellation) MUST be sent as single-payload
     **`XML`-only** (https://crrev.com/c/8834622; shipped M156-M159
     `RemotingClient` binaries sent pre-reply non-initiate stanzas as
     `PROTOBUF`-only).
   - **Responder Locking (`BOTH` `session-initiate` -> `PROTOBUF`)**: When a
     Protobuf-capable responder (host) receives a `BOTH` `session-initiate`, it
     locks the session's **outbound** format for `(peer, sid)` into `PROTOBUF`
     mode and sends its `JingleReply` ACK and `session-accept` as
     `PROTOBUF`-only (`iq_stanza` present, `stanza` omitted).
   - **Initiator Locking (`PROTOBUF` vs `XML`/`BOTH` Response)**: When the
     initiator receives the responder's first response to `session-initiate`
     (either the `JingleReply` ACK or `session-accept`, whichever arrives
     first; https://crrev.com/c/8834622), it locks the session's **outbound**
     format to `PROTOBUF` if and only if that response is `PROTOBUF`-only. If
     the response is `XML`-only **or** `BOTH`, the initiator MUST lock the
     session's outbound format to `XML`-only (updated in
     https://crrev.com/c/8834622; earlier Chromium builds treated a `BOTH`
     reply as `PROTOBUF`).

3. **Locked Outbound Session Format (`PROTOBUF` vs `XML`) & Ingress Acceptance**:
   - Once a session's outbound format is locked for `(peer, sid)` during
     initiation (`PROTOBUF` per Rule 2, or `XML` when `session-initiate` or its
     response is `XML`-only / `BOTH`), all subsequent outbound requests on that
     session MUST use that single locked format until `session-terminate`
     erases the session state.
   - **Ingress Format Tolerance & Per-Request Reply Matching**: Session format
     locking governs **outbound request generation**, not inbound parsing.
     Endpoints MUST continue parsing both `PROTOBUF` (`iq_stanza`) and
     `XML`-only (`stanza`) incoming stanzas at any point in the session
     lifecycle without flipping an already-locked outbound session format
     (ensuring a `PROTOBUF`-locked responder cleanly processes both an early
     `XML`-only `session-terminate` per Rule 2 and a `PROTOBUF`-only
     `session-terminate` from M156-M159 `RemotingClient` peers). Each incoming
     request MUST be acknowledged (`JingleMessageReply`) in the format of that
     specific request (`PROTOBUF` for `BOTH` `session-initiate` and `PROTOBUF`
     requests; `XML` for `XML` requests and any unexpected non-initiate `BOTH`
     request).

4. **XML-Only Initiation (`XML`)**:
   - When `session-initiate` contains only an XML stanza (`stanza` present
     without `iq_stanza`), the responder locks the session's outbound format to
     `XML`-only, and both peers communicate using `XML`-only for the lifetime
     of that session.

5. **Address Resolution & JID Routing**:
   - All outgoing XML stanzas must explicitly set the `to` attribute with the
     recipient's full JID including the `/chromoting_ftl_<registration_id>`
     resource part.
   - Incoming Protobuf `sender` and `receiver` `JabberId` fields must be mapped
     to valid `SignalingAddress` instances, stripping the `chromoting_ftl_`
     prefix when constructing internal registration identifiers.

### 3.4 Protobuf Wire Design Invariants & Canonical Rules

The Protobuf signaling reference design enforces the following architectural
invariants across all implementations:

1. **Native Binary Payloads (Raw `bytes` vs Base64 Strings)**:
   - All cryptographic materials (`spake_message`, `verification_hash`,
     `certificate`, `signature`) and SessionAuthz tokens
     (`session_authz_host_token`, `session_authz_session_token`) must be
     transmitted as raw `bytes` in Protobuf payloads.
   - For `SessionAuthz`, `SessionAuthzService` issues tokens as Base64-encoded
     strings: serialized XML (`<host-token>` and `<session-token>`) preserves
     those Base64 strings unchanged, whereas Protobuf emitters MUST
     Base64-decode them into raw `bytes` on egress and Base64-encode raw
     `bytes` back to the token string on ingress.
   - Base64 string wrapping is never applied inside Protobuf `bytes` fields.
     (Chromium's `JingleMessageProtoConverter` currently retains a transitional
     Base64-detection fallback for legacy prototype inputs; Web and Android
     implementations should unconditionally decode/encode Base64 at the
     Protobuf `bytes` boundary.)

2. **Normalized Addressing (`resource_part` Raw UUID & Boundary)**:
   - `JabberId.resource_part` must contain strictly the raw FTL registration
     UUID (e.g., `11111111-1111-1111-1111-111111111111`) with no
     `chromoting_ftl_` prefix.
   - `JabberId.local_part` and `JabberId.domain_part` must hold the separated
     username and domain strings.
   - Signaling adapters are the explicit normalization boundary: they must
     strip prefixes on egress when creating Protobuf `JabberId` and handle
     prefix addition/stripping on ingress when constructing `SignalingAddress`.

3. **Transport Decoupling, SDP Line Endings & HMAC-SHA256 Signature**:
   - `SessionInitiate` and `SessionAccept` actions are dedicated strictly to
     session establishment and authentication payloads.
   - WebRTC session descriptions (`SessionDescription`: SDP string with CRLF
     `\r\n` line endings plus a trailing `\r\n`, and 32-byte HMAC-SHA256
     `signature`) and candidate lists (`IceCandidate`: `candidate`, `sdp_mid`,
     `sdp_m_line_index`) must be encapsulated within `TransportInfo` actions.
   - **SDP Line Ending Normalization**: Per W3C XML §2.11, XML parsers
     normalize line endings (`\r\n` and `\r`) to LF (`\n`) upon parsing
     `<session-description>`. Implementations ingesting XML MUST normalize
     non-empty trimmed SDP lines to `\r\n`-delimited lines with a trailing
     `\r\n` (matching Chromium's `SdpMessage::ToString()`) before populating
     `SessionDescription.sdp` or passing to WebRTC parsers
     (`webrtc::SdpDeserialize` / `RTCPeerConnection.setRemoteDescription`).
   - **SDP Signature Formula**: Every WebRTC SDP offer and answer exchanged in
     an authenticated session MUST be signed with HMAC-SHA256 using the full
     **64-byte** SPAKE2 `auth_key` (`SPAKE2_MAX_KEY_SIZE`, untruncated) over
     the UTF-8 string `"<type> " + normalized_sdp`, where `<type>` is lowercase
     `"offer"` or `"answer"`, followed by a single ASCII space, followed by all
     non-empty whitespace-trimmed SDP lines joined by `\n` plus a trailing `\n`
     (matching Chromium's `SdpMessage::NormalizedForSignature()`).

4. **Structured Attachments vs Ad-Hoc String Delimiters**:
   - Host configuration key-value settings must use
     `HostConfigAttachment.settings` (`map<string, string>`) rather than
     comma/colon-delimited text bodies or ad-hoc XML attributes.
   - Host attributes must use `HostAttributesAttachment.attribute`
     (`repeated string`).
   - Extensions must be encapsulated using the `oneof attachment` within
     `repeated Attachment attachments`.

5. **Forward-Compatible Error & Termination Modeling**:
   - `SessionTerminate.reason` is a strict enum (`Reason`) mirroring XEP-0166
     standard reasons.
   - `SessionTerminate.error_code` is serialized as a `string` (e.g.,
     `"MAX_SESSION_LENGTH"`) to maintain forward and backward compatibility
     across mismatched client/host releases without requiring synchronized
     proto schema updates.
   - IQ responses use `ftl.JingleReply` for successful replies (`REPLY_RESULT`)
     and `ftl.ErrorStanza` (`Condition` enum + optional `text`) for error
     responses (`REPLY_ERROR`).
   - **IQ Result Serialization Rule**: When converting an `IqStanza` with
     `reply` (`JingleReply`) to XML, an empty
     `<jingle xmlns="urn:xmpp:jingle:1" />` child element MUST be generated
     inside the `<iq type="result">` element to satisfy XEP-0166 §6.1 and
     Chromium XML parser expectations.

6. **Omission of Legacy & Dead XML Constructs**:
   - Obsolete XML constructs (direct `<jingle><pairing-failed/></jingle>`
     children outside `<authentication>`, `<custom-tag>` generic `session-info`
     payloads) and test-only fields (`test_id`, `test_key`) are excluded from
     the wire Protobuf schema. `<pairing-failed error="...">` inside
     `<authentication>` is represented on the Protobuf wire via
     `Authentication.pairing_failed.error` (`PairingFailed` at field `9`, added
     in https://crrev.com/c/8829951); see Section 3.2 ("Pairing Error") for the
     normative XML nesting and `"unknown"` empty-error normalization rules.

---

## 4. Canonical Test Scenarios & Golden Vectors

> **Fixture Stanza ID Convention**: Cross-platform converter test suites
> (Chromium C++, Web TypeScript, Android Kotlin) use the human-readable stanza
> ID constants below (note that these static fixture IDs split into 3+
> non-empty `_`-separated tokens and therefore intentionally bypass
> `OrderedMessageQueue` sequencing, whereas live `JingleSession` request traffic
> MUST emit 1-based `<alphanumeric_prefix>_<seq>` IDs such as `a1b2c3d4_1` per
> Section 3.1):
> - Scenario 1: `msg_initiate_001`
> - Scenario 2A: `msg_initiate_002a`
> - Scenario 2B: `msg_initiate_002b`
> - Scenario 2C: `msg_info_002c`
> - Scenario 3A: `msg_accept_003a`
> - Scenario 3B: `msg_accept_003b`
> - Scenario 4: `msg_info_004`
> - Scenario 5: `msg_transport_005`
> - Scenario 6A: `msg_transport_006a`
> - Scenario 6B: `msg_restart_006b`
> - Scenario 7A: `msg_term_clean`
> - Scenario 7B: `msg_term_err`
> - Scenario 8A: `msg_reply_008_res`
> - Scenario 8B: `msg_reply_008_bad_req`, `msg_reply_008_not_impl`,
>   `msg_reply_008_invalid_sid`, `msg_reply_008_unexp_req`,
>   `msg_reply_008_unsupp_info`, `msg_reply_008_unspec`
>
> **Protobuf Wire & Map Comparison Guidance**: Protobuf map fields (such as
> `HostConfigAttachment.settings`) may be serialized to TextFormat as either
> `{ key: "..." value: "..." }` or JSON-style `{ "key": "value" }` depending
> on the protobuf library runtime. Test fixtures verifying wire parity across
> platforms must compare deserialized in-memory protobuf objects or normalized
> binary wire bytes rather than performing strict text-format string diffs.

### Scenario 1: `session-initiate` with Corp SessionAuthz & Host Config

#### XML (Chromium Native Output)
```xml
<iq
    to="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    from="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    type="set"
    id="msg_initiate_001"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="session-initiate"
      sid="crd_sess_987654321"
      initiator=
        "user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111">
    <content name="chromoting" creator="initiator">
      <description xmlns="google:remoting">
        <authentication
            supported-methods=
                "corp_session_authz_spake2_curve25519,spake2_curve25519">
          <host-token>
            aG9zdF9hdXRoel90b2tlbl9leGFtcGxlXzEyMzQ1Njc4OTA=
          </host-token>
        </authentication>
      </description>
      <transport xmlns="google:remoting:webrtc" />
    </content>
    <attachments xmlns="google:remoting">
      <host-configuration>Av1-Encoder-Speed:11,EnableMultimon:true,VideoCodecPreset:high_quality</host-configuration>
    </attachments>
  </jingle>
</iq>
```

#### Protobuf (`ftl.IqStanza` Canonical Wire Form)
```protobuf
id: "msg_initiate_001"
sender {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
receiver {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
jingle {
  session_id: "crd_sess_987654321"
  session_initiate {
    initiator {
      local_part: "user"
      domain_part: "example.com"
      resource_part: "11111111-1111-1111-1111-111111111111"
    }
    authentication {
      supported_methods:
          AUTHENTICATION_METHOD_CORP_SESSION_AUTHZ_SPAKE2_CURVE25519
      supported_methods: AUTHENTICATION_METHOD_SPAKE2_CURVE25519
      session_authz_host_token: "host_authz_token_example_1234567890"
    }
  }
  attachments {
    host_config {
      settings {
        key: "Av1-Encoder-Speed"
        value: "11"
      }
      settings {
        key: "VideoCodecPreset"
        value: "high_quality"
      }
      settings {
        key: "EnableMultimon"
        value: "true"
      }
    }
  }
}
```

---

### Scenario 2A: `session-initiate` with Standard PIN Authentication (Unpaired)

#### XML (Chromium Native Output)
```xml
<iq
    to="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    from="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    type="set"
    id="msg_initiate_002a"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="session-initiate"
      sid="crd_sess_987654321"
      initiator=
        "user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111">
    <content name="chromoting" creator="initiator">
      <description xmlns="google:remoting">
        <authentication
            supported-methods="pair_spake2_curve25519,spake2_curve25519" />
      </description>
      <transport xmlns="google:remoting:webrtc" />
    </content>
  </jingle>
</iq>
```

#### Protobuf (`ftl.IqStanza` Canonical Wire Form)
```protobuf
id: "msg_initiate_002a"
sender {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
receiver {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
jingle {
  session_id: "crd_sess_987654321"
  session_initiate {
    initiator {
      local_part: "user"
      domain_part: "example.com"
      resource_part: "11111111-1111-1111-1111-111111111111"
    }
    authentication {
      supported_methods: AUTHENTICATION_METHOD_PAIRED_SPAKE2_CURVE25519
      supported_methods: AUTHENTICATION_METHOD_SPAKE2_CURVE25519
    }
  }
}
```

---

### Scenario 2B: `session-initiate` with Paired SPAKE2 Authentication

#### XML (Chromium Native Output)
```xml
<iq
    to="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    from="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    type="set"
    id="msg_initiate_002b"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="session-initiate"
      sid="crd_sess_987654321"
      initiator=
        "user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111">
    <content name="chromoting" creator="initiator">
      <description xmlns="google:remoting">
        <authentication
            supported-methods="pair_spake2_curve25519,spake2_curve25519">
          <pairing-info client-id="paired_client_uuid_abc123" />
        </authentication>
      </description>
      <transport xmlns="google:remoting:webrtc" />
    </content>
  </jingle>
</iq>
```

#### Protobuf (`ftl.IqStanza` Canonical Wire Form)
```protobuf
id: "msg_initiate_002b"
sender {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
receiver {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
jingle {
  session_id: "crd_sess_987654321"
  session_initiate {
    initiator {
      local_part: "user"
      domain_part: "example.com"
      resource_part: "11111111-1111-1111-1111-111111111111"
    }
    authentication {
      supported_methods: AUTHENTICATION_METHOD_PAIRED_SPAKE2_CURVE25519
      supported_methods: AUTHENTICATION_METHOD_SPAKE2_CURVE25519
      pairing_info {
        client_id: "paired_client_uuid_abc123"
      }
    }
  }
}
```

---

### Scenario 2C: `session-info` with PIN Pairing Fallback (`<pairing-failed>`)

When a paired client's `client-id` is found in the host's pairing registry on
`session-initiate` (so the host sends `session-accept` using the stored paired
secret), and the paired SPAKE2 shared-secret check subsequently fails in
`PairingAuthenticatorBase::CheckForFailedSpakeExchange`, the host resets to a
PIN-based `Spake2Authenticator` and issues a `session-info` stanza carrying the
new PIN-derived `<spake-message>`, `<certificate>`, and
`<pairing-failed error="invalid-shared-secret" />` to trigger client fallback
to manual PIN entry.

#### XML (Chromium Native Output)
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="set"
    id="msg_info_002c"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="session-info"
      sid="crd_sess_987654321">
    <authentication xmlns="google:remoting">
      <spake-message>
        AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA=
      </spake-message>
      <certificate>
        MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyN3rL6u+eG8H9o3F
        7w4f1mK2s8u1v9X0y5z6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6
        c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6
        c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6
        c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6
        c7d8e9f0a1b2c3d4e5f6a7b8c9IDAQAB
      </certificate>
      <pairing-failed error="invalid-shared-secret" />
    </authentication>
  </jingle>
</iq>
```

#### Protobuf (`ftl.IqStanza` Canonical Wire Form)
```protobuf
id: "msg_info_002c"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
jingle {
  session_id: "crd_sess_987654321"
  session_info {
    authentication {
      spake_message:
          "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
          "\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20"
      certificate:
          "\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01"
          "\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01"
          "\x00\xc8\xdd\xeb\x2f\xab\xbe\x78\x6f\x07\xf6\x8d\xc5\xef\x0e\x1f"
          "\xd6\x62\xb6\xb3\xcb\xb5\xbf\xd5\xf4\xcb\x9c\xfa\x6b\xb6\xfc\x73"
          "\xd7\x74\x7b\x57\xf6\x6b\x76\xf8\x73\x97\x7a\x7b\xb7\xfc\x6b\xd6"
          "\xf4\x73\x57\x76\x7b\x77\xf8\x6b\x96\xfa\x73\xb7\x7c\x7b\xd7\xf4"
          "\x6b\x56\xf6\x73\x77\x78\x7b\x97\xfa\x6b\xb6\xfc\x73\xd7\x74\x7b"
          "\x57\xf6\x6b\x76\xf8\x73\x97\x7a\x7b\xb7\xfc\x6b\xd6\xf4\x73\x57"
          "\x76\x7b\x77\xf8\x6b\x96\xfa\x73\xb7\x7c\x7b\xd7\xf4\x6b\x56\xf6"
          "\x73\x77\x78\x7b\x97\xfa\x6b\xb6\xfc\x73\xd7\x74\x7b\x57\xf6\x6b"
          "\x76\xf8\x73\x97\x7a\x7b\xb7\xfc\x6b\xd6\xf4\x73\x57\x76\x7b\x77"
          "\xf8\x6b\x96\xfa\x73\xb7\x7c\x7b\xd7\xf4\x6b\x56\xf6\x73\x77\x78"
          "\x7b\x97\xfa\x6b\xb6\xfc\x73\xd7\x74\x7b\x57\xf6\x6b\x76\xf8\x73"
          "\x97\x7a\x7b\xb7\xfc\x6b\xd6\xf4\x73\x57\x76\x7b\x77\xf8\x6b\x96"
          "\xfa\x73\xb7\x7c\x7b\xd7\xf4\x6b\x56\xf6\x73\x77\x78\x7b\x97\xfa"
          "\x6b\xb6\xfc\x73\xd2\x03\x01\x00\x01"
      pairing_failed {
        error: "invalid-shared-secret"
      }
    }
  }
}
```

> **Pairing Fallback Contexts (`remoting/protocol/pairing_*_authenticator.cc`)**:
> Chromium's authenticators always attach `pairing_error` via
> `PairingAuthenticatorBase::GetNextMessage()` onto the newly created PIN
> `Spake2Authenticator`'s first message (never as a standalone
> `<pairing-failed>` without `spake_message`):
> 1. **Host `session_accept.authentication`**: When the host has no stored
>    pairing for the client (`"unknown-client-id"` when `client_id` is
>    unrecognized, or `"client-id-unknown"` when `client_id` is empty),
>    `NegotiatingHostAuthenticator` still sets
>    `method: AUTHENTICATION_METHOD_PAIRED_SPAKE2_CURVE25519`
>    (`method="pair_spake2_curve25519"`) alongside the PIN-derived
>    `spake_message` and mandatory `certificate` so the client instantiates
>    `PairingClientAuthenticator` and retains re-pairing capability when
>    prompting for the PIN.
> 2. **Host or Client `session_info.authentication`**: When paired SPAKE2 key
>    verification fails (`"invalid-shared-secret"`, emitted by either host or
>    client alongside the new PIN-derived `spake_message`, plus `certificate`
>    when host-originated as in Scenario 2C), or when `PairingClientAuthenticator`
>    starts without local pairing credentials (`"not-paired"`, client-originated
>    alongside the client's PIN-derived `spake_message`). (Note: while
>    `Spake2Authenticator` requires `spake_message`--and `certificate` from the
>    host--converter implementations still preserve `pairing_failed` even if
>    other fields are absent in synthetic converter unit tests.)
>
> All `pairing_failed.error` strings are opaque diagnostic labels for logging
> and debugging; implementations MUST trigger fallback based on
> `pairing_failed` presence (per Section 3.2) rather than matching specific
> error strings.

---

### Scenario 3A: `session-accept` with Corp SessionAuthz & Host Attributes

#### XML (Chromium Native Output)
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="set"
    id="msg_accept_003a"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="session-accept"
      sid="crd_sess_987654321">
    <content name="chromoting" creator="initiator">
      <description xmlns="google:remoting">
        <authentication method="corp_session_authz_spake2_curve25519">
          <spake-message>
            AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA=
          </spake-message>
          <session-token>
            c2Vzc2lvbl9hdXRoel90b2tlbl9leGFtcGxlXzA5ODc2NTQzMjE=
          </session-token>
        </authentication>
      </description>
      <transport xmlns="google:remoting:webrtc" />
    </content>
    <attachments xmlns="google:remoting">
      <host-attributes>Debug-Build,HWEncoder,SupportsIceDatagramTransport</host-attributes>
    </attachments>
  </jingle>
</iq>
```

#### Protobuf (`ftl.IqStanza` Canonical Wire Form)
```protobuf
id: "msg_accept_003a"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
jingle {
  session_id: "crd_sess_987654321"
  session_accept {
    authentication {
      method: AUTHENTICATION_METHOD_CORP_SESSION_AUTHZ_SPAKE2_CURVE25519
      session_authz_session_token: "session_authz_token_example_0987654321"
      spake_message:
          "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
          "\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20"
    }
  }
  attachments {
    host_attributes {
      attribute: "Debug-Build"
      attribute: "HWEncoder"
      attribute: "SupportsIceDatagramTransport"
    }
  }
}
```

> **Content Creator Note**: Per XEP-0166 §6.4, the `<content>` element in
> `session-accept` must retain `creator="initiator"` to identify the original
> creator of the content description, not `creator="responder"`.

---

### Scenario 3B: `session-accept` with SPAKE2 PIN & Host Certificate

#### XML (Chromium Native Output)
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="set"
    id="msg_accept_003b"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="session-accept"
      sid="crd_sess_987654321">
    <content name="chromoting" creator="initiator">
      <description xmlns="google:remoting">
        <authentication method="spake2_curve25519">
          <spake-message>
            AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA=
          </spake-message>
          <certificate>
            MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyN3rL6u+eG8H9o3F
            7w4f1mK2s8u1v9X0y5z6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6
            c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6
            c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6
            c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6
            c7d8e9f0a1b2c3d4e5f6a7b8c9IDAQAB
          </certificate>
        </authentication>
      </description>
      <transport xmlns="google:remoting:webrtc" />
    </content>
  </jingle>
</iq>
```

#### Protobuf (`ftl.IqStanza` Canonical Wire Form)
```protobuf
id: "msg_accept_003b"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
jingle {
  session_id: "crd_sess_987654321"
  session_accept {
    authentication {
      method: AUTHENTICATION_METHOD_SPAKE2_CURVE25519
      spake_message:
          "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
          "\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20"
      certificate:
          "\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01"
          "\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01"
          "\x00\xc8\xdd\xeb\x2f\xab\xbe\x78\x6f\x07\xf6\x8d\xc5\xef\x0e\x1f"
          "\xd6\x62\xb6\xb3\xcb\xb5\xbf\xd5\xf4\xcb\x9c\xfa\x6b\xb6\xfc\x73"
          "\xd7\x74\x7b\x57\xf6\x6b\x76\xf8\x73\x97\x7a\x7b\xb7\xfc\x6b\xd6"
          "\xf4\x73\x57\x76\x7b\x77\xf8\x6b\x96\xfa\x73\xb7\x7c\x7b\xd7\xf4"
          "\x6b\x56\xf6\x73\x77\x78\x7b\x97\xfa\x6b\xb6\xfc\x73\xd7\x74\x7b"
          "\x57\xf6\x6b\x76\xf8\x73\x97\x7a\x7b\xb7\xfc\x6b\xd6\xf4\x73\x57"
          "\x76\x7b\x77\xf8\x6b\x96\xfa\x73\xb7\x7c\x7b\xd7\xf4\x6b\x56\xf6"
          "\x73\x77\x78\x7b\x97\xfa\x6b\xb6\xfc\x73\xd7\x74\x7b\x57\xf6\x6b"
          "\x76\xf8\x73\x97\x7a\x7b\xb7\xfc\x6b\xd6\xf4\x73\x57\x76\x7b\x77"
          "\xf8\x6b\x96\xfa\x73\xb7\x7c\x7b\xd7\xf4\x6b\x56\xf6\x73\x77\x78"
          "\x7b\x97\xfa\x6b\xb6\xfc\x73\xd7\x74\x7b\x57\xf6\x6b\x76\xf8\x73"
          "\x97\x7a\x7b\xb7\xfc\x6b\xd6\xf4\x73\x57\x76\x7b\x77\xf8\x6b\x96"
          "\xfa\x73\xb7\x7c\x7b\xd7\xf4\x6b\x56\xf6\x73\x77\x78\x7b\x97\xfa"
          "\x6b\xb6\xfc\x73\xd2\x03\x01\x00\x01"
    }
  }
}
```

> **Certificate Base64 Ingestion Note**: The `<certificate>` XML body text
> spans multiple lines with leading indentation. XML parsers must strip all
> internal whitespace (`[ \t\r\n]`) before base64 decoding (as performed by
> Chromium's `base::CollapseWhitespaceASCII`), avoiding errors in strict
> decoders such as browser `atob()`.

> **Certificate Mandatory in `session-accept`**: The host certificate is
> mandatory in `session_accept.authentication.certificate` as raw DER `bytes`
> for all SPAKE2 / PIN-based authentication flows. Client implementations
> (including Chromium native `Spake2Authenticator`) strictly require the host
> certificate in the first incoming authentication message and will immediately
> reject the session with `INVALID_STATE` ("No valid host certificate.") if it is
> omitted.

---

### Scenario 4: `session-info` for Multi-Round SPAKE2 Auth

#### XML (Chromium Native Output)
```xml
<iq
    to="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    from="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    type="set"
    id="msg_info_004"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="session-info"
      sid="crd_sess_987654321">
    <authentication xmlns="google:remoting">
      <verification-hash>
        MDEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2Nzg5MDE=
      </verification-hash>
    </authentication>
  </jingle>
</iq>
```

#### Protobuf (`ftl.IqStanza` Canonical Wire Form)
```protobuf
id: "msg_info_004"
sender {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
receiver {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
jingle {
  session_id: "crd_sess_987654321"
  session_info {
    authentication {
      verification_hash: "01234567890123456789012345678901"
    }
  }
}
```

---

### Scenario 5: `transport-info` Trickle ICE Candidate Batch

#### XML (Chromium Native Output)
```xml
<iq
    to="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    from="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    type="set"
    id="msg_transport_005"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="transport-info"
      sid="crd_sess_987654321">
    <content name="chromoting" creator="initiator">
      <transport xmlns="google:remoting:webrtc">
        <candidate sdpMid="0" sdpMLineIndex="0">
          candidate:1001 1 udp 2122260223 192.168.1.150 54321 typ host
          generation 0
        </candidate>
        <candidate sdpMid="0" sdpMLineIndex="0">
          candidate:1002 1 udp 2122260222 2607:f8b0:4005:805::200e 54322 typ
          host generation 0
        </candidate>
        <candidate sdpMid="1" sdpMLineIndex="1">
          candidate:1003 1 tcp 1518280447 192.168.1.150 9 typ host
          tcptype active generation 0
        </candidate>
        <candidate sdpMid="1" sdpMLineIndex="1">
          candidate:2001 1 udp 1686052607 74.125.250.1 54321 typ srflx
          raddr 192.168.1.150 rport 54321 generation 0
        </candidate>
        <candidate sdpMid="0" sdpMLineIndex="0">
          candidate:3001 1 udp 41885695 74.125.250.200 19302 typ relay
          raddr 74.125.250.1 rport 54321 generation 0
        </candidate>
      </transport>
    </content>
  </jingle>
</iq>
```

#### Protobuf (`ftl.IqStanza` Canonical Wire Form)
```protobuf
id: "msg_transport_005"
sender {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
receiver {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
jingle {
  session_id: "crd_sess_987654321"
  transport_info {
    candidates {
      candidate:
          "candidate:1001 1 udp 2122260223 192.168.1.150 54321 typ host"
          " generation 0"
      sdp_mid: "0"
      sdp_m_line_index: 0
    }
    candidates {
      candidate:
          "candidate:1002 1 udp 2122260222 2607:f8b0:4005:805::200e 54322 typ"
          " host generation 0"
      sdp_mid: "0"
      sdp_m_line_index: 0
    }
    candidates {
      candidate:
          "candidate:1003 1 tcp 1518280447 192.168.1.150 9 typ host tcptype"
          " active generation 0"
      sdp_mid: "1"
      sdp_m_line_index: 1
    }
    candidates {
      candidate:
          "candidate:2001 1 udp 1686052607 74.125.250.1 54321 typ srflx"
          " raddr 192.168.1.150 rport 54321 generation 0"
      sdp_mid: "1"
      sdp_m_line_index: 1
    }
    candidates {
      candidate:
          "candidate:3001 1 udp 41885695 74.125.250.200 19302 typ relay"
          " raddr 74.125.250.1 rport 54321 generation 0"
      sdp_mid: "0"
      sdp_m_line_index: 0
    }
  }
}
```

> **Candidate Line Whitespace Collapsing**: Multi-line candidate text in
> XML elements must be collapsed into single space-delimited SDP lines
> when ingesting XML, matching standard WebRTC candidate syntax.
>
> **Multi-Stream Candidate Batching**: Candidates within a single
> `transport-info` batch may span multiple media lines (e.g., audio `sdpMid="0"`
> and video `sdpMid="1"`). Golden vectors intentionally interleave stream IDs
> to verify that parsers extract `sdpMid` and `sdpMLineIndex` per candidate
> rather than assuming batch-wide uniformity.

---

### Scenario 6A: `transport-info` with Initial WebRTC SDP Answer & Signature

> **CRD WebRTC Offer/Answer Role Convention**: In Chrome Remote Desktop, the
> Host (`TransportRole::SERVER`) creates and transmits the initial WebRTC SDP
> Offer, while the Client (`TransportRole::CLIENT`) generates and returns the
> WebRTC SDP Answer. Therefore, `transport-info` stanzas containing
> `type="answer"` originate from the client (`from="user@..."`) and target the
> host (`to="host@..."`).

#### XML (Chromium Native Output)
```xml
<iq
    to="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    from="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    type="set"
    id="msg_transport_006a"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="transport-info"
      sid="crd_sess_987654321">
    <content name="chromoting" creator="initiator">
      <transport xmlns="google:remoting:webrtc">
        <session-description
            type="answer"
            signature="8lQoAhaKAcktnMveS0dDfWv6x7mfNhAhisfU5aZ8QOk=">
v=0
o=- 5123456789012345 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1 2
m=audio 9 UDP/TLS/RTP/SAVPF 111
c=IN IP4 0.0.0.0
a=ice-ufrag:ClientUfrag123
a=ice-pwd:ClientPassword1234567890
a=fingerprint:sha-256 AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99
a=setup:active
a=mid:0
a=sendrecv
a=rtcp-mux
a=rtpmap:111 opus/48000/2
        </session-description>
      </transport>
    </content>
  </jingle>
</iq>
```

#### Protobuf (`ftl.IqStanza` Canonical Wire Form)
```protobuf
id: "msg_transport_006a"
sender {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
receiver {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
jingle {
  session_id: "crd_sess_987654321"
  transport_info {
    session_description {
      type: ANSWER
      sdp: "v=0\r\no=- 5123456789012345 2 IN IP4 127.0.0.1\r\ns=-\r\n"
           "t=0 0\r\na=group:BUNDLE 0 1 2\r\n"
           "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\nc=IN IP4 0.0.0.0\r\n"
           "a=ice-ufrag:ClientUfrag123\r\n"
           "a=ice-pwd:ClientPassword1234567890\r\n"
           "a=fingerprint:sha-256 AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:"
           "88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99\r\n"
           "a=setup:active\r\na=mid:0\r\na=sendrecv\r\na=rtcp-mux\r\n"
           "a=rtpmap:111 opus/48000/2\r\n"
      signature:
          "\xf2\x54\x28\x02\x16\x8a\x01\xc9\x2d\x9c\xcb\xde\x4b\x47\x43\x7d"
          "\x6b\xfa\xc7\xb9\x9f\x36\x10\x21\x8a\xc7\xd4\xe5\xa6\x7c\x40\xe9"
    }
  }
}
```

> **Test Key Note (`signature` Verification)**: The 32-byte `signature` in
> Scenario 6A is the real HMAC-SHA256 digest computed per Section 3.4.3 over
> `"answer " + NormalizedForSignature(sdp)` using the 64-byte test `auth_key`
> consisting of sequential bytes `0x00, 0x01, ..., 0x3f`.

---

### Scenario 6B: `transport-info` for ICE Restart (SDP Offer)

#### XML (Chromium Native Output)
```xml
<iq
    to="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    from="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    type="set"
    id="msg_restart_006b"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="transport-info"
      sid="crd_sess_987654321">
    <content name="chromoting" creator="initiator">
      <transport xmlns="google:remoting:webrtc">
        <session-description type="offer">
v=0
o=- 4123456789012345 3 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1 2
m=audio 9 UDP/TLS/RTP/SAVPF 111
c=IN IP4 0.0.0.0
a=ice-ufrag:NewClientUfrag
a=ice-pwd:NewClientPassword1234567890
a=fingerprint:sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF
a=setup:actpass
a=mid:0
a=sendrecv
a=rtcp-mux
a=rtpmap:111 opus/48000/2
        </session-description>
        <candidate
            sdpMid="0"
            sdpMLineIndex="0">
          candidate:5001 1 udp 2122260223 10.0.0.5 54321 typ host generation 1
        </candidate>
      </transport>
    </content>
  </jingle>
</iq>
```

#### Protobuf (`ftl.IqStanza` Canonical Wire Form)
```protobuf
id: "msg_restart_006b"
sender {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
receiver {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
jingle {
  session_id: "crd_sess_987654321"
  transport_info {
    session_description {
      type: OFFER
      sdp: "v=0\r\no=- 4123456789012345 3 IN IP4 127.0.0.1\r\ns=-\r\n"
           "t=0 0\r\na=group:BUNDLE 0 1 2\r\n"
           "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\nc=IN IP4 0.0.0.0\r\n"
           "a=ice-ufrag:NewClientUfrag\r\n"
           "a=ice-pwd:NewClientPassword1234567890\r\n"
           "a=fingerprint:sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:"
           "EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF\r\n"
           "a=setup:actpass\r\na=mid:0\r\na=sendrecv\r\na=rtcp-mux\r\n"
           "a=rtpmap:111 opus/48000/2\r\n"
    }
    candidates {
      candidate:
          "candidate:5001 1 udp 2122260223 10.0.0.5 54321 typ host"
          " generation 1"
      sdp_mid: "0"
      sdp_m_line_index: 0
    }
  }
}
```

> **Converter Fixture Note (`signature` Presence)**: Scenario 6B intentionally
> omits `signature` to exercise converter handling when the optional wire field
> is unset alongside bundled ICE candidates. At the `JingleSession` /
> `WebrtcTransport` layer, all live SDP offers and answers (including ICE
> restarts) MUST carry a valid HMAC-SHA256 `signature` per Section 3.4.3.

---

### Scenario 7: `session-terminate` (Clean Disconnect vs Error Conditions)

#### A. Clean Disconnect (`SUCCESS`)
**XML:**
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="set"
    id="msg_term_clean"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="session-terminate"
      sid="crd_sess_987654321">
    <reason><success/></reason>
  </jingle>
</iq>
```
**Protobuf:**
```protobuf
id: "msg_term_clean"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
jingle {
  session_id: "crd_sess_987654321"
  session_terminate { reason: SUCCESS }
}
```

> **Note (`error_code` on Clean Close)**: Scenario 7A illustrates the minimal
> `SUCCESS` stanza with `error_code` omitted. Live `JingleSession::Close(OK)`
> additionally emits `<error-code xmlns="google:remoting">OK</error-code>` /
> `error_code: "OK"`; on ingress, omitted `error_code` maps to `OK` once past
> `CONNECTING` (or `SESSION_REJECTED` during `CONNECTING`).

#### B. Error Disconnect (`MAX_SESSION_LENGTH`)
**XML:**
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="set"
    id="msg_term_err"
    xmlns="jabber:client">
  <jingle
      xmlns="urn:xmpp:jingle:1"
      action="session-terminate"
      sid="crd_sess_987654321">
    <reason><expired/></reason>
    <error-code xmlns="google:remoting">
      MAX_SESSION_LENGTH
    </error-code>
    <error-details xmlns="google:remoting">
      The maximum allowed session duration (20 hours) has elapsed.
    </error-details>
    <error-location xmlns="google:remoting">
      remoting/host/client_session.cc:512
    </error-location>
  </jingle>
</iq>
```
**Protobuf:**
```protobuf
id: "msg_term_err"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
jingle {
  session_id: "crd_sess_987654321"
  session_terminate {
    reason: EXPIRED
    error_code: "MAX_SESSION_LENGTH"
    error_details:
        "The maximum allowed session duration (20 hours) has elapsed."
    error_location: "remoting/host/client_session.cc:512"
  }
}
```

---

### Scenario 8: IQ Responses (Success Result vs All 6 Error Conditions)

#### A. IQ Result (Success Reply)

**XML (Chromium Native Output):**
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="result"
    id="msg_reply_008_res"
    xmlns="jabber:client">
  <jingle xmlns="urn:xmpp:jingle:1" />
</iq>
```

**Protobuf (`ftl.IqStanza` Canonical Wire Form):**
```protobuf
id: "msg_reply_008_res"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
reply {
}
```

#### B. IQ Error Reply: `BAD_REQUEST`

**XML (Chromium Native Output):**
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="error"
    id="msg_reply_008_bad_req"
    xmlns="jabber:client">
  <error type="modify" xmlns="jabber:client">
    <bad-request />
  </error>
</iq>
```

**Protobuf (`ftl.IqStanza` Canonical Wire Form):**
```protobuf
id: "msg_reply_008_bad_req"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
error {
  condition: BAD_REQUEST
}
```

#### C. IQ Error Reply: `NOT_IMPLEMENTED`

**XML (Chromium Native Output):**
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="error"
    id="msg_reply_008_not_impl"
    xmlns="jabber:client">
  <error type="cancel" xmlns="jabber:client">
    <feature-bad-request />
  </error>
</iq>
```

**Protobuf (`ftl.IqStanza` Canonical Wire Form):**
```protobuf
id: "msg_reply_008_not_impl"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
error {
  condition: NOT_IMPLEMENTED
}
```

#### D. IQ Error Reply: `INVALID_SID`

**XML (Chromium Native Output):**
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="error"
    id="msg_reply_008_invalid_sid"
    xmlns="jabber:client">
  <error type="modify" xmlns="jabber:client">
    <item-not-found />
    <text xml:lang="en">Invalid SID</text>
  </error>
</iq>
```

**Protobuf (`ftl.IqStanza` Canonical Wire Form):**
```protobuf
id: "msg_reply_008_invalid_sid"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
error {
  condition: INVALID_SID
  text: "Invalid SID"
}
```

#### E. IQ Error Reply: `UNEXPECTED_REQUEST`

**XML (Chromium Native Output):**
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="error"
    id="msg_reply_008_unexp_req"
    xmlns="jabber:client">
  <error type="modify" xmlns="jabber:client">
    <unexpected-request />
  </error>
</iq>
```

**Protobuf (`ftl.IqStanza` Canonical Wire Form):**
```protobuf
id: "msg_reply_008_unexp_req"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
error {
  condition: UNEXPECTED_REQUEST
}
```

#### F. IQ Error Reply: `UNSUPPORTED_INFO`

**XML (Chromium Native Output):**
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="error"
    id="msg_reply_008_unsupp_info"
    xmlns="jabber:client">
  <error type="modify" xmlns="jabber:client">
    <feature-not-implemented />
  </error>
</iq>
```

**Protobuf (`ftl.IqStanza` Canonical Wire Form):**
```protobuf
id: "msg_reply_008_unsupp_info"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
error {
  condition: UNSUPPORTED_INFO
}
```

#### G. IQ Error Reply: `UNSPECIFIED`

**XML (Chromium Native Output):**
```xml
<iq
    to="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    from="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    type="error"
    id="msg_reply_008_unspec"
    xmlns="jabber:client">
  <error type="cancel" xmlns="jabber:client">
    <unspecified-error />
  </error>
</iq>
```

**Protobuf (`ftl.IqStanza` Canonical Wire Form):**
```protobuf
id: "msg_reply_008_unspec"
sender {
  local_part: "host"
  domain_part: "example.com"
  resource_part: "55555555-5555-5555-5555-555555555555"
}
receiver {
  local_part: "user"
  domain_part: "example.com"
  resource_part: "11111111-1111-1111-1111-111111111111"
}
error {
  condition: CONDITION_UNSPECIFIED
}
```

---

## 5. Complete Signaling State Transitions

### 5.1 Connection State Machine Matrix

```text
  +------------------+
  |   INITIALIZING   |
  +--------+---------+
           | Send session-initiate (Auth / Jingle)
           v
  +------------------+
  |    CONNECTING    |
  +--------+---------+
           | Receive session-accept / session-info
           v
  +------------------+     Auth failed / Invalid Token      +------------+
  |  AUTHENTICATING  +------------------------------------->+   FAILED   |
  +--------+---------+                                      +------------+
           | SPAKE2 / SessionAuthz verified                        ^
           v                                                       |
  +------------------+                                             |
  |  AUTHENTICATED   |                                             |
  +--------+---------+                                             |
           | Exchange transport-info (SDP Offer/Answer, ICE)       |
           v                                                       |
  +------------------+     ICE Disconnect / ICE Restart            |
  |    CONNECTED     +-----------------------------------+         |
  +--------+---------+                                   |         |
           |                                             v         |
           | Receive / Send              +----------------------+  |
           | session-terminate (SUCCESS) |     RECONNECTING     +--+
           v                             +-----------+----------+ ICE Timeout
  +------------------+                               | ICE Restart success
  |      CLOSED      |<------------------------------+
  +------------------+
```

### 5.2 Out-of-Order / Race Handling Scenarios

- **Out-of-Order Stanza Delivery (`OrderedMessageQueue`)**:
  - *Sequence*: Jingle `SET` stanzas arrive out of order over FTL (e.g.,
    `<prefix>_3` arrives before `<prefix>_2`).
  - *Handling*: `OrderedMessageQueue` buffers sequenced stanzas keyed by their
    1-based `<seq>` integer and drains contiguous stanzas strictly in ascending
    order once `<prefix>_2` arrives.
- **Early `transport-info` & Trickle ICE Candidates**:
  - *Sequence A*: `transport-info` arrives while `state == CONNECTING` (before
    `session-accept`).
  - *Handling A*: Rejected immediately with IQ error `UNEXPECTED_REQUEST`.
  - *Sequence B*: `transport-info` arrives while `state == AUTHENTICATING`
    (after `session-accept`, while multi-round SPAKE2 `session-info` messages
    are still completing).
  - *Handling B*: `JingleSession` buffers the message in
    `pending_transport_info_` and replays it in order immediately upon
    transitioning to `AUTHENTICATED`.
  - *Sequence C*: Once `AUTHENTICATED`, Trickle ICE candidates arrive before
    the initial `transport-info` SDP Offer/Answer is applied.
  - *Handling C*: Transport buffers candidates in
    `pending_incoming_candidates_` and applies them immediately once
    `SetRemoteDescription` completes on the peer connection.
- **Concurrent Disconnect**:
  - *Sequence*: Host sends `session-terminate` while Client sends
    `transport-info`.
  - *Handling*: Host responds with IQ error (`INVALID_SID`). Client receives
    `session-terminate`, destroys session manager instance, and stops
    transmitting.
- **HTTP 401 Unauthenticated**:
  - *Sequence*: FTL signaling channel receives HTTP 401 during `SendMessage`.
  - *Handling*: `FtlSignalStrategy` triggers OAuth token cache invalidation and
    reconnects; drops non-idempotent in-flight stanzas and retries connection
    handshake if session state allows.
- **Stanza ID Mismatch**:
  - *Sequence*: IQ Reply received with unrecognized ID.
  - *Handling*: Reply is dropped with `VLOG(1)` warning; session state is
    unchanged.

---

## 6. Implementation & Parity Checklist for Web Client & Android

To ensure zero regressions against Chromium before the branch point:

### 6.1 Web Client Parity Checklist
- [ ] Parse `ftl.IqStanza` directly without intermediate XML conversion when
      `iq_stanza` field is present, and implement the format negotiation,
      `BOTH`-reply `XML` fallback, pre-reply `XML`-only non-initiate stanza
      handling, and ingress format tolerance rules in Section 3.3 Rules 2-4
      (https://crrev.com/c/8834622).
- [ ] Format outgoing Jingle `SET` request IDs with an underscore-free
      alphanumeric prefix and **1-based** sequence numbers (`<prefix>_1`,
      `<prefix>_2`, ...), and implement `OrderedMessageQueue` parsing and
      `next_incoming_` initialization per Section 3.1 (filtering empty tokens
      before checking `tokens.length === 2`).
- [ ] Keep `SessionAuthz` `<host-token>` and `<session-token>` unchanged
      (Base64 strings) in serialized XML while encoding Base64-decoded raw
      `bytes` in Protobuf `session_authz_host_token` and
      `session_authz_session_token`.
- [ ] For `IceCandidate` objects, always populate `sdp_mid`,
      `sdp_m_line_index`, and full `candidate:...` string (without `a=`).
- [ ] Emit an empty `<jingle xmlns="urn:xmpp:jingle:1" />` child inside
      `<iq type="result">` when serializing `JingleReply` to XML, and emit
      `<error>`, condition child tags (including `BAD_REQUEST`), and `<text>`
      in the `jabber:client` namespace on XML egress.
- [ ] Enforce `MIN_HOST_VERSION_FOR_PROTOBUF = 160` per the table in
      Section 3.3 Rule 1.
- [ ] Implement `<pairing-failed>` / `authentication.pairing_failed` mapping
      (including `"unknown"` empty-error normalization and `<authentication>`
      XML nesting) and comma-joined `host_attributes.attribute` Protobuf
      ingress splitting per Section 3.2 (https://crrev.com/c/8829951).
- [ ] Include Scenarios 1-8 in web client unit test suite.

### 6.2 Android Parity Checklist
- [ ] In Kotlin/Java proto builders, ensure `JabberId` resource part contains
      raw UUID (no `chromoting_ftl_` prefix).
- [ ] Format outgoing Jingle `SET` request IDs with an underscore-free
      alphanumeric prefix and **1-based** sequence numbers (`<prefix>_1`,
      `<prefix>_2`, ...), and implement `OrderedMessageQueue` parsing and
      `next_incoming_` initialization per Section 3.1 (filtering empty tokens
      before checking `tokens.size == 2`).
- [ ] Pass cryptographic fields (`spake_message`, `verification_hash`,
      `certificate`, `signature`) directly as raw `bytes` in Protobuf, and
      Base64-decode `SessionAuthz` tokens into raw `bytes` in Protobuf while
      leaving serialized XML `<host-token>` and `<session-token>` unchanged.
- [ ] For `IceCandidate` objects, always populate `sdp_mid`,
      `sdp_m_line_index`, and `candidate:...` (without `a=`); normalize XML
      `<session-description>` SDP line endings to `\r\n` with a trailing
      `\r\n` while signing over `\n`-joined lines with the full 64-byte SPAKE2
      `auth_key` per Section 3.4.3.
- [ ] Emit `<error>`, legacy XMPP condition tags, and `<text>` in the
      `jabber:client` namespace on XML egress, implement the egress and ingress
      `SessionTerminate` `ErrorCode` / `Reason` mappings in Section 3.2, and
      reject `transport-info` in `CONNECTING` with `UNEXPECTED_REQUEST`
      (buffering only in `AUTHENTICATING`).
- [ ] Ensure host configuration settings map keys match exact
      PascalCase/kebab-case strings (`Av1-Encoder-Speed`, `VideoCodecPreset`,
      `EnableMultimon`).
- [ ] Enforce `MIN_HOST_VERSION_FOR_PROTOBUF = 160` in the Android client per
      the table in Section 3.3 Rule 1.
- [ ] Implement `<pairing-failed>` / `authentication.pairing_failed` mapping
      (including `"unknown"` empty-error normalization and `<authentication>`
      XML nesting) and comma-joined `host_attributes.attribute` Protobuf
      ingress splitting per Section 3.2 (https://crrev.com/c/8829951).
- [ ] Ensure host implementations populate raw DER certificate bytes in
      `session_accept.authentication.certificate` during SPAKE2 authentication
      (preserving `AUTHENTICATION_METHOD_PAIRED_SPAKE2_CURVE25519` on
      `session-accept` PIN fallback; omitting `certificate` causes immediate
      client rejection with `INVALID_STATE`: "No valid host certificate").
- [ ] Include Scenarios 1-8 in Android unit test suite.

### 6.3 Chromium Unit Test Plan
- [ ] Implement `remoting/signaling/jingle_compatibility_unittest.cc`
      validating roundtrip invariance of Scenarios 1-8.
- [ ] Ensure all tests pass on Chromium Linux, Windows, Mac, and Android test
      targets before branch cut.

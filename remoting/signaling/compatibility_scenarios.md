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
  - *Gaps & Risks*: High Risk. Uses synthetic micro-payloads (`"test_sdp"`,
    single fake candidate, `{1, 2, 3}`). Lacks real multi-m-line SDPs,
    IPv6/STUN/TURN candidates, and SPAKE2 tokens.
- **`jingle_message_xml_converter_unittest.cc`**:
  - *Scope*: `JingleMessage` <-> XML `XmlElement`
  - *Gaps & Risks*: Tests XML in total isolation from Protobuf. Does not test
    cross-format translation fidelity (`XML` -> `Model` -> `Proto` or
    `Proto` -> `Model` -> `XML`).
- **`jingle_message_struct_converter_unittest.cc`**:
  - *Scope*: `JingleMessage` <-> `internal::IqStanzaStruct`
  - *Gaps & Risks*: Tests internal C++ memory representations; does not
    validate wire serialization parity against TypeScript or Kotlin parsers.
- **`ftl_signal_strategy_unittest.cc`**:
  - *Scope*: Format negotiation (`BOTH`, `PROTOBUF`, `XML`)
  - *Gaps & Risks*: Uses mocked callbacks; does not test full real-world
    multi-round handshakes or concurrent multi-platform traffic.
- **`jingle_session_unittest.cc`**:
  - *Scope*: `JingleSession` state machine
  - *Gaps & Risks*: Critical Gap. Uses `FakeSignalStrategy` with in-memory
    structs and `FakeAuthenticator`; completely bypasses wire serialization
    and Protobuf encoding.

### 2.2 Exhaustive Gap Breakdown

1. **Payload Complexity Gaps**:
   - **SDP Descriptions**: Real WebRTC SDP contains bundle groups
     (`a=group:BUNDLE 0 1 2`), multiple media m-lines (`audio`, `video`,
     `application`), DTLS certificate fingerprints
     (`a=fingerprint:sha-256 ...`), ICE credentials (`a=ice-ufrag`,
     `a=ice-pwd`), and RTP header extensions.
   - **ICE Candidates**: Real sets include multiple transport protocols (UDP,
     TCP), candidate types (`host`, `srflx`, `relay`), and IPv4 / IPv6
     addresses with various priority levels and generation counters.
   - **Authentication Data**: Multi-round SPAKE2 Curve25519 public points (32
     bytes), SHA256 verification hashes (32 bytes), and SessionAuthz bearer
     tokens (both raw byte arrays and base64 strings).
   - **Attachment Configurations**: Real host attributes (`Debug-Build`,
     `HWEncoder`, `SupportsIceDatagramTransport`, `CapturesAudio`,
     `SupportsDirectPeer`) and configuration key-value pairs
     (`Av1-Encoder-Speed: 11`, `VideoCodecPreset: high_quality`,
     `EnableMultimon: true`).
2. **Binary vs Base64 Encoding Mismatches**:
   - XML serializes binary data as Base64 strings. Protobuf defines these
     fields as `bytes`.
   - Differences in Base64 padding, URL-safe vs standard Base64 encoding, or
     token string wrappers must be verified across C++, TypeScript, and Kotlin
     against Chromium's parser heuristics.
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
  - XML: `<host-token>` (Base64 string)
  - Proto: `authentication.session_authz_host_token` (raw `bytes`)
- **Session Token**:
  - XML: `<session-token>` (Base64 string)
  - Proto: `authentication.session_authz_session_token` (raw `bytes`)
- **Pairing Info Client ID**:
  - XML: `<pairing-info client-id="...">`
  - Proto: `authentication.pairing_info.client_id` (`string`)
- **Pairing Error**:
  - XML: `<pairing-failed error="...">` (nested inside `<authentication>` or
    as a direct child of `<jingle>`)
  - Model: `authentication.pairing_error` (string)
- **SDP Offer / Answer Encapsulation**:
  - XML: `<transport><session-description type="offer|answer">` embedded
    within `<jingle action="session-initiate|session-accept|transport-info">`
  - Proto: Dedicated `transport_info.session_description.sdp` + `type` enum
    sent in a `transport_info` Jingle action (or standalone message payload)
- **SDP Signature**:
  - XML: `<session-description signature="...">` (Base64 HMAC)
  - Proto: `transport_info.session_description.signature` (raw `bytes`)
- **ICE Candidate Line**:
  - XML: `<candidate>candidate:...</candidate>`
  - Proto: `transport_info.candidates[i].candidate` (string)
- **ICE Candidate Mid**:
  - XML: `<candidate sdpMid="...">`
  - Proto: `transport_info.candidates[i].sdp_mid` (string)
- **ICE Candidate Index**:
  - XML: `<candidate sdpMLineIndex="...">`
  - Proto: `transport_info.candidates[i].sdp_m_line_index` (`int32`)
- **Host Attributes**:
  - XML: `<attachments><host-attributes><attribute>...`
  - Proto: `repeated Attachment.host_attributes.attribute` (string)
- **Host Config Map**:
  - XML: `<attachments><host-configuration key="value" .../>`
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
> Chromium C++ emits and parses error condition tags in the default
> `jabber:client` namespace (inherited from `<iq xmlns="jabber:client">`).
> While RFC 6120 §8.3.2 specifies `urn:ietf:params:xml:ns:xmpp-stanzas`,
> Chromium matches condition tags within `jabber:client`. Ingress XML parsers
> should match condition child tags namespace-agnostically or accept both
> namespaces. This mapping is intentional in Chromium's codebase
> (`jingle_message_xml_converter.cc` and `jingle_message_proto_converter.cc`).
> External implementations MUST NOT alter or invert these mappings.

### 3.3 Signaling Format Negotiation & Dual-Payload Rules

Chrome Remote Desktop supports both legacy XML stanzas and structured Protobuf
(`ftl.IqStanza`) payloads over FTL signaling. To maintain seamless
bidirectional compatibility between native Chromium hosts/clients, the Web
Client, and Android implementations:

1. **Client Initiation Version Thresholds**:
   - **Host Version <= 154 (or unversioned / Android-to-Android peers)**: The
     client must send **XML-only** `session-initiate` (`stanza` populated,
     `iq_stanza` omitted).
   - **Host Version >= 155**: The client sends **Dual-Payload** (`BOTH`)
     `session-initiate` (encapsulating both `stanza` XML and `iq_stanza`
     Protobuf).

2. **Dual-Payload Mode (`BOTH`) Scope & Negotiation**:
   - **Strict Scope**: Dual-Payload (`BOTH`) is strictly restricted to the
     initial `session-initiate` message. Once the session is accepted or
     locked, all subsequent messages in that session MUST be single-payload
     (strictly Protobuf-only if negotiated, or XML-only if fallback occurred).
   - Outbound `session-initiate` messages generated by modern clients targeting
     hosts >= 155 contain both the serialized XML stanza string
     (`ftl.ChromotingXmppMessage.stanza`) and the structured Protobuf object
     (`ftl.ChromotingXmppMessage.iq_stanza`).
   - When a Protobuf-capable host receives a dual-payload `session-initiate`, it
     responds with a Protobuf-only `session-accept` (`iq_stanza` present,
     `stanza` omitted).
   - Receiving a Protobuf-only response locks the session into `PROTOBUF` mode
     for all subsequent requests and replies (`session-info`, `transport-info`,
     etc.).
   - If the host responds with XML or dual-payload, the session falls back to
     XML for the lifetime of that session.

3. **Protobuf-Only Negotiation (`PROTOBUF`)**:
   - When `PROTOBUF`-only mode is established for a session, all subsequent
     requests and replies (`JingleMessage` and `JingleMessageReply`) for that
     `sid` must use Protobuf-only without XML serialization overhead.

4. **XML-Only Mode (`XML`)**:
   - If an incoming message contains only an XML stanza (`stanza` present
     without `iq_stanza`), peers must communicate using XML-only for the
     lifetime of that session.

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
   - Base64 string wrapping is an XML-only artifact; Protobuf builders must
     never apply Base64 encoding to `bytes` fields.
   - For backwards compatibility during C++ in-memory conversion
     (`JingleAuthentication`), deserializers may tolerate legacy Base64
     inputs, but all canonical Protobuf wire emitters must output raw byte
     buffers.

2. **Normalized Addressing (`resource_part` Raw UUID & Boundary)**:
   - `JabberId.resource_part` must contain strictly the raw FTL registration
     UUID (e.g., `11111111-1111-1111-1111-111111111111`) with no
     `chromoting_ftl_` prefix.
   - `JabberId.local_part` and `JabberId.domain_part` must hold the separated
     username and domain strings.
   - Signaling adapters are the explicit normalization boundary: they must
     strip prefixes on egress when creating Protobuf `JabberId` and handle
     prefix addition/stripping on ingress when constructing `SignalingAddress`.

3. **Transport Decoupling & SDP Line Ending Normalization**:
   - `SessionInitiate` and `SessionAccept` actions are dedicated strictly to
     session establishment and authentication payloads.
   - WebRTC session descriptions (`SessionDescription`: SDP string with CRLF
     `\r\n` line endings and optional HMAC signature) and candidate lists
     (`IceCandidate`: `candidate`, `sdp_mid`, `sdp_m_line_index`) must be
     encapsulated within `TransportInfo` actions.
   - **SDP Line Ending Normalization**: Per W3C XML §2.11, XML parsers
     normalize line endings (`\r\n` and `\r`) to LF (`\n`) upon parsing
     `<session-description>`. Implementations ingesting XML MUST normalize `\n`
     to `\r\n` before populating `SessionDescription.sdp` or passing to WebRTC
     parsers (`webrtc::SdpDeserialize` / `webrtc::CreateSessionDescription`),
     while signature verification continues to normalize with `\n`.

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
   - Obsolete XML elements (`<pairing-failed>` in auth bodies or directly under
     `<jingle>`, `<custom-tag>` generic session-info payloads) and test-only
     fields (`test_id`, `test_key`) are excluded from the wire Protobuf schema.
     Ingress XML parsers should accept `<pairing-failed>` both inside
     `<authentication>` and directly under `<jingle>` for maximum legacy
     robustness.

---

## 4. Canonical Test Scenarios & Golden Vectors

> **Fixture Stanza ID Convention**: Cross-platform test suites (Chromium C++,
> Web TypeScript, Android Kotlin) must use the standardized stanza ID
> constants defined across these scenarios:
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
      <host-configuration
          Av1-Encoder-Speed="11"
          EnableMultimon="true"
          VideoCodecPreset="high_quality" />
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

When a paired client initiates a session with a `client-id` that has expired,
been revoked, or is unrecognized by the host, the host issues a `session-info`
stanza signaling `<pairing-failed>` to trigger a fallback to manual PIN entry.

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
      <pairing-failed error="invalid-client-id" />
    </authentication>
  </jingle>
</iq>
```

#### Protobuf Wire Note
> **Protobuf Architecture Note**: Per Section 3.4 Invariant 6,
> `<pairing-failed>` is an obsolete XML-only construct and is intentionally
> omitted from the wire Protobuf schema (`ftl::Authentication`). For legacy
> XML compatibility, Chromium C++ ingests `<pairing-failed error="...">` into
> `JingleAuthentication::pairing_error` and serializes it to XML. In Protobuf
> signaling, PIN authentication fallback is renegotiated directly over
> `session_info` authentication messages.

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
          <session-token>
            c2Vzc2lvbl9hdXRoel90b2tlbl9leGFtcGxlXzA5ODc2NTQzMjE=
          </session-token>
          <spake-message>
            AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA=
          </spake-message>
        </authentication>
      </description>
      <transport xmlns="google:remoting:webrtc" />
    </content>
    <attachments xmlns="google:remoting">
      <host-attributes>
        <attribute>Debug-Build</attribute>
        <attribute>HWEncoder</attribute>
        <attribute>SupportsIceDatagramTransport</attribute>
      </host-attributes>
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
          <certificate>
            MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyN3rL6u+eG8H9o3F
            7w4f1mK2s8u1v9X0y5z6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6
            c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6
            c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6
            c7d8e9f0a1b2c3d4e5f6a7b8c9IDAQAB
          </certificate>
          <spake-message>
            AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyA=
          </spake-message>
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
            signature="c2lnbmF0dXJlX2htYWNfZXhhbXBsZV8xMjM0NTY3ODkw">
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
      signature: "signature_hmac_example_1234567890"
    }
  }
}
```

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

#### B. Error Disconnect (`MAX_SESSION_DURATION_REACHED`)
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

- **Early ICE Candidates**:
  - *Sequence*: `transport-info` (candidates) arrives before `session-accept`.
  - *Handling*: Session buffers candidate list in `pending_candidates_` and
    applies them immediately once `session-accept` SDP answer is set on the
    peer connection.
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
      `iq_stanza` field is present.
- [ ] For `session_authz_host_token` and `session_authz_session_token`, encode
      strings into raw byte buffers matching Protobuf schema.
- [ ] For `IceCandidate` objects, always populate `sdp_mid`,
      `sdp_m_line_index`, and full candidate line string.
- [ ] Ensure client enforces `MIN_HOST_VERSION_FOR_PROTOBUF = 155` (sending
      XML-only for hosts <= 154 and unversioned peers; dual payload for
      hosts >= 155).
- [ ] Include Scenarios 1-8 in web client unit test suite.

### 6.2 Android Parity Checklist
- [ ] In Kotlin/Java proto builders, ensure `JabberId` resource part contains
      raw UUID (no prefix).
- [ ] Validate that SPAKE2 public point byte arrays are passed directly as
      `bytes` fields rather than base64 strings.
- [ ] Ensure host configuration settings map keys match exact
      PascalCase/kebab-case strings (`Av1-Encoder-Speed`, `VideoCodecPreset`,
      `EnableMultimon`).
- [ ] Ensure client enforces `MIN_HOST_VERSION_FOR_PROTOBUF = 155` (sending
      XML-only for hosts <= 154 and unversioned peers; dual payload for
      hosts >= 155).
- [ ] Ensure `pairing_info.client_id` is mapped between XML
      `<pairing-info client-id="...">` and Protobuf
      `authentication.pairing_info.client_id`.
- [ ] Include Scenarios 1-8 in Android unit test suite.

### 6.3 Chromium Unit Test Plan
- [ ] Implement `remoting/signaling/jingle_compatibility_unittest.cc`
      validating roundtrip invariance of Scenarios 1-8.
- [ ] Ensure all tests pass on Chromium Linux, Windows, Mac, and Android test
      targets before branch cut.

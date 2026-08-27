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
> raw UUID. External platforms must output the raw UUID in `resource_part`.

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
  - Proto: *Omitted from `ftl.Authentication`* (uses
    `AUTHENTICATION_METHOD_PAIRED_SPAKE2_CURVE25519` enum)
- **Pairing Error**:
  - XML: `<pairing-failed error="...">`
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
  - Proto: `ftl.IqStanza.reply = JingleReply {}`
- **IQ Error Reply**:
  - XML: `<iq type='error'><error type='...'>...</error></iq>`
  - Proto: `ftl.IqStanza.error = ErrorStanza` (`Condition` enum + `text`)

### 3.3 Signaling Format Negotiation & Dual-Payload Rules

Chrome Remote Desktop supports both legacy XML stanzas and structured Protobuf
(`ftl.IqStanza`) payloads over FTL signaling. To maintain seamless
bidirectional compatibility between native Chromium hosts/clients, the Web
Client, and Android implementations:

1. **Dual-Payload Mode (`BOTH`)**:
   - Outbound `session-initiate` messages generated by clients supporting modern
     signaling encapsulate both the serialized XML stanza string
     (`ftl.ChromotingXmppMessage.stanza`) and the structured Protobuf object
     (`ftl.ChromotingXmppMessage.iq_stanza`).
   - If an incoming message contains both XML stanza and Protobuf
     (`SignalingFormat::BOTH`), peers requiring XML compatibility must respond
     with XML (or dual payload) and ensure the `to` JID attribute
     (`to="user@example.com/chromoting_ftl_<registration_id>"`) is correctly
     populated in outgoing XML.
   - For active sessions negotiated with `BOTH`, intermediate messages
     (`session-info`, `transport-info`, etc.) continue using dual payload or the
     peer's negotiated format.

2. **Protobuf-Only Negotiation (`PROTOBUF`)**:
   - Protobuf signaling is negotiated **only** when the initiating peer sends
     Protobuf without XML fallback (i.e. `iq_stanza` is present and `stanza` is
     omitted).
   - When `PROTOBUF`-only mode is established for a session, all subsequent
     requests and replies (`JingleMessage` and `JingleMessageReply`) for that
     `sid` must use Protobuf-only without XML serialization overhead.

3. **XML-Only Mode (`XML`)**:
   - If an incoming message contains only an XML stanza (`stanza` present
     without `iq_stanza`), peers must communicate using XML-only for the
     lifetime of that session.

4. **Address Resolution & JID Routing**:
   - All outgoing XML stanzas must explicitly set the `to` attribute with the
     recipient's full JID including the `/chromoting_ftl_<registration_id>`
     resource part.
   - Incoming Protobuf `sender` and `receiver` `JabberId` fields must be mapped
     to valid `SignalingAddress` instances, stripping the `chromoting_ftl_`
     prefix when constructing internal registration identifiers.

---

## 4. Canonical Test Scenarios & Golden Vectors

### Scenario 1: `session-initiate` with Corp SessionAuthz & SDP Offer

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
      <transport xmlns="google:remoting:webrtc">
        <session-description type="offer">
v=0
o=- 4123456789012345 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1 2
a=msid-semantic: WMS
m=audio 9 UDP/TLS/RTP/SAVPF 111
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:UFrG
a=ice-pwd:PassWord1234567890ABCDEF
a=fingerprint:sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF
a=setup:actpass
a=mid:0
a=sendrecv
a=rtcp-mux
a=rtpmap:111 opus/48000/2
m=video 9 UDP/TLS/RTP/SAVPF 96
c=IN IP4 0.0.0.0
a=ice-ufrag:UFrG
a=ice-pwd:PassWord1234567890ABCDEF
a=fingerprint:sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF
a=setup:actpass
a=mid:1
a=rtpmap:96 VP9/90000
        </session-description>
      </transport>
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
    }
  }
}
```

---

### Scenario 3A: `session-accept` with Corp SessionAuthz & SDP Answer

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
      <transport xmlns="google:remoting:webrtc">
        <session-description type="answer">
v=0
o=- 8987654321098765 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1 2
m=audio 9 UDP/TLS/RTP/SAVPF 111
c=IN IP4 0.0.0.0
a=ice-ufrag:HostUfrag
a=ice-pwd:HostPassword9876543210ZYXWVU
a=fingerprint:sha-256 FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00
a=setup:active
a=mid:0
a=rtpmap:111 opus/48000/2
m=video 9 UDP/TLS/RTP/SAVPF 96
c=IN IP4 0.0.0.0
a=ice-ufrag:HostUfrag
a=ice-pwd:HostPassword9876543210ZYXWVU
a=fingerprint:sha-256 FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00
a=setup:active
a=mid:1
a=rtpmap:96 VP9/90000
        </session-description>
      </transport>
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
        <candidate sdpMid="0" sdpMLineIndex="0">
          candidate:1003 1 tcp 1518280447 192.168.1.150 9 typ host
          tcptype active generation 0
        </candidate>
        <candidate sdpMid="0" sdpMLineIndex="0">
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
      sdp_mid: "0"
      sdp_m_line_index: 0
    }
    candidates {
      candidate:
          "candidate:2001 1 udp 1686052607 74.125.250.1 54321 typ srflx"
          " raddr 192.168.1.150 rport 54321 generation 0"
      sdp_mid: "0"
      sdp_m_line_index: 0
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

---

### Scenario 6: `transport-info` for ICE Restart

#### XML (Chromium Native Output)
```xml
<iq
    to="host@example.com/chromoting_ftl_55555555-5555-5555-5555-555555555555"
    from="user@example.com/chromoting_ftl_11111111-1111-1111-1111-111111111111"
    type="set"
    id="msg_restart_006"
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
id: "msg_restart_006"
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

- **`BAD_REQUEST`**:
  - XML: `<error type='modify'><bad-request/></error>`
  - Proto: `ErrorStanza.Condition.BAD_REQUEST` (1)
- **`NOT_IMPLEMENTED`**:
  - XML: `<error type='cancel'><feature-bad-request/></error>`
  - Proto: `ErrorStanza.Condition.NOT_IMPLEMENTED` (2)
- **`INVALID_SID`**:
  - XML:
    `<error type='modify'><item-not-found/><text>Invalid SID</text></error>`
  - Proto: `ErrorStanza.Condition.INVALID_SID` (3)
- **`UNEXPECTED_REQUEST`**:
  - XML: `<error type='modify'><unexpected-request/></error>`
  - Proto: `ErrorStanza.Condition.UNEXPECTED_REQUEST` (4)
- **`UNSUPPORTED_INFO`**:
  - XML: `<error type='modify'><feature-not-implemented/></error>`
  - Proto: `ErrorStanza.Condition.UNSUPPORTED_INFO` (5)
- **`UNSPECIFIED`**:
  - XML: `<error type='cancel'><unspecified-error/></error>`
  - Proto: `ErrorStanza.Condition.CONDITION_UNSPECIFIED` (0)

---

## 5. Complete Signaling State Transitions

### 5.1 Connection State Machine Matrix

```text
  +------------------+
  |   INITIALIZING   |
  +--------+---------+
           | Send session-initiate (SDP Offer)
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
           | WebRTC Connected (Data Channels open)                 |
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
- [ ] Include Scenarios 1-8 in web client unit test suite.

### 6.2 Android Parity Checklist
- [ ] In Kotlin/Java proto builders, ensure `JabberId` resource part contains
      raw UUID (no prefix).
- [ ] Validate that SPAKE2 public point byte arrays are passed directly as
      `bytes` fields rather than base64 strings.
- [ ] Ensure host configuration settings map keys match exact
      PascalCase/kebab-case strings (`Av1-Encoder-Speed`, `VideoCodecPreset`,
      `EnableMultimon`).
- [ ] Include Scenarios 1-8 in Android unit test suite.

### 6.3 Chromium Unit Test Plan
- [ ] Implement `remoting/signaling/jingle_compatibility_unittest.cc`
      validating roundtrip invariance of Scenarios 1-8.
- [ ] Ensure all tests pass on Chromium Linux, Windows, Mac, and Android test
      targets before branch cut.

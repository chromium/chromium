// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

chromium::import! {
    "//crypto:tpm";
}

const OBJECT_HANDLE: u32 = 0x81000001;
const SIGN_HANDLE: u32 = 0x81000002;
const QUALIFYING_DATA: &[u8] = &[1, 2, 3, 4];
const WRONG_CHALLENGE: &[u8] = &[5, 6, 7, 8];

struct ResponseBuilder {
    tag: u16,
    rc: u32,
    magic: u32,
    type_: u16,
    qualified_signer: Vec<u8>,
    extra_data: Vec<u8>,
    algorithms: tpm::SignatureAlgorithms,
    sig: Vec<u8>,
}

#[allow(dead_code)]
impl ResponseBuilder {
    fn new() -> Self {
        Self {
            tag: tpm::TPM_ST_NO_SESSIONS,
            rc: 0,
            magic: tpm::TPM_GENERATED_VALUE,
            type_: tpm::TPM_ST_ATTEST_CERTIFY,
            qualified_signer: Vec::new(),
            extra_data: Vec::new(),
            algorithms: tpm::SignatureAlgorithms {
                sig_alg: tpm::ffi::TpmAlg::TPM_ALG_RSASSA.repr,
                hash_alg: tpm::ffi::TpmAlg::TPM_ALG_SHA256.repr,
            },
            sig: Vec::new(),
        }
    }

    fn with_tag(mut self, tag: u16) -> Self {
        self.tag = tag;
        self
    }

    fn with_rc(mut self, rc: u32) -> Self {
        self.rc = rc;
        self
    }

    fn with_magic(mut self, magic: u32) -> Self {
        self.magic = magic;
        self
    }

    fn with_type(mut self, type_: u16) -> Self {
        self.type_ = type_;
        self
    }

    fn with_qualified_signer(mut self, qualified_signer: &[u8]) -> Self {
        self.qualified_signer = qualified_signer.to_vec();
        self
    }

    fn with_extra_data(mut self, extra_data: &[u8]) -> Self {
        self.extra_data = extra_data.to_vec();
        self
    }

    fn with_sig_alg(mut self, sig_alg: u16) -> Self {
        self.algorithms.sig_alg = sig_alg;
        self
    }

    fn with_hash_alg(mut self, hash_alg: u16) -> Self {
        self.algorithms.hash_alg = hash_alg;
        self
    }

    fn with_sig(mut self, sig: &[u8]) -> Self {
        self.sig = sig.to_vec();
        self
    }

    fn build(self) -> Vec<u8> {
        let mut attest_payload_size: u16 = 4 // Magic
            + 2 // Type
            + 2 // Name size
            + u16::try_from(self.qualified_signer.len()).unwrap()
            + 2 // Data size
            + u16::try_from(self.extra_data.len()).unwrap()
            + 17 // clockInfo
            + 8; // firmwareVersion

        if self.type_ == tpm::TPM_ST_ATTEST_CERTIFY {
            attest_payload_size += 2 + 2; // name and qualifiedName
        }

        let attest_size: u16 = 2 // Attest size field
            + attest_payload_size;

        let mut signature_size: u16 = 2 // sigAlg
            + 2 // hashAlg
            + u16::try_from(self.sig.len()).unwrap();
        if self.algorithms.sig_alg == tpm::ffi::TpmAlg::TPM_ALG_RSASSA.repr {
            signature_size += 2; // sig size field
        }

        let mut total_size: u32 = 10; // Header
        if self.rc == 0 {
            total_size += u32::from(attest_size) + u32::from(signature_size);
        }

        let mut writer = tpm::Writer::with_capacity(usize::try_from(total_size).unwrap());
        writer.write_u16(self.tag);
        writer.write_u32(total_size);
        writer.write_u32(self.rc);

        if self.rc == 0 {
            writer.write_u16(attest_payload_size);
            writer.write_u32(self.magic);
            writer.write_u16(self.type_);

            writer.write_tpm2b(&self.qualified_signer);

            writer.write_tpm2b(&self.extra_data);

            writer.write_bytes(&[0; 17]); // clockInfo
            writer.write_bytes(&[0; 8]); // firmwareVersion

            if self.type_ == tpm::TPM_ST_ATTEST_CERTIFY {
                writer.write_u16(0); // name
                writer.write_u16(0); // qualified_name
            }

            // Signature
            writer.write_u16(self.algorithms.sig_alg);
            writer.write_u16(self.algorithms.hash_alg);
            if self.algorithms.sig_alg == tpm::ffi::TpmAlg::TPM_ALG_RSASSA.repr {
                writer.write_tpm2b(&self.sig);
            } else {
                writer.write_bytes(&self.sig);
            }
        }

        writer.into_inner()
    }
}

#[gtest(TpmTest, BuildCertifyCommandNullScheme)]
fn test_build_certify_command_null_scheme() {
    let cmd = tpm::build_certify_command(OBJECT_HANDLE, SIGN_HANDLE, QUALIFYING_DATA);
    expect_eq!(cmd.len(), 48);

    let mut reader = tpm::Reader::new(&cmd);

    expect_eq!(reader.read_u16().unwrap(), tpm::TPM_ST_SESSIONS);
    expect_eq!(reader.read_u32().unwrap(), 48); // commandSize
    expect_eq!(reader.read_u32().unwrap(), tpm::TPM_CC_CERTIFY);

    // Handles
    expect_eq!(reader.read_u32().unwrap(), OBJECT_HANDLE);
    expect_eq!(reader.read_u32().unwrap(), SIGN_HANDLE);

    // Auth size
    expect_eq!(reader.read_u32().unwrap(), 18);

    // Auth sessions
    expect_eq!(reader.read_u32().unwrap(), tpm::TPM_RS_PW);
    expect_eq!(reader.read_u16().unwrap(), 0); // nonce size
    expect_eq!(reader.read_u8().unwrap(), 0); // sessionAttributes
    expect_eq!(reader.read_u16().unwrap(), 0); // hmac size

    expect_eq!(reader.read_u32().unwrap(), tpm::TPM_RS_PW);
    expect_eq!(reader.read_u16().unwrap(), 0); // nonce size
    expect_eq!(reader.read_u8().unwrap(), 0); // sessionAttributes
    expect_eq!(reader.read_u16().unwrap(), 0); // hmac size

    // Qualifying data
    expect_eq!(reader.read_u16().unwrap(), u16::try_from(QUALIFYING_DATA.len()).unwrap());
    expect_eq!(reader.read_bytes(QUALIFYING_DATA.len()).unwrap(), QUALIFYING_DATA);

    // Scheme
    expect_eq!(reader.read_u16().unwrap(), tpm::ffi::TpmAlg::TPM_ALG_NULL.repr);
}

#[gtest(TpmParserTest, EmptyBuffer)]
fn test_empty_buffer() {
    let empty: &[u8] = &[];
    let challenge: &[u8] = &[];
    let result = tpm::parse_certify_response(empty, challenge);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::BufferTooSmall));
}

#[gtest(TpmParserTest, BadMagic)]
fn test_bad_magic() {
    let bad_magic = ResponseBuilder::new().with_magic(0xBAADBEEF).build();

    let challenge: &[u8] = &[];
    let result = tpm::parse_certify_response(&bad_magic, challenge);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::BadMagicNumber));
}

#[gtest(TpmParserTest, TpmErrorResponse)]
fn test_tpm_error_response() {
    let error_resp = ResponseBuilder::new().with_rc(0x100).build();

    let challenge: &[u8] = &[];
    let result = tpm::parse_certify_response(&error_resp, challenge);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::TpmErrorResponse));
    expect_eq!(result.tpm_response_code, 0x100);
}

#[gtest(TpmParserTest, WrongTag)]
fn test_wrong_tag() {
    let wrong_tag = ResponseBuilder::new().with_tag(0x8003).build();

    let challenge: &[u8] = &[];
    let result = tpm::parse_certify_response(&wrong_tag, challenge);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::WrongType));
}

#[gtest(TpmParserTest, WrongAttestType)]
fn test_wrong_attest_type() {
    let wrong_type = ResponseBuilder::new().with_type(0x8018).build();

    let challenge: &[u8] = &[];
    let result = tpm::parse_certify_response(&wrong_type, challenge);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::WrongType));
}

#[gtest(TpmParserTest, ChallengeMismatch)]
fn test_challenge_mismatch() {
    let challenge_mismatch = ResponseBuilder::new().with_extra_data(QUALIFYING_DATA).build();

    let challenge: &[u8] = WRONG_CHALLENGE;
    let result = tpm::parse_certify_response(&challenge_mismatch, challenge);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::ChallengeMismatch));
}

#[gtest(TpmParserTest, HappyPath)]
fn test_happy_path() {
    let expected_sig = &[0xAA, 0xBB, 0xCC, 0xDD];
    let happy_resp =
        ResponseBuilder::new().with_extra_data(QUALIFYING_DATA).with_sig(expected_sig).build();

    let result = tpm::parse_certify_response(&happy_resp, QUALIFYING_DATA);

    expect_true!(matches!(result.result, tpm::ffi::ParseResult::Ok));
    expect_eq!(result.tpm_response_code, 0);

    let expected_statement_bytes = &[
        0xFF, 0x54, 0x43, 0x47, // TPM_GENERATED_VALUE
        0x80, 0x17, // TPM_ST_ATTEST_CERTIFY
        0x00, 0x00, // qualified signer len
        0x00, 0x04, // extra data len
        0x01, 0x02, 0x03, 0x04, // extra data (QUALIFYING_DATA)
        // clockInfo (17 bytes)
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // firmwareVersion (8 bytes)
        0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x00, // name len
        0x00, 0x00, // qualified name len
    ];
    expect_eq!(result.statement, expected_statement_bytes);
    let expected_signature_bytes = &[
        0x00, 0x14, // TPM_ALG_RSASSA
        0x00, 0x0B, // TPM_ALG_SHA256
        0x00, 0x04, // signature size
        0xAA, 0xBB, 0xCC, 0xDD, // signature bytes
    ];
    expect_eq!(result.signature, expected_signature_bytes);
}

#[gtest(TpmParserTest, ParseRsaSignature)]
fn test_parse_rsa_signature() {
    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::ffi::TpmAlg::TPM_ALG_RSASSA.repr);
    writer.write_u16(tpm::ffi::TpmAlg::TPM_ALG_SHA256.repr);
    writer.write_tpm2b(b"rsa signature bytes");
    let signature = writer.into_inner();

    let parsed = tpm::parse_tpm_signature(&signature);
    expect_true!(matches!(parsed.status, tpm::ffi::SignatureParseResult::Ok));
    expect_eq!(parsed.sig_alg, tpm::ffi::TpmAlg::TPM_ALG_RSASSA);
    expect_eq!(parsed.hash_alg, tpm::ffi::TpmAlg::TPM_ALG_SHA256);
    expect_eq!(parsed.rsa_sig, b"rsa signature bytes");
    expect_true!(parsed.ecdsa_r.is_empty());
    expect_true!(parsed.ecdsa_s.is_empty());
}

#[gtest(TpmParserTest, ParseEcdsaSignature)]
fn test_parse_ecdsa_signature() {
    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::ffi::TpmAlg::TPM_ALG_ECDSA.repr);
    writer.write_u16(tpm::ffi::TpmAlg::TPM_ALG_SHA256.repr);
    writer.write_tpm2b(b"r coordinate");
    writer.write_tpm2b(b"s coordinate");
    let signature = writer.into_inner();

    let parsed = tpm::parse_tpm_signature(&signature);
    expect_true!(matches!(parsed.status, tpm::ffi::SignatureParseResult::Ok));
    expect_eq!(parsed.sig_alg, tpm::ffi::TpmAlg::TPM_ALG_ECDSA);
    expect_eq!(parsed.hash_alg, tpm::ffi::TpmAlg::TPM_ALG_SHA256);
    expect_true!(parsed.rsa_sig.is_empty());
    expect_eq!(parsed.ecdsa_r, b"r coordinate");
    expect_eq!(parsed.ecdsa_s, b"s coordinate");
}

#[gtest(TpmParserTest, ParseInvalidSignatureAlgorithm)]
fn test_parse_invalid_signature_algorithm() {
    let mut writer = tpm::Writer::new();
    writer.write_u16(0x1234); // Invalid signature algorithm
    writer.write_u16(tpm::ffi::TpmAlg::TPM_ALG_SHA256.repr);
    writer.write_bytes(b"dummy");
    let signature = writer.into_inner();

    let parsed = tpm::parse_tpm_signature(&signature);
    expect_true!(matches!(
        parsed.status,
        tpm::ffi::SignatureParseResult::UnsupportedSignatureAlgorithm
    ));
}

#[gtest(TpmParserTest, ParseBufferTooSmall)]
fn test_parse_buffer_too_small() {
    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::ffi::TpmAlg::TPM_ALG_RSASSA.repr);
    let signature = writer.into_inner();

    let parsed = tpm::parse_tpm_signature(&signature);
    expect_true!(matches!(parsed.status, tpm::ffi::SignatureParseResult::BufferTooSmall));
}

#[gtest(TpmParserTest, ParseTrailingBytes)]
fn test_parse_trailing_bytes() {
    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::ffi::TpmAlg::TPM_ALG_RSASSA.repr);
    writer.write_u16(tpm::ffi::TpmAlg::TPM_ALG_SHA256.repr);
    writer.write_tpm2b(b"rsa signature bytes");
    writer.write_bytes(b"extra garbage");
    let signature = writer.into_inner();

    let parsed = tpm::parse_tpm_signature(&signature);
    expect_true!(matches!(parsed.status, tpm::ffi::SignatureParseResult::TrailingBytes));
}

#[gtest(TpmTest, BuildHashCommand)]
fn test_build_hash_command() {
    let data = &[1, 2, 3, 4];
    let hash_alg = tpm::ffi::TpmAlg::TPM_ALG_SHA256.repr;
    // Note: TPM_RH_OWNER (0x40000001) is used for standard keys and mock validation
    // tickets in unit tests. By contrast, TPM_RH_ENDORSEMENT (0x4000000b) MUST be
    // used for Windows Attestation Identity Keys (AIKs) in production.
    let hierarchy = tpm::TPM_RH_OWNER;
    let cmd = tpm::build_hash_command(data, hash_alg, hierarchy);

    // Header size (10) + data size prefix (2) + data size (4) + hash_alg (2) +
    // hierarchy (4) = 22
    expect_eq!(cmd.len(), 22);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TPM_ST_NO_SESSIONS);
    expect_eq!(reader.read_u32().unwrap(), 22);
    expect_eq!(reader.read_u32().unwrap(), tpm::TPM_CC_HASH);

    expect_eq!(reader.read_u16().unwrap(), 4);
    expect_eq!(reader.read_bytes(4).unwrap(), data);
    expect_eq!(reader.read_u16().unwrap(), hash_alg);
    expect_eq!(reader.read_u32().unwrap(), hierarchy);
}

struct HashResponseBuilder {
    rc: u32,
    digest: Vec<u8>,
    ticket_tag: u16,
    ticket_hierarchy: u32,
    ticket_digest: Vec<u8>,
}

impl HashResponseBuilder {
    fn new() -> Self {
        Self {
            rc: 0,
            digest: vec![1, 2, 3],
            ticket_tag: tpm::TPM_ST_HASHCHECK,
            ticket_hierarchy: tpm::TPM_RH_OWNER,
            ticket_digest: vec![4, 5, 6],
        }
    }

    fn build(self) -> Vec<u8> {
        let digest_len = u16::try_from(self.digest.len()).unwrap();
        let ticket_digest_len = u16::try_from(self.ticket_digest.len()).unwrap();

        let ticket_size = 2 // tag
            + 4 // hierarchy
            + 2 // digest size
            + ticket_digest_len;

        let payload_size = 2 // digest size
            + digest_len
            + ticket_size;

        let total_size = 10 + payload_size;

        let mut writer = tpm::Writer::with_capacity(total_size.into());
        writer.write_u16(tpm::TPM_ST_NO_SESSIONS);
        writer.write_u32(total_size.into());
        writer.write_u32(self.rc);

        if self.rc == 0 {
            writer.write_tpm2b(&self.digest);
            writer.write_u16(self.ticket_tag);
            writer.write_u32(self.ticket_hierarchy);
            writer.write_tpm2b(&self.ticket_digest);
        }

        writer.into_inner()
    }
}

#[gtest(TpmParserTest, HashHappyPath)]
fn test_hash_happy_path() {
    let builder = HashResponseBuilder::new();
    let resp = builder.build();

    let result = tpm::parse_hash_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::Ok));
    expect_eq!(result.tpm_response_code, 0);
    expect_eq!(result.digest, &[1, 2, 3]);

    let expected_ticket = {
        let mut writer = tpm::Writer::new();
        writer.write_u16(tpm::TPM_ST_HASHCHECK);
        writer.write_u32(tpm::TPM_RH_OWNER);
        writer.write_tpm2b(&[4, 5, 6]);
        writer.into_inner()
    };
    expect_eq!(result.validation_ticket, expected_ticket);
}

#[gtest(TpmTest, BuildSignCommand)]
fn test_build_sign_command() {
    let key_handle = 0x81000001;
    let digest = &[1, 2, 3];
    let sig_alg = tpm::ffi::TpmAlg::TPM_ALG_ECDSA.repr;
    let hash_alg = tpm::ffi::TpmAlg::TPM_ALG_SHA256.repr;
    let validation_ticket = &[7, 8, 9, 10];
    let cmd = tpm::build_sign_command(key_handle, digest, sig_alg, hash_alg, validation_ticket);

    // Header (10) + Handle (4) + AuthSize (4) + Session (9) + digest prefix (2) +
    // digest len (3)
    // + sig_alg (2) + hash_alg (2) + ticket len (4) = 40
    expect_eq!(cmd.len(), 40);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TPM_ST_SESSIONS);
    expect_eq!(reader.read_u32().unwrap(), 40);
    expect_eq!(reader.read_u32().unwrap(), tpm::TPM_CC_SIGN);

    expect_eq!(reader.read_u32().unwrap(), key_handle);

    // Auth session
    expect_eq!(reader.read_u32().unwrap(), 9); // auth size
    expect_eq!(reader.read_u32().unwrap(), tpm::TPM_RS_PW);
    expect_eq!(reader.read_u16().unwrap(), 0);
    expect_eq!(reader.read_u8().unwrap(), 0);
    expect_eq!(reader.read_u16().unwrap(), 0);

    // Parameters
    expect_eq!(reader.read_u16().unwrap(), 3);
    expect_eq!(reader.read_bytes(3).unwrap(), digest);

    expect_eq!(reader.read_u16().unwrap(), sig_alg);
    expect_eq!(reader.read_u16().unwrap(), hash_alg);

    expect_eq!(reader.read_bytes(4).unwrap(), validation_ticket);
}

#[gtest(TpmTest, BuildSignCommandNullScheme)]
fn test_build_sign_command_null_scheme() {
    let key_handle = 0x81000001;
    let digest = &[1, 2, 3];
    let sig_alg = tpm::ffi::TpmAlg::TPM_ALG_NULL.repr;
    let hash_alg = tpm::ffi::TpmAlg::TPM_ALG_SHA256.repr;
    let validation_ticket = &[7, 8, 9, 10];
    let cmd = tpm::build_sign_command(key_handle, digest, sig_alg, hash_alg, validation_ticket);

    // Header (10) + Handle (4) + AuthSize (4) + Session (9) + digest prefix (2) +
    // digest len (3)
    // + sig_alg (2) + ticket len (4) = 38 (no hash_alg written)
    expect_eq!(cmd.len(), 38);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TPM_ST_SESSIONS);
    expect_eq!(reader.read_u32().unwrap(), 38);
    expect_eq!(reader.read_u32().unwrap(), tpm::TPM_CC_SIGN);

    expect_eq!(reader.read_u32().unwrap(), key_handle);

    // Auth session
    expect_eq!(reader.read_u32().unwrap(), 9); // auth size
    expect_eq!(reader.read_u32().unwrap(), tpm::TPM_RS_PW);
    expect_eq!(reader.read_u16().unwrap(), 0);
    expect_eq!(reader.read_u8().unwrap(), 0);
    expect_eq!(reader.read_u16().unwrap(), 0);

    // Parameters
    expect_eq!(reader.read_u16().unwrap(), 3);
    expect_eq!(reader.read_bytes(3).unwrap(), digest);

    expect_eq!(reader.read_u16().unwrap(), sig_alg); // TPM_ALG_NULL
                                                     // No hash_alg

    expect_eq!(reader.read_bytes(4).unwrap(), validation_ticket);
}

struct SignResponseBuilder {
    tag: u16,
    rc: u32,
    sig_alg: u16,
    hash_alg: u16,
    sig: Vec<u8>,
}

impl SignResponseBuilder {
    fn new() -> Self {
        Self {
            tag: tpm::TPM_ST_SESSIONS,
            rc: 0,
            sig_alg: tpm::ffi::TpmAlg::TPM_ALG_RSASSA.repr,
            hash_alg: tpm::ffi::TpmAlg::TPM_ALG_SHA256.repr,
            sig: vec![0xAA, 0xBB],
        }
    }

    fn build(self) -> Vec<u8> {
        let mut sig_size = 2 // sigAlg
            + 2 // hashAlg
            + u16::try_from(self.sig.len()).unwrap();
        if self.sig_alg == tpm::ffi::TpmAlg::TPM_ALG_RSASSA.repr {
            sig_size += 2; // size prefix
        }

        let mut total_size = 10;
        if self.rc == 0 {
            if self.tag == tpm::TPM_ST_SESSIONS {
                total_size += 4; // parameterSize field
            }
            total_size += u32::from(sig_size);
            if self.tag == tpm::TPM_ST_SESSIONS {
                total_size += 5; // Session size
            }
        }

        let mut writer = tpm::Writer::with_capacity(total_size.try_into().unwrap());
        writer.write_u16(self.tag);
        writer.write_u32(total_size);
        writer.write_u32(self.rc);

        if self.rc == 0 {
            if self.tag == tpm::TPM_ST_SESSIONS {
                writer.write_u32(sig_size.into());
            }

            writer.write_u16(self.sig_alg);
            writer.write_u16(self.hash_alg);
            if self.sig_alg == tpm::ffi::TpmAlg::TPM_ALG_RSASSA.repr {
                writer.write_tpm2b(&self.sig);
            } else {
                writer.write_bytes(&self.sig);
            }

            if self.tag == tpm::TPM_ST_SESSIONS {
                // Auth Response Session
                writer.write_u16(0); // nonce size
                writer.write_u8(0); // sessionAttributes
                writer.write_u16(0); // HMAC size (for pw it is 0)
            }
        }

        writer.into_inner()
    }
}

#[gtest(TpmParserTest, SignHappyPath)]
fn test_sign_happy_path() {
    let builder = SignResponseBuilder::new();
    let resp = builder.build();

    let result = tpm::parse_sign_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::Ok));
    expect_eq!(result.tpm_response_code, 0);

    let expected_sig_bytes = {
        let mut writer = tpm::Writer::new();
        writer.write_u16(tpm::ffi::TpmAlg::TPM_ALG_RSASSA.repr);
        writer.write_u16(tpm::ffi::TpmAlg::TPM_ALG_SHA256.repr);
        writer.write_tpm2b(&[0xAA, 0xBB]);
        writer.into_inner()
    };
    expect_eq!(result.signature, expected_sig_bytes);
}

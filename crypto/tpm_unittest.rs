// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

chromium::import! {
    "//crypto:tpm";
}

fn build_test_ticket(tag: u16, hierarchy: u32, digest: &[u8]) -> Vec<u8> {
    let mut writer = tpm::Writer::new();
    writer.write_u16(tag);
    writer.write_u32(hierarchy);
    writer.write_tpm2b(digest);
    writer.into_inner()
}

#[gtest(TpmParserTest, ParseRsaSignature)]
fn test_parse_rsa_signature() {
    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TpmAlgSigScheme::TPM_ALG_RSASSA.repr);
    writer.write_u16(tpm::TpmAlgHash::TPM_ALG_SHA256.repr);
    writer.write_tpm2b(b"rsa signature bytes");
    let signature = writer.into_inner();

    let parsed = tpm::parse_tpm_signature(&signature);
    expect_true!(matches!(parsed.status, tpm::ffi::SignatureParseResult::Ok));
    expect_eq!(parsed.sig_alg, tpm::TpmAlgSigScheme::TPM_ALG_RSASSA);
    expect_eq!(parsed.hash_alg, tpm::TpmAlgHash::TPM_ALG_SHA256);
    expect_eq!(parsed.rsa_sig, b"rsa signature bytes");
    expect_true!(parsed.ecdsa_r.is_empty());
    expect_true!(parsed.ecdsa_s.is_empty());
}

#[gtest(TpmParserTest, ParseEcdsaSignature)]
fn test_parse_ecdsa_signature() {
    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TpmAlgSigScheme::TPM_ALG_ECDSA.repr);
    writer.write_u16(tpm::TpmAlgHash::TPM_ALG_SHA256.repr);
    writer.write_tpm2b(b"r coordinate");
    writer.write_tpm2b(b"s coordinate");
    let signature = writer.into_inner();

    let parsed = tpm::parse_tpm_signature(&signature);
    expect_true!(matches!(parsed.status, tpm::ffi::SignatureParseResult::Ok));
    expect_eq!(parsed.sig_alg, tpm::TpmAlgSigScheme::TPM_ALG_ECDSA);
    expect_eq!(parsed.hash_alg, tpm::TpmAlgHash::TPM_ALG_SHA256);
    expect_true!(parsed.rsa_sig.is_empty());
    expect_eq!(parsed.ecdsa_r, b"r coordinate");
    expect_eq!(parsed.ecdsa_s, b"s coordinate");
}

#[gtest(TpmParserTest, ParseInvalidSignatureAlgorithm)]
fn test_parse_invalid_signature_algorithm() {
    let mut writer = tpm::Writer::new();
    writer.write_u16(0x1234); // Invalid signature algorithm
    writer.write_u16(tpm::TpmAlgHash::TPM_ALG_SHA256.repr);
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
    writer.write_u16(tpm::TpmAlgSigScheme::TPM_ALG_RSASSA.repr);
    let signature = writer.into_inner();

    let parsed = tpm::parse_tpm_signature(&signature);
    expect_true!(matches!(parsed.status, tpm::ffi::SignatureParseResult::BufferTooSmall));
}

#[gtest(TpmParserTest, ParseTrailingBytes)]
fn test_parse_trailing_bytes() {
    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TpmAlgSigScheme::TPM_ALG_RSASSA.repr);
    writer.write_u16(tpm::TpmAlgHash::TPM_ALG_SHA256.repr);
    writer.write_tpm2b(b"rsa signature bytes");
    writer.write_bytes(b"extra garbage");
    let signature = writer.into_inner();

    let parsed = tpm::parse_tpm_signature(&signature);
    expect_true!(matches!(parsed.status, tpm::ffi::SignatureParseResult::TrailingBytes));
}

#[gtest(TpmTest, BuildHashCommand)]
fn test_build_hash_command() {
    let data = &[1, 2, 3, 4];
    let hash_alg = tpm::TpmAlgHash::TPM_ALG_SHA256;
    let cmd = tpm::build_hash_command(data, hash_alg);

    // Header size (10) + data size prefix (2) + data size (4) + hash_alg (2) +
    // hierarchy (4) = 22
    expect_eq!(cmd.len(), 22);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_NO_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 22);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_HASH.repr);

    expect_eq!(reader.read_u16().unwrap(), 4);
    expect_eq!(reader.read_bytes(4).unwrap(), data);
    expect_eq!(reader.read_u16().unwrap(), hash_alg.repr);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmRh::TPM_RH_OWNER.repr);
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
            ticket_tag: tpm::TpmSt::TPM_ST_HASHCHECK.repr,
            ticket_hierarchy: tpm::TpmRh::TPM_RH_OWNER.repr,
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
        writer.write_u16(tpm::TpmSt::TPM_ST_NO_SESSIONS.repr);
        writer.write_u32(total_size.into());
        writer.write_u32(self.rc);

        if self.rc == 0 {
            writer.write_tpm2b(&self.digest);
            writer.write_bytes(&build_test_ticket(
                self.ticket_tag,
                self.ticket_hierarchy,
                &self.ticket_digest,
            ));
        }

        writer.into_inner()
    }
}

#[gtest(TpmParserTest, HashHappyPath)]
fn test_hash_happy_path() {
    let builder = HashResponseBuilder::new();
    let resp = builder.build();

    let result = tpm::parse_hash_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::Ok));
    expect_eq!(result.status.tpm_response_code, 0);
    expect_eq!(result.digest, &[1, 2, 3]);

    let expected_ticket = build_test_ticket(
        tpm::TpmSt::TPM_ST_HASHCHECK.repr,
        tpm::TpmRh::TPM_RH_OWNER.repr,
        &[4, 5, 6],
    );
    expect_eq!(result.validation_ticket, expected_ticket);
}

#[gtest(TpmTest, BuildSignCommand)]
fn test_build_sign_command() {
    let key_handle = 0x81000001;
    let digest = &[1, 2, 3];
    let validation_ticket = &[7, 8, 9, 10];
    let cmd = tpm::build_sign_command(key_handle, digest, validation_ticket);

    // Header (10) + Handle (4) + AuthSize (4) + Session (9) + digest prefix (2) +
    // digest len (3) + sig_alg TPM_ALG_NULL (2) + ticket len (4) = 38
    expect_eq!(cmd.len(), 38);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 38);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_SIGN.repr);

    expect_eq!(reader.read_u32().unwrap(), key_handle);

    // Auth session
    expect_eq!(reader.read_u32().unwrap(), 9); // auth size
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmRh::TPM_RS_PW.repr);
    expect_eq!(reader.read_u16().unwrap(), 0);
    expect_eq!(reader.read_u8().unwrap(), 0);
    expect_eq!(reader.read_u16().unwrap(), 0);

    // Parameters
    expect_eq!(reader.read_u16().unwrap(), 3);
    expect_eq!(reader.read_bytes(3).unwrap(), digest);

    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_NULL.repr);

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
            tag: tpm::TpmSt::TPM_ST_SESSIONS.repr,
            rc: 0,
            sig_alg: tpm::TpmAlgSigScheme::TPM_ALG_RSASSA.repr,
            hash_alg: tpm::TpmAlgHash::TPM_ALG_SHA256.repr,
            sig: vec![0xAA, 0xBB],
        }
    }

    fn build(self) -> Vec<u8> {
        let mut sig_size = 2 // sigAlg
            + 2 // hashAlg
            + u16::try_from(self.sig.len()).unwrap();
        if self.sig_alg == tpm::TpmAlgSigScheme::TPM_ALG_RSASSA.repr {
            sig_size += 2; // size prefix
        }

        let mut total_size = 10;
        if self.rc == 0 {
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                total_size += 4; // parameterSize field
            }
            total_size += u32::from(sig_size);
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                total_size += 5; // Session size
            }
        }

        let mut writer = tpm::Writer::with_capacity(total_size.try_into().unwrap());
        writer.write_u16(self.tag);
        writer.write_u32(total_size);
        writer.write_u32(self.rc);

        if self.rc == 0 {
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                writer.write_u32(sig_size.into());
            }

            writer.write_u16(self.sig_alg);
            writer.write_u16(self.hash_alg);
            if self.sig_alg == tpm::TpmAlgSigScheme::TPM_ALG_RSASSA.repr {
                writer.write_tpm2b(&self.sig);
            } else {
                writer.write_bytes(&self.sig);
            }

            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
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
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::Ok));
    expect_eq!(result.status.tpm_response_code, 0);

    let expected_sig_bytes = {
        let mut writer = tpm::Writer::new();
        writer.write_u16(tpm::TpmAlgSigScheme::TPM_ALG_RSASSA.repr);
        writer.write_u16(tpm::TpmAlgHash::TPM_ALG_SHA256.repr);
        writer.write_tpm2b(&[0xAA, 0xBB]);
        writer.into_inner()
    };
    expect_eq!(result.signature, expected_sig_bytes);
}

#[gtest(TpmTest, BuildHashSequenceStartCommand)]
fn test_build_hash_sequence_start_command() {
    let hash_alg = tpm::TpmAlgHash::TPM_ALG_SHA256;
    let cmd = tpm::build_hash_sequence_start_command(hash_alg);

    // Header (10) + auth (2) + hashAlg (2) = 14
    expect_eq!(cmd.len(), 14);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_NO_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 14);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_HASH_SEQUENCE_START.repr);

    expect_eq!(reader.read_u16().unwrap(), 0); // empty auth
    expect_eq!(reader.read_u16().unwrap(), hash_alg.repr);
}

struct HashSequenceStartResponseBuilder {
    tag: u16,
    rc: u32,
    sequence_handle: u32,
}

impl HashSequenceStartResponseBuilder {
    fn new() -> Self {
        Self { tag: tpm::TpmSt::TPM_ST_NO_SESSIONS.repr, rc: 0, sequence_handle: 0x80000001 }
    }

    fn set_tag(mut self, tag: u16) -> Self {
        self.tag = tag;
        self
    }

    fn set_rc(mut self, rc: u32) -> Self {
        self.rc = rc;
        self
    }

    fn build(self) -> Vec<u8> {
        let mut total_size = 10;
        if self.rc == 0 {
            total_size += 4; // sequenceHandle
        }

        let mut writer = tpm::Writer::with_capacity(total_size);
        writer.write_u16(self.tag);
        writer.write_u32(u32::try_from(total_size).unwrap());
        writer.write_u32(self.rc);

        if self.rc == 0 {
            writer.write_u32(self.sequence_handle);
        }

        writer.into_inner()
    }
}

#[gtest(TpmParserTest, HashSequenceStartHappyPath)]
fn test_hash_sequence_start_happy_path() {
    let builder = HashSequenceStartResponseBuilder::new();
    let resp = builder.build();

    let result = tpm::parse_hash_sequence_start_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::Ok));
    expect_eq!(result.status.tpm_response_code, 0);
    expect_eq!(result.sequence_handle, 0x80000001);
}

#[gtest(TpmParserTest, HashSequenceStartWrongTag)]
fn test_hash_sequence_start_wrong_tag() {
    let builder = HashSequenceStartResponseBuilder::new().set_tag(tpm::TpmSt::TPM_ST_SESSIONS.repr);
    let resp = builder.build();

    let result = tpm::parse_hash_sequence_start_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::WrongType));
}

#[gtest(TpmParserTest, HashSequenceStartBufferTooSmall)]
fn test_hash_sequence_start_buffer_too_small() {
    let builder = HashSequenceStartResponseBuilder::new();
    let mut resp = builder.build();
    resp.pop();

    let result = tpm::parse_hash_sequence_start_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::BufferTooSmall));
}

#[gtest(TpmParserTest, HashSequenceStartTrailingBytes)]
fn test_hash_sequence_start_trailing_bytes() {
    let builder = HashSequenceStartResponseBuilder::new();
    let mut resp = builder.build();
    resp.push(0);

    let result = tpm::parse_hash_sequence_start_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::TrailingBytes));
}

#[gtest(TpmParserTest, HashSequenceStartTpmError)]
fn test_hash_sequence_start_tpm_error() {
    let builder = HashSequenceStartResponseBuilder::new().set_rc(0x100);
    let resp = builder.build();

    let result = tpm::parse_hash_sequence_start_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::TpmErrorResponse));
    expect_eq!(result.status.tpm_response_code, 0x100);
    expect_eq!(result.sequence_handle, 0);
}

#[gtest(TpmTest, BuildSequenceUpdateCommand)]
fn test_build_sequence_update_command() {
    let sequence_handle = 0x80000001;
    let data = &[1, 2, 3, 4];
    let cmd = tpm::build_sequence_update_command(sequence_handle, data);

    // Header (10) + Handle (4) + AuthSize (4) + Session (9) + data prefix (2) +
    // data len (4) = 33
    expect_eq!(cmd.len(), 33);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 33);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_SEQUENCE_UPDATE.repr);

    expect_eq!(reader.read_u32().unwrap(), sequence_handle);

    // Auth session
    expect_eq!(reader.read_u32().unwrap(), 9); // auth size
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmRh::TPM_RS_PW.repr);
    expect_eq!(reader.read_u16().unwrap(), 0);
    expect_eq!(reader.read_u8().unwrap(), 0);
    expect_eq!(reader.read_u16().unwrap(), 0);

    expect_eq!(reader.read_tpm2b().unwrap(), data);
}

#[gtest(TpmTest, BuildSequenceUpdateCommandMaxBuffer)]
fn test_build_sequence_update_command_max_buffer() {
    let sequence_handle = 0x80000001;
    let data = vec![0xAA; tpm::TPM_MAX_BUFFER_SIZE];
    let cmd = tpm::build_sequence_update_command(sequence_handle, &data);

    // Header (10) + Handle (4) + AuthSize (4) + Session (9) + data prefix (2) +
    // 1024 = 1053
    expect_eq!(cmd.len(), 1053);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 1053);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_SEQUENCE_UPDATE.repr);
}

struct SequenceUpdateResponseBuilder {
    tag: u16,
    rc: u32,
    parameter_size: u32,
}

impl SequenceUpdateResponseBuilder {
    fn new() -> Self {
        Self { tag: tpm::TpmSt::TPM_ST_SESSIONS.repr, rc: 0, parameter_size: 0 }
    }

    fn set_tag(mut self, tag: u16) -> Self {
        self.tag = tag;
        self
    }

    fn set_rc(mut self, rc: u32) -> Self {
        self.rc = rc;
        self
    }

    fn set_parameter_size(mut self, size: u32) -> Self {
        self.parameter_size = size;
        self
    }

    fn build(self) -> Vec<u8> {
        let mut total_size = 10;
        if self.rc == 0 {
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                total_size += 4; // parameterSize field
            }
            total_size += self.parameter_size;
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                total_size += 5; // Session size
            }
        }

        let mut writer = tpm::Writer::with_capacity(total_size.try_into().unwrap());
        writer.write_u16(self.tag);
        writer.write_u32(total_size);
        writer.write_u32(self.rc);

        if self.rc == 0 {
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                writer.write_u32(self.parameter_size);
            }

            if self.parameter_size > 0 {
                writer.write_bytes(&vec![0; self.parameter_size as usize]);
            }

            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                // Auth Response Session
                writer.write_u16(0); // nonce size
                writer.write_u8(0); // sessionAttributes
                writer.write_u16(0); // HMAC size (for pw it is 0)
            }
        }

        writer.into_inner()
    }
}

#[gtest(TpmParserTest, SequenceUpdateHappyPath)]
fn test_sequence_update_happy_path() {
    let builder = SequenceUpdateResponseBuilder::new();
    let resp = builder.build();

    let result = tpm::parse_sequence_update_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::Ok));
    expect_eq!(result.tpm_response_code, 0);
}

#[gtest(TpmParserTest, SequenceUpdateWrongTag)]
fn test_sequence_update_wrong_tag() {
    let builder = SequenceUpdateResponseBuilder::new().set_tag(tpm::TpmSt::TPM_ST_NO_SESSIONS.repr);
    let resp = builder.build();

    let result = tpm::parse_sequence_update_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::WrongType));
}

#[gtest(TpmParserTest, SequenceUpdateNonZeroParameterSize)]
fn test_sequence_update_non_zero_parameter_size() {
    let builder = SequenceUpdateResponseBuilder::new().set_parameter_size(4);
    let resp = builder.build();

    let result = tpm::parse_sequence_update_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::TrailingBytes));
}

#[gtest(TpmParserTest, SequenceUpdateBufferTooSmall)]
fn test_sequence_update_buffer_too_small() {
    let builder = SequenceUpdateResponseBuilder::new();
    let mut resp = builder.build();
    resp.pop();

    let result = tpm::parse_sequence_update_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::BufferTooSmall));
}

#[gtest(TpmParserTest, SequenceUpdateTrailingBytes)]
fn test_sequence_update_trailing_bytes() {
    let builder = SequenceUpdateResponseBuilder::new();
    let mut resp = builder.build();
    resp.push(0);

    let result = tpm::parse_sequence_update_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::TrailingBytes));
}

#[gtest(TpmParserTest, SequenceUpdateTpmError)]
fn test_sequence_update_tpm_error() {
    let builder = SequenceUpdateResponseBuilder::new().set_rc(0x100);
    let resp = builder.build();

    let result = tpm::parse_sequence_update_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::TpmErrorResponse));
    expect_eq!(result.tpm_response_code, 0x100);
}

#[gtest(TpmTest, BuildSequenceCompleteCommand)]
fn test_build_sequence_complete_command() {
    let sequence_handle = 0x80000001;
    let data = &[1, 2, 3, 4];
    let cmd = tpm::build_sequence_complete_command(sequence_handle, data);

    // Header (10) + Handle (4) + AuthSize (4) + Session (9) + data prefix (2) +
    // data len (4) + hierarchy (4) = 37
    expect_eq!(cmd.len(), 37);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 37);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_SEQUENCE_COMPLETE.repr);

    expect_eq!(reader.read_u32().unwrap(), sequence_handle);

    // Auth session
    expect_eq!(reader.read_u32().unwrap(), 9); // auth size
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmRh::TPM_RS_PW.repr);
    expect_eq!(reader.read_u16().unwrap(), 0);
    expect_eq!(reader.read_u8().unwrap(), 0);
    expect_eq!(reader.read_u16().unwrap(), 0);

    expect_eq!(reader.read_tpm2b().unwrap(), data);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmRh::TPM_RH_OWNER.repr);
}

#[gtest(TpmTest, BuildSequenceCompleteCommandMaxBuffer)]
fn test_build_sequence_complete_command_max_buffer() {
    let sequence_handle = 0x80000001;
    let data = vec![0xAA; tpm::TPM_MAX_BUFFER_SIZE];
    let cmd = tpm::build_sequence_complete_command(sequence_handle, &data);

    // Header (10) + Handle (4) + AuthSize (4) + Session (9) + data prefix (2) +
    // 1024 + hierarchy (4) = 1057
    expect_eq!(cmd.len(), 1057);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 1057);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_SEQUENCE_COMPLETE.repr);
}

struct SequenceCompleteResponseBuilder {
    tag: u16,
    rc: u32,
    digest: Vec<u8>,
    ticket_tag: u16,
    ticket_hierarchy: u32,
    ticket_digest: Vec<u8>,
}

impl SequenceCompleteResponseBuilder {
    fn new() -> Self {
        Self {
            tag: tpm::TpmSt::TPM_ST_SESSIONS.repr,
            rc: 0,
            digest: vec![1, 2, 3],
            ticket_tag: tpm::TpmSt::TPM_ST_HASHCHECK.repr,
            ticket_hierarchy: tpm::TpmRh::TPM_RH_OWNER.repr,
            ticket_digest: vec![4, 5, 6],
        }
    }

    fn set_tag(mut self, tag: u16) -> Self {
        self.tag = tag;
        self
    }

    fn set_rc(mut self, rc: u32) -> Self {
        self.rc = rc;
        self
    }

    fn set_ticket_tag(mut self, tag: u16) -> Self {
        self.ticket_tag = tag;
        self
    }

    fn build(self) -> Vec<u8> {
        let ticket_size = 2 + 4 + 2 + self.ticket_digest.len();
        let body_size = 2 + self.digest.len() + ticket_size;

        let mut total_size = 10;
        if self.rc == 0 {
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                total_size += 4; // parameterSize field
            }
            total_size += u32::try_from(body_size).unwrap();
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                total_size += 5; // Session size
            }
        }

        let mut writer = tpm::Writer::with_capacity(total_size.try_into().unwrap());
        writer.write_u16(self.tag);
        writer.write_u32(total_size);
        writer.write_u32(self.rc);

        if self.rc == 0 {
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                writer.write_u32(u32::try_from(body_size).unwrap());
            }

            writer.write_tpm2b(&self.digest);
            writer.write_bytes(&build_test_ticket(
                self.ticket_tag,
                self.ticket_hierarchy,
                &self.ticket_digest,
            ));

            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                // Auth Response Session
                writer.write_u16(0); // nonce size
                writer.write_u8(0); // sessionAttributes
                writer.write_u16(0); // HMAC size (for pw it is 0)
            }
        }

        writer.into_inner()
    }
}

#[gtest(TpmParserTest, SequenceCompleteHappyPath)]
fn test_sequence_complete_happy_path() {
    let builder = SequenceCompleteResponseBuilder::new();
    let resp = builder.build();

    let result = tpm::parse_sequence_complete_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::Ok));
    expect_eq!(result.status.tpm_response_code, 0);
    expect_eq!(result.digest, &[1, 2, 3]);

    let expected_ticket = build_test_ticket(
        tpm::TpmSt::TPM_ST_HASHCHECK.repr,
        tpm::TpmRh::TPM_RH_OWNER.repr,
        &[4, 5, 6],
    );
    expect_eq!(result.validation_ticket, expected_ticket);
}

#[gtest(TpmParserTest, SequenceCompleteWrongTag)]
fn test_sequence_complete_wrong_tag() {
    let builder =
        SequenceCompleteResponseBuilder::new().set_tag(tpm::TpmSt::TPM_ST_NO_SESSIONS.repr);
    let resp = builder.build();

    let result = tpm::parse_sequence_complete_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::WrongType));
}

#[gtest(TpmParserTest, SequenceCompleteWrongTicketTag)]
fn test_sequence_complete_wrong_ticket_tag() {
    let builder = SequenceCompleteResponseBuilder::new()
        .set_ticket_tag(tpm::TpmSt::TPM_ST_ATTEST_CERTIFY.repr);
    let resp = builder.build();

    let result = tpm::parse_sequence_complete_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::WrongType));
}

#[gtest(TpmParserTest, SequenceCompleteBufferTooSmall)]
fn test_sequence_complete_buffer_too_small() {
    let builder = SequenceCompleteResponseBuilder::new();
    let mut resp = builder.build();
    resp.pop();

    let result = tpm::parse_sequence_complete_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::BufferTooSmall));
}

#[gtest(TpmParserTest, SequenceCompleteTrailingBytes)]
fn test_sequence_complete_trailing_bytes() {
    let builder = SequenceCompleteResponseBuilder::new();
    let mut resp = builder.build();
    resp.push(0);

    let result = tpm::parse_sequence_complete_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::TrailingBytes));
}

#[gtest(TpmParserTest, SequenceCompleteTpmError)]
fn test_sequence_complete_tpm_error() {
    let builder = SequenceCompleteResponseBuilder::new().set_rc(0x100);
    let resp = builder.build();

    let result = tpm::parse_sequence_complete_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::TpmErrorResponse));
    expect_eq!(result.status.tpm_response_code, 0x100);
    expect_true!(result.digest.is_empty());
    expect_true!(result.validation_ticket.is_empty());
}

#[gtest(TpmTest, BuildFlushContextCommand)]
fn test_build_flush_context_command() {
    let handle = 0x80000001;
    let cmd = tpm::build_flush_context_command(handle);

    // Header (10) + Handle (4) = 14
    expect_eq!(cmd.len(), 14);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_NO_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 14);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_FLUSH_CONTEXT.repr);

    expect_eq!(reader.read_u32().unwrap(), handle);
}

struct FlushContextResponseBuilder {
    tag: u16,
    rc: u32,
}

impl FlushContextResponseBuilder {
    fn new() -> Self {
        Self { tag: tpm::TpmSt::TPM_ST_NO_SESSIONS.repr, rc: 0 }
    }

    fn set_tag(mut self, tag: u16) -> Self {
        self.tag = tag;
        self
    }

    fn set_rc(mut self, rc: u32) -> Self {
        self.rc = rc;
        self
    }

    fn build(self) -> Vec<u8> {
        let total_size = 10;
        let mut writer = tpm::Writer::with_capacity(total_size);
        writer.write_u16(self.tag);
        writer.write_u32(total_size as u32);
        writer.write_u32(self.rc);
        writer.into_inner()
    }
}

#[gtest(TpmParserTest, FlushContextHappyPath)]
fn test_flush_context_happy_path() {
    let builder = FlushContextResponseBuilder::new();
    let resp = builder.build();

    let result = tpm::parse_flush_context_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::Ok));
    expect_eq!(result.tpm_response_code, 0);
}

#[gtest(TpmParserTest, FlushContextWrongTag)]
fn test_flush_context_wrong_tag() {
    let builder = FlushContextResponseBuilder::new().set_tag(tpm::TpmSt::TPM_ST_SESSIONS.repr);
    let resp = builder.build();

    let result = tpm::parse_flush_context_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::WrongType));
}

#[gtest(TpmParserTest, FlushContextBufferTooSmall)]
fn test_flush_context_buffer_too_small() {
    let builder = FlushContextResponseBuilder::new();
    let mut resp = builder.build();
    resp.pop();

    let result = tpm::parse_flush_context_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::BufferTooSmall));
}

#[gtest(TpmParserTest, FlushContextTrailingBytes)]
fn test_flush_context_trailing_bytes() {
    let builder = FlushContextResponseBuilder::new();
    let mut resp = builder.build();
    resp.push(0);

    let result = tpm::parse_flush_context_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::TrailingBytes));
}

#[gtest(TpmParserTest, FlushContextTpmError)]
fn test_flush_context_tpm_error() {
    let builder = FlushContextResponseBuilder::new().set_rc(0x100);
    let resp = builder.build();

    let result = tpm::parse_flush_context_response(&resp);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::TpmErrorResponse));
    expect_eq!(result.tpm_response_code, 0x100);
}

#[gtest(TpmTest, BuildCreateAikCommandEccP256)]
fn test_build_create_aik_command_ecc_p256() {
    const PARENT_HANDLE: u32 = 0x81000009;
    let cmd = tpm::build_create_aik_command(
        PARENT_HANDLE,
        tpm::TpmAlgSigScheme::TPM_ALG_ECDSA,
        tpm::TpmAlgHash::TPM_ALG_SHA256,
    );
    expect_eq!(cmd.len(), 65);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 65);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_CREATE.repr);

    // Parent Handle
    expect_eq!(reader.read_u32().unwrap(), PARENT_HANDLE);

    // Auth session
    expect_eq!(reader.read_u32().unwrap(), 9);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmRh::TPM_RS_PW.repr);
    expect_eq!(reader.read_u16().unwrap(), 0);
    expect_eq!(reader.read_u8().unwrap(), 0);
    expect_eq!(reader.read_u16().unwrap(), 0);

    // inSensitive (size 4, userAuth 0, data 0)
    expect_eq!(reader.read_u16().unwrap(), 4);
    expect_eq!(reader.read_u16().unwrap(), 0);
    expect_eq!(reader.read_u16().unwrap(), 0);

    // inPublic (size 24)
    expect_eq!(reader.read_u16().unwrap(), 24);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgPublic::TPM_ALG_ECC.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgHash::TPM_ALG_SHA256.repr);
    expect_eq!(reader.read_u32().unwrap(), tpm::AIK_OBJECT_ATTRIBUTES);
    expect_eq!(reader.read_u16().unwrap(), 0); // authPolicy

    // parameters
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_NULL.repr); // symmetric
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_ECDSA.repr); // scheme
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgHash::TPM_ALG_SHA256.repr); // scheme hashAlg
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmEccCurve::TPM_ECC_NIST_P256.repr); // curveID
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_NULL.repr); // kdf

    // unique
    expect_eq!(reader.read_u16().unwrap(), 0); // x
    expect_eq!(reader.read_u16().unwrap(), 0); // y

    // outsideInfo
    expect_eq!(reader.read_u16().unwrap(), 0);

    // creationPCR
    expect_eq!(reader.read_u32().unwrap(), 0);
}

#[gtest(TpmTest, BuildCreateAikCommandEccP384)]
fn test_build_create_aik_command_ecc_p384() {
    const PARENT_HANDLE: u32 = 0x81000009;
    let cmd = tpm::build_create_aik_command(
        PARENT_HANDLE,
        tpm::TpmAlgSigScheme::TPM_ALG_ECDSA,
        tpm::TpmAlgHash::TPM_ALG_SHA384,
    );
    expect_eq!(cmd.len(), 65);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 65);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_CREATE.repr);
    expect_eq!(reader.read_u32().unwrap(), PARENT_HANDLE);

    // Skip auth session (13 bytes) + inSensitive (6 bytes)
    let _ = reader.read_bytes(19).unwrap();

    // inPublic
    expect_eq!(reader.read_u16().unwrap(), 24);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgPublic::TPM_ALG_ECC.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgHash::TPM_ALG_SHA384.repr);
    expect_eq!(reader.read_u32().unwrap(), tpm::AIK_OBJECT_ATTRIBUTES);
    expect_eq!(reader.read_u16().unwrap(), 0);

    // parameters
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_NULL.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_ECDSA.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgHash::TPM_ALG_SHA384.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmEccCurve::TPM_ECC_NIST_P384.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_NULL.repr);
}

#[gtest(TpmTest, BuildCreateAikCommandEccP521)]
fn test_build_create_aik_command_ecc_p521() {
    const PARENT_HANDLE: u32 = 0x81000009;
    let cmd = tpm::build_create_aik_command(
        PARENT_HANDLE,
        tpm::TpmAlgSigScheme::TPM_ALG_ECDSA,
        tpm::TpmAlgHash::TPM_ALG_SHA512,
    );
    expect_eq!(cmd.len(), 65);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 65);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_CREATE.repr);
    expect_eq!(reader.read_u32().unwrap(), PARENT_HANDLE);

    // Skip auth session (13 bytes) + inSensitive (6 bytes)
    let _ = reader.read_bytes(19).unwrap();

    // inPublic
    expect_eq!(reader.read_u16().unwrap(), 24);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgPublic::TPM_ALG_ECC.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgHash::TPM_ALG_SHA512.repr);
    expect_eq!(reader.read_u32().unwrap(), tpm::AIK_OBJECT_ATTRIBUTES);
    expect_eq!(reader.read_u16().unwrap(), 0);

    // parameters
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_NULL.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_ECDSA.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgHash::TPM_ALG_SHA512.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmEccCurve::TPM_ECC_NIST_P521.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_NULL.repr);
}

#[gtest(TpmTest, BuildCreateAikCommandRsaPkcs1)]
fn test_build_create_aik_command_rsa_pkcs1() {
    const PARENT_HANDLE: u32 = 0x81000001;
    let cmd = tpm::build_create_aik_command(
        PARENT_HANDLE,
        tpm::TpmAlgSigScheme::TPM_ALG_RSASSA,
        tpm::TpmAlgHash::TPM_ALG_SHA256,
    );
    expect_eq!(cmd.len(), 65);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 65);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_CREATE.repr);
    expect_eq!(reader.read_u32().unwrap(), PARENT_HANDLE);

    // Skip auth session (13 bytes) + inSensitive (6 bytes)
    let _ = reader.read_bytes(19).unwrap();

    // inPublic
    expect_eq!(reader.read_u16().unwrap(), 24);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgPublic::TPM_ALG_RSA.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgHash::TPM_ALG_SHA256.repr);
    expect_eq!(reader.read_u32().unwrap(), tpm::AIK_OBJECT_ATTRIBUTES);
    expect_eq!(reader.read_u16().unwrap(), 0);

    // parameters
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_NULL.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_RSASSA.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgHash::TPM_ALG_SHA256.repr);
    expect_eq!(reader.read_u16().unwrap(), 2048);
    expect_eq!(reader.read_u32().unwrap(), 0); // exponent

    // unique
    expect_eq!(reader.read_u16().unwrap(), 0);

    // outsideInfo
    expect_eq!(reader.read_u16().unwrap(), 0);

    // creationPCR
    expect_eq!(reader.read_u32().unwrap(), 0);
}

#[gtest(TpmTest, BuildCreateAikCommandRsaPss)]
fn test_build_create_aik_command_rsa_pss() {
    const PARENT_HANDLE: u32 = 0x81000001;
    let cmd = tpm::build_create_aik_command(
        PARENT_HANDLE,
        tpm::TpmAlgSigScheme::TPM_ALG_RSAPSS,
        tpm::TpmAlgHash::TPM_ALG_SHA256,
    );
    expect_eq!(cmd.len(), 65);

    let mut reader = tpm::Reader::new(&cmd);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmSt::TPM_ST_SESSIONS.repr);
    expect_eq!(reader.read_u32().unwrap(), 65);
    expect_eq!(reader.read_u32().unwrap(), tpm::TpmCc::TPM_CC_CREATE.repr);
    expect_eq!(reader.read_u32().unwrap(), PARENT_HANDLE);

    // Skip auth session (13 bytes) + inSensitive (6 bytes)
    let _ = reader.read_bytes(19).unwrap();

    // inPublic
    expect_eq!(reader.read_u16().unwrap(), 24);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgPublic::TPM_ALG_RSA.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgHash::TPM_ALG_SHA256.repr);
    expect_eq!(reader.read_u32().unwrap(), tpm::AIK_OBJECT_ATTRIBUTES);
    expect_eq!(reader.read_u16().unwrap(), 0);

    // parameters
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_NULL.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgSigScheme::TPM_ALG_RSAPSS.repr);
    expect_eq!(reader.read_u16().unwrap(), tpm::TpmAlgHash::TPM_ALG_SHA256.repr);
    expect_eq!(reader.read_u16().unwrap(), 2048);
    expect_eq!(reader.read_u32().unwrap(), 0); // exponent

    // unique
    expect_eq!(reader.read_u16().unwrap(), 0);
}

struct CreateResponseBuilder {
    tag: u16,
    rc: u32,
    out_private: Vec<u8>,
    out_public: Vec<u8>,
    creation_data: Vec<u8>,
    creation_hash: Vec<u8>,
    ticket_tag: u16,
    ticket_hierarchy: u32,
    ticket_digest: Vec<u8>,
}

impl CreateResponseBuilder {
    fn new() -> Self {
        Self {
            tag: tpm::TpmSt::TPM_ST_SESSIONS.repr,
            rc: 0,
            out_private: vec![1, 2, 3],
            out_public: vec![4, 5, 6],
            creation_data: vec![7, 8],
            creation_hash: vec![9, 10],
            ticket_tag: tpm::TpmSt::TPM_ST_CREATION.repr,
            ticket_hierarchy: tpm::TpmRh::TPM_RH_OWNER.repr,
            ticket_digest: vec![11, 12],
        }
    }

    fn set_tag(mut self, tag: u16) -> Self {
        self.tag = tag;
        self
    }

    fn set_rc(mut self, rc: u32) -> Self {
        self.rc = rc;
        self
    }

    fn set_ticket_tag(mut self, ticket_tag: u16) -> Self {
        self.ticket_tag = ticket_tag;
        self
    }

    fn build(self) -> Vec<u8> {
        let ticket_size = 2 // tag
            + 4 // hierarchy
            + 2 // digest size
            + u32::try_from(self.ticket_digest.len()).unwrap();

        let param_size = 2
            + u32::try_from(self.out_private.len()).unwrap()
            + 2
            + u32::try_from(self.out_public.len()).unwrap()
            + 2
            + u32::try_from(self.creation_data.len()).unwrap()
            + 2
            + u32::try_from(self.creation_hash.len()).unwrap()
            + ticket_size;

        let mut total_size = 10;
        if self.rc == 0 {
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                total_size += 4; // parameterSize field
            }
            total_size += param_size;
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                total_size += 5; // auth session size (nonce size 0, attrs 0,
                                 // hmac size 0)
            }
        }

        let mut writer = tpm::Writer::with_capacity(total_size.try_into().unwrap());
        writer.write_u16(self.tag);
        writer.write_u32(total_size);
        writer.write_u32(self.rc);

        if self.rc == 0 {
            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                writer.write_u32(param_size);
            }
            writer.write_tpm2b(&self.out_private);
            writer.write_tpm2b(&self.out_public);
            writer.write_tpm2b(&self.creation_data);
            writer.write_tpm2b(&self.creation_hash);
            writer.write_bytes(&build_test_ticket(
                self.ticket_tag,
                self.ticket_hierarchy,
                &self.ticket_digest,
            ));

            if self.tag == tpm::TpmSt::TPM_ST_SESSIONS.repr {
                writer.write_u16(0); // nonce size
                writer.write_u8(0); // sessionAttributes
                writer.write_u16(0); // hmac size
            }
        }

        writer.into_inner()
    }
}

#[gtest(TpmParserTest, CreateHappyPath)]
fn test_create_happy_path() {
    let builder = CreateResponseBuilder::new();
    let resp = builder.build();

    let result = tpm::parse_create_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::Ok));
    expect_eq!(result.status.tpm_response_code, 0);
    expect_eq!(result.out_private, &[0, 3, 1, 2, 3]);
    expect_eq!(result.out_public, &[0, 3, 4, 5, 6]);
}

#[gtest(TpmParserTest, CreateBufferTooSmall)]
fn test_create_buffer_too_small() {
    let builder = CreateResponseBuilder::new();
    let mut resp = builder.build();
    resp.pop();

    let result = tpm::parse_create_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::BufferTooSmall));
}

#[gtest(TpmParserTest, CreateTrailingBytes)]
fn test_create_trailing_bytes() {
    let builder = CreateResponseBuilder::new();
    let mut resp = builder.build();
    resp.push(0);

    let result = tpm::parse_create_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::TrailingBytes));
}

#[gtest(TpmParserTest, CreateTpmError)]
fn test_create_tpm_error() {
    let builder = CreateResponseBuilder::new().set_rc(0x100);
    let resp = builder.build();

    let result = tpm::parse_create_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::TpmErrorResponse));
    expect_eq!(result.status.tpm_response_code, 0x100);
    expect_true!(result.out_private.is_empty());
    expect_true!(result.out_public.is_empty());
}

#[gtest(TpmParserTest, CreateWrongTicketTag)]
fn test_create_wrong_ticket_tag() {
    let builder = CreateResponseBuilder::new().set_ticket_tag(tpm::TpmSt::TPM_ST_HASHCHECK.repr);
    let resp = builder.build();

    let result = tpm::parse_create_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::WrongType));
}

#[gtest(TpmParserTest, CreateWrongTag)]
fn test_create_wrong_tag() {
    let builder = CreateResponseBuilder::new().set_tag(0x8003);
    let resp = builder.build();

    let result = tpm::parse_create_response(&resp);
    expect_true!(matches!(result.status.result, tpm::ffi::ParseResult::WrongType));
}

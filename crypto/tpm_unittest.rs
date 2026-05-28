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
const DUMMY_SIGNATURE: &[u8] = &[1, 2, 3, 4, 5, 6];
const INVALID_SPKI: &[u8] = &[0xBA, 0xAD, 0xBE, 0xEF];

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
                sig_alg: tpm::TPM_ALG_RSASSA,
                hash_alg: tpm::TPM_ALG_SHA256,
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
        if self.algorithms.sig_alg == tpm::TPM_ALG_RSASSA {
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
            if self.algorithms.sig_alg == tpm::TPM_ALG_RSASSA {
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
    expect_eq!(reader.read_u16().unwrap(), tpm::TPM_ALG_NULL);
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

#[gtest(TpmParserTest, NonceMismatch)]
fn test_nonce_mismatch() {
    let nonce_mismatch = ResponseBuilder::new().with_extra_data(QUALIFYING_DATA).build();

    let challenge: &[u8] = WRONG_CHALLENGE;
    let result = tpm::parse_certify_response(&nonce_mismatch, challenge);
    expect_true!(matches!(result.result, tpm::ffi::ParseResult::NonceMismatch));
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

#[gtest(TpmVerifySignatureTest, UnsupportedAlgorithm)]
fn test_unsupported_algorithm() {
    let statement = b"test payload";
    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TPM_ALG_RSASSA);
    writer.write_u16(0x000C); // Invalid, expected SHA256=0x000B
    writer.write_tpm2b(DUMMY_SIGNATURE);
    let signature = writer.into_inner();

    let key = bssl_crypto::rsa::PrivateKey::generate(bssl_crypto::rsa::KeySize::Rsa2048);
    let spki = key.as_public().to_der_subject_public_key_info();

    let verify_response = tpm::verify_signature(statement, &signature, spki.as_ref());

    expect_true!(matches!(verify_response, tpm::ffi::VerificationResult::UnsupportedHashAlgorithm));
}

#[gtest(TpmVerifySignatureTest, InvalidPublicKey)]
fn test_invalid_public_key() {
    let statement = b"test payload";
    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TPM_ALG_RSASSA);
    writer.write_u16(tpm::TPM_ALG_SHA256);
    writer.write_tpm2b(DUMMY_SIGNATURE);
    let signature = writer.into_inner();

    let verify_response = tpm::verify_signature(statement, &signature, INVALID_SPKI);
    expect_true!(matches!(verify_response, tpm::ffi::VerificationResult::InvalidPublicKey));
}

#[gtest(TpmVerifySignatureTest, ValidRsaSsaSignature)]
fn test_valid_rsassa_signature() {
    let key = bssl_crypto::rsa::PrivateKey::generate(bssl_crypto::rsa::KeySize::Rsa2048);
    let spki = key.as_public().to_der_subject_public_key_info();

    let statement = b"test payload";
    let sig = key.sign_pkcs1::<bssl_crypto::digest::Sha256>(statement);

    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TPM_ALG_RSASSA);
    writer.write_u16(tpm::TPM_ALG_SHA256);
    writer.write_tpm2b(&sig);
    let signature = writer.into_inner();

    let verify_response = tpm::verify_signature(statement, &signature, spki.as_ref());
    expect_true!(matches!(verify_response, tpm::ffi::VerificationResult::Ok));
}

#[gtest(TpmVerifySignatureTest, ValidEcdsaSignature)]
fn test_valid_ecdsa_signature() {
    let key = bssl_crypto::ecdsa::PrivateKey::<bssl_crypto::ec::P256>::generate();
    let spki = key.to_public_key().to_der_subject_public_key_info();

    let statement = b"test payload";
    let sig = key.sign_p1363(statement);
    expect_eq!(sig.len(), 64);
    let (r, s) = sig.split_at(32);

    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TPM_ALG_ECDSA);
    writer.write_u16(tpm::TPM_ALG_SHA256);
    writer.write_tpm2b(r);
    writer.write_tpm2b(s);
    let signature = writer.into_inner();

    let verify_response = tpm::verify_signature(statement, &signature, spki.as_ref());
    expect_true!(matches!(verify_response, tpm::ffi::VerificationResult::Ok));
}

#[gtest(TpmVerifySignatureTest, ValidRsaSsaSha1Signature)]
fn test_valid_rsassa_sha1_signature() {
    let key = bssl_crypto::rsa::PrivateKey::generate(bssl_crypto::rsa::KeySize::Rsa2048);
    let spki = key.as_public().to_der_subject_public_key_info();

    let statement = b"test payload";
    let sig = key.sign_pkcs1::<bssl_crypto::digest::InsecureSha1>(statement);

    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TPM_ALG_RSASSA);
    writer.write_u16(tpm::TPM_ALG_SHA1);
    writer.write_tpm2b(&sig);
    let signature = writer.into_inner();

    let verify_response = tpm::verify_signature(statement, &signature, spki.as_ref());
    expect_true!(matches!(verify_response, tpm::ffi::VerificationResult::Ok));
}

#[gtest(TpmVerifySignatureTest, EcdsaSha1Unsupported)]
fn test_ecdsa_sha1_unsupported() {
    let key = bssl_crypto::ecdsa::PrivateKey::<bssl_crypto::ec::P256>::generate();
    let spki = key.to_public_key().to_der_subject_public_key_info();

    let statement = b"test payload";
    let sig = key.sign_p1363(statement);
    let (r, s) = sig.split_at(32);

    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TPM_ALG_ECDSA);
    writer.write_u16(tpm::TPM_ALG_SHA1);
    writer.write_tpm2b(r);
    writer.write_tpm2b(s);
    let signature = writer.into_inner();

    let verify_response = tpm::verify_signature(statement, &signature, spki.as_ref());
    expect_true!(matches!(verify_response, tpm::ffi::VerificationResult::UnsupportedHashAlgorithm));
}

#[gtest(TpmVerifySignatureTest, UnsupportedSignatureAlgorithm)]
fn test_unsupported_signature_algorithm() {
    let statement = b"test payload";
    let mut writer = tpm::Writer::new();
    writer.write_u16(0x1234); // Invalid signature algorithm
    writer.write_u16(tpm::TPM_ALG_SHA256);
    writer.write_bytes(DUMMY_SIGNATURE);
    let signature = writer.into_inner();

    let key = bssl_crypto::rsa::PrivateKey::generate(bssl_crypto::rsa::KeySize::Rsa2048);
    let spki = key.as_public().to_der_subject_public_key_info();

    let verify_response = tpm::verify_signature(statement, &signature, spki.as_ref());
    expect_true!(matches!(
        verify_response,
        tpm::ffi::VerificationResult::UnsupportedSignatureAlgorithm
    ));
}

#[gtest(TpmVerifySignatureTest, InvalidSignature)]
fn test_invalid_signature() {
    let key1 = bssl_crypto::rsa::PrivateKey::generate(bssl_crypto::rsa::KeySize::Rsa2048);
    let key2 = bssl_crypto::rsa::PrivateKey::generate(bssl_crypto::rsa::KeySize::Rsa2048);
    let spki2 = key2.as_public().to_der_subject_public_key_info();

    let statement = b"test payload";
    let sig = key1.sign_pkcs1::<bssl_crypto::digest::Sha256>(statement);

    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TPM_ALG_RSASSA);
    writer.write_u16(tpm::TPM_ALG_SHA256);
    writer.write_tpm2b(&sig);
    let signature = writer.into_inner();

    let verify_response = tpm::verify_signature(statement, &signature, spki2.as_ref());
    expect_true!(matches!(verify_response, tpm::ffi::VerificationResult::InvalidSignature));
}

#[gtest(TpmExtractionTest, ValidExtraction)]
fn test_valid_extraction() {
    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TPM_ALG_RSASSA);
    writer.write_u16(tpm::TPM_ALG_SHA256);
    writer.write_tpm2b(DUMMY_SIGNATURE);
    let signature = writer.into_inner();

    let signature_algorithms_response = tpm::extract_signature_algorithms(&signature);
    expect_true!(signature_algorithms_response.has_algorithms);
    expect_eq!(signature_algorithms_response.sig_alg, tpm::TPM_ALG_RSASSA);
    expect_eq!(signature_algorithms_response.hash_alg, tpm::TPM_ALG_SHA256);
}

#[gtest(TpmExtractionTest, BufferTooSmall)]
fn test_buffer_too_small() {
    let mut writer = tpm::Writer::new();
    writer.write_u16(tpm::TPM_ALG_RSASSA);
    let signature = writer.into_inner(); // Missing hash_alg

    let signature_algorithms_response = tpm::extract_signature_algorithms(&signature);
    expect_false!(signature_algorithms_response.has_algorithms);
}

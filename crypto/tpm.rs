// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub use ffi::{
    CreateResponse, ResponseStatus, TpmAlgHash, TpmAlgPublic, TpmAlgSigScheme, TpmCc, TpmConstant,
    TpmEccCurve, TpmRh, TpmSt,
};

/// Size of a standard TPM command header (Tag + Size + CommandCode).
pub const TPM_HEADER_SIZE: usize = 10;
/// Size of a TPM handle in bytes.
pub const TPM_HANDLE_SIZE: usize = 4;
/// Size of the auth size field in bytes.
pub const TPM_AUTH_SIZE_SIZE: usize = 4;
/// Size of a password session authorization area in bytes.
pub const TPM_SESSION_SIZE: usize = 9;

/// Maximum buffer size for a TPM2B_MAX_BUFFER structure (typically 1024 bytes
/// in TPM 2.0).
pub const TPM_MAX_BUFFER_SIZE: usize = 1024;

/// Object attributes for an Attestation Identity Key (AIK).
/// fixedTPM (0x02) | fixedParent (0x10) | sensitiveDataOrigin (0x20) |
/// userWithAuth (0x40) | restricted (0x10000) | sign (0x40000) = 0x00050072.
pub const AIK_OBJECT_ATTRIBUTES: u32 = 0x00050072;

/// Errors that can occur during TPM response parsing.
#[derive(Debug)]
pub enum TpmParseError {
    /// The provided buffer was too small to read the required data.
    BufferTooSmall,
    /// The provided buffer had trailing bytes after parsing completed.
    TrailingBytes,
    /// The TPM returned an error code. Contains the TPM response code.
    TpmErrorResponse(u32),
    /// The structure did not contain the expected TPM magic number.
    BadMagicNumber,
    /// The structure type did not match the expected type.
    WrongType,
    /// The provided challenge did not match the challenge in the attestation.
    ChallengeMismatch,
}

impl std::fmt::Display for TpmParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            TpmParseError::BufferTooSmall => write!(f, "buffer too small"),
            TpmParseError::TrailingBytes => write!(f, "trailing bytes in buffer"),
            TpmParseError::TpmErrorResponse(code) => {
                write!(f, "TPM returned an error response: {:#010x}", code)
            }
            TpmParseError::BadMagicNumber => write!(f, "bad magic number in TPM response"),
            TpmParseError::WrongType => write!(f, "wrong type in TPM response"),
            TpmParseError::ChallengeMismatch => write!(f, "challenge mismatch in TPM response"),
        }
    }
}

impl std::error::Error for TpmParseError {}

/// Errors that can occur during TPM signature parsing.
#[derive(Debug)]
pub enum TpmSignatureParseError {
    /// The provided signature buffer was too small to read the required data.
    BufferTooSmall,
    /// The provided signature buffer had trailing bytes after parsing the
    /// signature.
    TrailingBytes,
    /// The signature algorithm identified is not supported by this verification
    /// implementation.
    UnsupportedSignatureAlgorithm,
}

impl std::fmt::Display for TpmSignatureParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            TpmSignatureParseError::BufferTooSmall => write!(f, "buffer too small"),
            TpmSignatureParseError::TrailingBytes => write!(f, "trailing bytes in buffer"),
            TpmSignatureParseError::UnsupportedSignatureAlgorithm => {
                write!(f, "unsupported signature algorithm")
            }
        }
    }
}

impl std::error::Error for TpmSignatureParseError {}

/// CXX bridge for TPM FFI.
#[cxx::bridge(namespace = "crypto::tpm")]
pub mod ffi {
    /// Results that can occur during TPM response parsing.
    // LINT.IfChange(ParseResult)
    #[derive(Debug)]
    enum ParseResult {
        /// Parsing completed successfully.
        Ok = 0,
        /// The buffer was too small to read the required fields.
        BufferTooSmall = 1,
        /// The buffer had trailing bytes after parsing.
        TrailingBytes = 2,
        /// The TPM returned an error code.
        TpmErrorResponse = 3,
        /// The structure did not contain the expected TPM magic number.
        BadMagicNumber = 4,
        /// The structure type did not match the expected type.
        WrongType = 5,
        /// The provided challenge did not match the challenge in the
        /// attestation.
        ChallengeMismatch = 6,
    }
    // LINT.ThenChange(//crypto/tpm_parser.h:TpmParseResult)

    /// Status of a TPM parse operation. This is effectively an unrolled version
    /// of `Result<(), TpmParseError>` used across the CXX FFI boundary, where
    /// Rust enum payloads and Result types cannot easily be passed directly.
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    struct ResponseStatus {
        /// The outcome of the parsing operation.
        result: ParseResult,
        /// The TPM response code, if the TPM returned an error.
        tpm_response_code: u32,
    }

    /// Response from parsing a TPM2_Create command.
    #[cxx_name = "RawCreateResponse"]
    struct CreateResponse {
        /// The outcome of the parsing operation.
        status: ResponseStatus,
        /// The serialized `TPM2B_PRIVATE` structure returned by the TPM.
        out_private: Vec<u8>,
        /// The serialized `TPM2B_PUBLIC` structure returned by the TPM.
        out_public: Vec<u8>,
    }

    /// Response from parsing a TPM2_Certify command.
    #[cxx_name = "RawCertifyResponse"]
    struct CertifyResponse {
        /// The outcome of the parsing operation.
        status: ResponseStatus,
        /// The serialized `TPMS_ATTEST` statement returned by the TPM.
        statement: Vec<u8>,
        /// The serialized `TPMT_SIGNATURE` returned by the TPM.
        signature: Vec<u8>,
    }

    /// Response from parsing a TPM2_Hash command.
    #[cxx_name = "RawHashResponse"]
    struct HashResponse {
        /// The outcome of the parsing operation.
        status: ResponseStatus,
        /// The hash digest returned by the TPM.
        digest: Vec<u8>,
        /// The validation ticket (`TPMT_TK_HASHCHECK`) returned by the TPM.
        validation_ticket: Vec<u8>,
    }

    /// Response from parsing a TPM2_Sign command.
    #[cxx_name = "RawSignResponse"]
    struct SignResponse {
        /// The outcome of the parsing operation.
        status: ResponseStatus,
        /// The serialized `TPMT_SIGNATURE` returned by the TPM.
        signature: Vec<u8>,
    }

    /// Response from parsing a TPM2_HashSequenceStart command.
    #[cxx_name = "RawHashSequenceStartResponse"]
    struct HashSequenceStartResponse {
        /// The outcome of the parsing operation.
        status: ResponseStatus,
        /// The sequence handle created by the TPM.
        sequence_handle: u32,
    }

    /// Results that can occur during TPM signature parsing.
    // LINT.IfChange(SignatureParseResult)
    #[derive(Debug)]
    enum SignatureParseResult {
        /// Parsing completed successfully.
        Ok = 0,
        /// The signature buffer was too small to read the required fields.
        BufferTooSmall = 1,
        /// The signature buffer had trailing bytes after parsing.
        TrailingBytes = 2,
        /// The signature algorithm is not supported.
        UnsupportedSignatureAlgorithm = 3,
    }
    // LINT.ThenChange(//crypto/tpm_parser.h:TpmCertifyVerifyResult)

    /// TPM Public / Key Types. See Table 9 & 14 in
    /// https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=41.
    #[derive(Debug, PartialEq, Eq)]
    #[repr(u16)]
    enum TpmAlgPublic {
        /// TPM_ALG_RSA is the RSA algorithm.
        TPM_ALG_RSA = 0x0001,
        /// TPM_ALG_ECC is the ECC algorithm.
        TPM_ALG_ECC = 0x0023,
    }

    /// TPM Cryptographic Hash Algorithms. See Table 10 in
    /// https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=42.
    #[derive(Debug, PartialEq, Eq)]
    #[repr(u16)]
    enum TpmAlgHash {
        /// TPM_ALG_SHA1 is the SHA-1 hash algorithm.
        TPM_ALG_SHA1 = 0x0004,
        /// TPM_ALG_SHA256 is the SHA-256 hash algorithm.
        TPM_ALG_SHA256 = 0x000B,
        /// TPM_ALG_SHA384 is the SHA-384 hash algorithm.
        TPM_ALG_SHA384 = 0x000C,
        /// TPM_ALG_SHA512 is the SHA-512 hash algorithm.
        TPM_ALG_SHA512 = 0x000D,
    }

    /// TPM Signature Schemes. See Table 13 in
    /// https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=44.
    #[derive(Debug, PartialEq, Eq)]
    #[repr(u16)]
    enum TpmAlgSigScheme {
        /// TPM_ALG_NULL is the null algorithm. In TPM 2.0, TPM_ALG_NULL is a
        /// valid signature scheme indicating no scheme, or instructing
        /// the TPM to use the scheme configured in a restricted key's
        /// public template.
        TPM_ALG_NULL = 0x0010,
        /// TPM_ALG_RSASSA is the RSASSA signature algorithm.
        TPM_ALG_RSASSA = 0x0014,
        /// TPM_ALG_RSAPSS is the RSAPSS signature algorithm.
        TPM_ALG_RSAPSS = 0x0016,
        /// TPM_ALG_ECDSA is the ECDSA signature algorithm.
        TPM_ALG_ECDSA = 0x0018,
    }

    /// TPM ECC Curves. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=46 for details.
    #[derive(Debug)]
    #[repr(u16)]
    enum TpmEccCurve {
        /// TPM_ECC_NONE is no curve.
        TPM_ECC_NONE = 0x0000,
        /// TPM_ECC_NIST_P256 is the NIST P-256 curve.
        TPM_ECC_NIST_P256 = 0x0003,
        /// TPM_ECC_NIST_P384 is the NIST P-384 curve.
        TPM_ECC_NIST_P384 = 0x0004,
        /// TPM_ECC_NIST_P521 is the NIST P-521 curve.
        TPM_ECC_NIST_P521 = 0x0005,
    }

    /// TPM Reserved Handles. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=88 for details.
    #[derive(Debug)]
    #[repr(u32)]
    enum TpmRh {
        /// TPM_RH_OWNER is the handle for the storage hierarchy. This is
        /// used for standard keys and as the hierarchy for validation tickets
        /// in unit tests.
        TPM_RH_OWNER = 0x40000001,
        /// TPM_RS_PW is the handle for a password session.
        TPM_RS_PW = 0x40000009,
    }

    /// TPM Constants.
    #[derive(Debug)]
    #[repr(u32)]
    enum TpmConstant {
        /// TPM_GENERATED_VALUE is the magic number in TPM generated structures.
        TPM_GENERATED_VALUE = 0xFF544347,
    }

    /// TPM Command Codes.
    #[derive(Debug)]
    #[repr(u32)]
    enum TpmCc {
        /// TPM_CC_SEQUENCE_COMPLETE is the command code for
        /// TPM2_SequenceComplete.
        TPM_CC_SEQUENCE_COMPLETE = 0x0000013E,
        /// TPM_CC_CERTIFY is the command code for TPM2_Certify.
        TPM_CC_CERTIFY = 0x00000148,
        /// TPM_CC_CREATE is the command code for TPM2_Create.
        TPM_CC_CREATE = 0x00000153,
        /// TPM_CC_SEQUENCE_UPDATE is the command code for TPM2_SequenceUpdate.
        TPM_CC_SEQUENCE_UPDATE = 0x0000015C,
        /// TPM_CC_SIGN is the command code for TPM2_Sign.
        TPM_CC_SIGN = 0x0000015D,
        /// TPM_CC_FLUSH_CONTEXT is the command code for TPM2_FlushContext.
        TPM_CC_FLUSH_CONTEXT = 0x00000165,
        /// TPM_CC_HASH is the command code for TPM2_Hash.
        TPM_CC_HASH = 0x0000017D,
        /// TPM_CC_HASH_SEQUENCE_START is the command code for
        /// TPM2_HashSequenceStart.
        TPM_CC_HASH_SEQUENCE_START = 0x00000186,
    }

    /// TPM Structure Tags.
    #[derive(Debug)]
    #[repr(u16)]
    enum TpmSt {
        /// TPM_ST_NO_SESSIONS indicates that the command has no sessions.
        TPM_ST_NO_SESSIONS = 0x8001,
        /// TPM_ST_SESSIONS indicates that the command has sessions.
        TPM_ST_SESSIONS = 0x8002,
        /// TPM_ST_ATTEST_CERTIFY is the tag for a certify attestation
        /// statement.
        TPM_ST_ATTEST_CERTIFY = 0x8017,
        /// TPM_ST_CREATION is the tag for a creation ticket.
        TPM_ST_CREATION = 0x8021,
        /// TPM_ST_HASHCHECK is the tag for a hashcheck validation ticket.
        TPM_ST_HASHCHECK = 0x8024,
    }

    /// Struct containing the parsed raw components of a TPM signature.
    struct RawSignatureComponents {
        /// The outcome of the parsing operation.
        status: SignatureParseResult,
        /// The signature algorithm ID (e.g., TPM_ALG_RSASSA or TPM_ALG_ECDSA).
        sig_alg: TpmAlgSigScheme,
        /// The hash algorithm ID (e.g., TPM_ALG_SHA256).
        hash_alg: TpmAlgHash,
        /// The raw RSA signature bytes, if sig_alg is TPM_ALG_RSASSA.
        rsa_sig: Vec<u8>,
        /// The raw ECDSA r coordinate, if sig_alg is TPM_ALG_ECDSA.
        ecdsa_r: Vec<u8>,
        /// The raw ECDSA s coordinate, if sig_alg is TPM_ALG_ECDSA.
        ecdsa_s: Vec<u8>,
    }

    extern "Rust" {
        /// Builds a TPM2_Certify command buffer.
        ///
        /// This function constructs the raw byte representation of a
        /// TPM2_Certify command.
        ///
        /// # Arguments
        ///
        /// * `object_handle` - Handle of the object to be certified (e.g., the
        ///   signing key).
        /// * `sign_handle` - Handle of the key used to sign the attestation
        ///   (e.g., the AIK).
        /// * `qualifying_data` - Data provided by the caller to ensure
        ///   freshness (e.g., a challenge).
        ///
        /// # Returns
        ///
        /// A `Vec<u8>` containing the serialized command buffer.
        ///
        /// # Panics
        ///
        /// Panics if `qualifying_data` exceeds `u16::MAX` bytes.
        fn build_certify_command(
            object_handle: u32,
            sign_handle: u32,
            qualifying_data: &[u8],
        ) -> Vec<u8>;

        /// Parses a TPM2_Certify response.
        ///
        /// This function reads the response buffer from a TPM2_Certify command,
        /// validates the headers, and extracts the attestation
        /// statement and signature. It also verifies that the response
        /// is for a certify command, checks the magic number,
        /// and ensures the provided `expected_extra_data` matches the one in
        /// the attestation's extra data to prevent replay attacks
        /// (TPM2_Certify operates on `TPM2B_DATA qualifyingData`, which
        /// for key attestation protocols is typically the SHA-256
        /// digest of the challenge).
        ///
        /// # Arguments
        ///
        /// * `resp` - The raw byte response from the TPM2_Certify command.
        /// * `expected_extra_data` - The extra data expected in the
        ///   attestation's `extra_data` field (e.g., the SHA-256 digest of the
        ///   challenge).
        ///
        /// # Returns
        ///
        /// A `CertifyResponse` containing the parsing result, any TPM error
        /// code, the serialized `TPMS_ATTEST` statement, and the
        /// serialized `TPMT_SIGNATURE`.
        fn parse_certify_response(resp: &[u8], expected_extra_data: &[u8]) -> CertifyResponse;

        /// Builds a TPM2_Create command buffer for an Attestation Identity Key
        /// (AIK).
        ///
        /// This function constructs the raw byte representation of a
        /// TPM2_Create command.
        ///
        /// # Arguments
        ///
        /// * `parent_handle` - Handle of the parent key (e.g., Storage Root
        ///   Key).
        /// * `scheme` - Signing scheme (e.g., TPM_ALG_ECDSA, TPM_ALG_RSASSA, or
        ///   TPM_ALG_RSAPSS).
        /// * `hash_alg` - Hash algorithm (e.g., TPM_ALG_SHA256, TPM_ALG_SHA384,
        ///   or TPM_ALG_SHA512).
        ///
        /// # Returns
        ///
        /// A `Vec<u8>` containing the serialized command buffer.
        ///
        /// # Panics
        ///
        /// Panics if `scheme` or `hash_alg` is unsupported.
        fn build_create_aik_command(
            parent_handle: u32,
            scheme: TpmAlgSigScheme,
            hash_alg: TpmAlgHash,
        ) -> Vec<u8>;

        /// Parses a TPM2_Create response.
        ///
        /// This function reads the response buffer from a TPM2_Create command,
        /// validates the response headers and creation ticket, and extracts the
        /// private and public key areas.
        ///
        /// # Arguments
        ///
        /// * `resp` - The raw byte response from the TPM2_Create command.
        ///
        /// # Returns
        ///
        /// A `CreateResponse` containing the parsing result, any TPM error
        /// code, the serialized `TPM2B_PRIVATE` structure, and the serialized
        /// `TPM2B_PUBLIC` structure.
        fn parse_create_response(resp: &[u8]) -> CreateResponse;

        /// Builds a TPM2_FlushContext command buffer.
        fn build_flush_context_command(handle: u32) -> Vec<u8>;

        /// Parses a TPM2_FlushContext response.
        fn parse_flush_context_response(resp: &[u8]) -> ResponseStatus;

        /// Builds a TPM2_Hash command buffer.
        fn build_hash_command(data: &[u8], hash_alg: TpmAlgHash) -> Vec<u8>;

        /// Parses a TPM2_Hash response.
        ///
        /// Note that if the TPM returns an error code, the `digest` and
        /// `validation_ticket` fields will be empty.
        fn parse_hash_response(resp: &[u8]) -> HashResponse;

        /// Builds a TPM2_HashSequenceStart command buffer.
        fn build_hash_sequence_start_command(hash_alg: TpmAlgHash) -> Vec<u8>;

        /// Parses a TPM2_HashSequenceStart response.
        fn parse_hash_sequence_start_response(resp: &[u8]) -> HashSequenceStartResponse;

        /// Builds a TPM2_SequenceUpdate command buffer.
        fn build_sequence_update_command(sequence_handle: u32, data: &[u8]) -> Vec<u8>;

        /// Parses a TPM2_SequenceUpdate response.
        fn parse_sequence_update_response(resp: &[u8]) -> ResponseStatus;

        /// Builds a TPM2_SequenceComplete command buffer.
        fn build_sequence_complete_command(sequence_handle: u32, data: &[u8]) -> Vec<u8>;

        /// Parses a TPM2_SequenceComplete response.
        fn parse_sequence_complete_response(resp: &[u8]) -> HashResponse;

        /// Builds a TPM2_Sign command buffer.
        fn build_sign_command(key_handle: u32, digest: &[u8], validation_ticket: &[u8]) -> Vec<u8>;

        /// Parses a TPM2_Sign response.
        fn parse_sign_response(resp: &[u8]) -> SignResponse;

        /// Parses a serialized `TPMT_SIGNATURE` and returns its raw components.
        fn parse_tpm_signature(signature: &[u8]) -> RawSignatureComponents;
    }
}

impl ffi::ResponseStatus {
    /// A successful response status with no error code.
    pub const OK: Self = Self { result: ffi::ParseResult::Ok, tpm_response_code: 0 };
}

impl From<TpmParseError> for ffi::ResponseStatus {
    fn from(err: TpmParseError) -> Self {
        match err {
            TpmParseError::BufferTooSmall => {
                Self { result: ffi::ParseResult::BufferTooSmall, tpm_response_code: 0 }
            }
            TpmParseError::TrailingBytes => {
                Self { result: ffi::ParseResult::TrailingBytes, tpm_response_code: 0 }
            }
            TpmParseError::TpmErrorResponse(code) => {
                Self { result: ffi::ParseResult::TpmErrorResponse, tpm_response_code: code }
            }
            TpmParseError::BadMagicNumber => {
                Self { result: ffi::ParseResult::BadMagicNumber, tpm_response_code: 0 }
            }
            TpmParseError::WrongType => {
                Self { result: ffi::ParseResult::WrongType, tpm_response_code: 0 }
            }
            TpmParseError::ChallengeMismatch => {
                Self { result: ffi::ParseResult::ChallengeMismatch, tpm_response_code: 0 }
            }
        }
    }
}

impl From<Result<(), TpmParseError>> for ffi::ResponseStatus {
    fn from(result: Result<(), TpmParseError>) -> Self {
        match result {
            Ok(()) => Self::OK,
            Err(err) => err.into(),
        }
    }
}

/// Header of a TPM response.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ResponseHeader {
    pub tag: TpmSt,
    pub response_size: usize,
}

/// A helper structure to read structured data from a byte slice.
/// Used for parsing TPM responses.
pub struct Reader<'a> {
    data: &'a [u8],
}

impl<'a> Reader<'a> {
    /// Creates a new Reader for the given byte slice.
    pub fn new(data: &'a [u8]) -> Self {
        Self { data }
    }

    /// Returns true if the reader has no more data.
    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    /// Returns the number of remaining bytes.
    pub fn remaining(&self) -> usize {
        self.data.len()
    }

    /// Ensures that no unread bytes remain in the reader.
    pub fn ensure_empty(&self) -> Result<(), TpmParseError> {
        if self.is_empty() {
            Ok(())
        } else {
            Err(TpmParseError::TrailingBytes)
        }
    }

    /// Reads and validates a standard TPM response header.
    ///
    /// Checks that the buffer contains the full response, that there are no
    /// trailing bytes beyond `response_size`, and that the TPM returned a
    /// success code (0).
    pub fn read_response_header(
        &mut self,
        total_len: usize,
    ) -> Result<ResponseHeader, TpmParseError> {
        let tag =
            self.read_u16().map(|repr| TpmSt { repr }).ok_or(TpmParseError::BufferTooSmall)?;
        let response_size: usize = self
            .read_u32()
            .ok_or(TpmParseError::BufferTooSmall)?
            .try_into()
            .map_err(|_| TpmParseError::BufferTooSmall)?;
        let response_code = self.read_u32().ok_or(TpmParseError::BufferTooSmall)?;

        if total_len < response_size {
            return Err(TpmParseError::BufferTooSmall);
        }
        if total_len > response_size {
            return Err(TpmParseError::TrailingBytes);
        }
        if response_code != 0 {
            return Err(TpmParseError::TpmErrorResponse(response_code));
        }

        Ok(ResponseHeader { tag, response_size })
    }

    /// Reads `len` bytes from the slice. Returns error if buffer is too small.
    pub fn read_bytes(&mut self, len: usize) -> Option<&'a [u8]> {
        let (val, rest) = self.data.split_at_checked(len)?;
        self.data = rest;
        Some(val)
    }

    /// Safely extracts a fixed-size chunk from the reader, advancing the
    /// internal cursor.
    fn take<const N: usize>(&mut self) -> Option<&[u8; N]> {
        let (chunk, rest) = self.data.split_first_chunk()?;
        self.data = rest;
        Some(chunk)
    }

    /// Reads a single byte.
    pub fn read_u8(&mut self) -> Option<u8> {
        Some(u8::from_be_bytes(*self.take()?))
    }

    /// Reads a u16 in big-endian format.
    pub fn read_u16(&mut self) -> Option<u16> {
        Some(u16::from_be_bytes(*self.take()?))
    }

    /// Reads a u32 in big-endian format.
    pub fn read_u32(&mut self) -> Option<u32> {
        Some(u32::from_be_bytes(*self.take()?))
    }

    /// Reads a TPM2B structure (a 2-byte size prefix followed by that many
    /// bytes) and returns the payload (excluding the size prefix).
    ///
    /// See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=141 for details.
    pub fn read_tpm2b(&mut self) -> Option<&'a [u8]> {
        let size: usize = self.read_u16()?.into();
        self.read_bytes(size)
    }

    /// Reads a TPM2B structure (a 2-byte size prefix followed by that many
    /// bytes) and returns the full slice including the 2-byte size prefix.
    pub fn read_tpm2b_raw(&mut self) -> Option<&'a [u8]> {
        let (&size_bytes, _) = self.data.split_first_chunk::<2>()?;
        let size: usize = u16::from_be_bytes(size_bytes).into();
        self.read_bytes(2 + size)
    }

    /// Consumes and returns all remaining bytes in the reader.
    pub fn read_all(self) -> &'a [u8] {
        self.data
    }
}

/// A helper structure to write structured data to a byte vector.
#[derive(Default)]
pub struct Writer {
    buffer: Vec<u8>,
}

impl Writer {
    /// Creates a new Writer with an empty buffer.
    pub fn new() -> Self {
        Self { buffer: Vec::new() }
    }

    /// Creates a new Writer with the specified capacity.
    pub fn with_capacity(capacity: usize) -> Self {
        Self { buffer: Vec::with_capacity(capacity) }
    }

    /// Writes a single byte.
    pub fn write_u8(&mut self, val: u8) {
        self.buffer.push(val);
    }

    /// Writes a u16 in big-endian format.
    pub fn write_u16(&mut self, val: u16) {
        self.buffer.extend_from_slice(&val.to_be_bytes());
    }

    /// Writes a u32 in big-endian format.
    pub fn write_u32(&mut self, val: u32) {
        self.buffer.extend_from_slice(&val.to_be_bytes());
    }

    /// Writes a slice of bytes.
    pub fn write_bytes(&mut self, val: &[u8]) {
        self.buffer.extend_from_slice(val);
    }

    /// Writes a TPM2B structure (a 2-byte size prefix followed by the payload).
    /// Panics if the payload length exceeds `u16::MAX`.
    pub fn write_tpm2b(&mut self, payload: &[u8]) {
        let len = u16::try_from(payload.len()).expect("payload length exceeds u16::MAX");
        self.write_u16(len);
        self.write_bytes(payload);
    }

    /// Writes a standard TPM command header (tag, size, command code).
    pub fn write_command_header(
        &mut self,
        tag: ffi::TpmSt,
        total_size: usize,
        command_code: ffi::TpmCc,
    ) {
        self.write_u16(tag.repr);
        self.write_u32(u32::try_from(total_size).expect("command size exceeds u32::MAX"));
        self.write_u32(command_code.repr);
    }

    /// Writes password session authorizations with empty passwords.
    pub fn write_password_sessions(&mut self, count: usize) {
        let auth_size =
            u32::try_from(count * TPM_SESSION_SIZE).expect("auth size exceeds u32::MAX");
        self.write_u32(auth_size);
        for _ in 0..count {
            self.write_u32(TpmRh::TPM_RS_PW.repr);
            self.write_u16(0); // nonce size: 0
            self.write_u8(0); // sessionAttributes: 0
            self.write_u16(0); // hmac size: 0
        }
    }

    /// Consumes the Writer and returns the inner byte vector.
    pub fn into_inner(self) -> Vec<u8> {
        self.buffer
    }
}

/// Builds a TPM2_Certify command.
///
/// * `object_handle` - Handle of the object to be certified (the signing key).
/// * `sign_handle` - Handle of the key used to sign the attestation (the AIK).
/// * `qualifying_data` - Data provided by the caller to ensure freshness (e.g.,
///   a challenge).
///
/// Note: This function currently assumes empty password authorizations for both
/// the object and sign handles.
///
/// # Panics
///
/// Panics if `qualifying_data` exceeds `u16::MAX` bytes.
///
/// A TPM Certify command has the following structure (Table 97):
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPMI_ST_COMMAND_TAG | tag            |
/// | UINT32              | commandSize    |
/// | TPM_CC              | commandCode    |
///
/// Handles:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPMI_DH_OBJECT      | objectHandle   |
/// | TPMI_DH_OBJECT+     | signHandle     |
///
/// Parameters:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPM2B_DATA          | qualifyingData |
/// | TPMT_SIG_SCHEME+    | inScheme       |
///
/// See Table 97 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=154.
///
/// Also see https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-1-Architecture_Version-185_pub.pdf#page=97
/// for a general overview of the structure of a TPM command.
pub fn build_certify_command(
    object_handle: u32,
    sign_handle: u32,
    qualifying_data: &[u8],
) -> Vec<u8> {
    let total_size = TPM_HEADER_SIZE
        + (2 * TPM_HANDLE_SIZE)
        + TPM_AUTH_SIZE_SIZE
        + (2 * TPM_SESSION_SIZE)
        + 2
        + qualifying_data.len()
        + 2; // inScheme (Null)

    let mut writer = Writer::with_capacity(total_size);

    // 1. Command Header
    writer.write_command_header(TpmSt::TPM_ST_SESSIONS, total_size, TpmCc::TPM_CC_CERTIFY);

    // 2. Handles
    writer.write_u32(object_handle);
    writer.write_u32(sign_handle);

    // 3. Authorization Area (TPMS_AUTH_COMMAND)
    writer.write_password_sessions(2);

    // 4. Command Parameters
    // qualifyingData (TPM2B_DATA)
    writer.write_tpm2b(qualifying_data);

    // inScheme (TPMT_SIG_SCHEME)
    writer.write_u16(TpmAlgSigScheme::TPM_ALG_NULL.repr);

    writer.into_inner()
}

/// Builds a TPM2_Create command for an Attestation Identity Key (AIK).
///
/// * `parent_handle` - Handle of the parent key under which the AIK is created
///   (e.g., Storage Root Key).
/// * `scheme` - Signing scheme (TPM_ALG_RSASSA, TPM_ALG_RSAPSS, or
///   TPM_ALG_ECDSA).
/// * `hash_alg` - Hash algorithm (TPM_ALG_SHA256, TPM_ALG_SHA384, or
///   TPM_ALG_SHA512).
///
/// Key type and parameters are inferred from the signing scheme:
/// - RSASSA / RSAPSS implies RSA (2048 bits, default exponent).
/// - ECDSA implies ECC (curve inferred from hash algorithm: SHA-256 -> P-256,
///   SHA-384 -> P-384, SHA-512 -> P-521).
///
/// A TPM Create command has the following structure (Table 18 in Part 3):
///
/// Header:
/// | Type                | Name                     |
/// |---------------------|--------------------------|
/// | TPMI_ST_COMMAND_TAG | tag (TPM_ST_SESSIONS)    |
/// | UINT32              | commandSize              |
/// | TPM_CC              | commandCode (TPM_CC_CREATE) |
///
/// Handles:
/// | Type                | Name                     |
/// |---------------------|--------------------------|
/// | TPMI_DH_OBJECT      | parentHandle             |
///
/// Parameters:
/// | Type                | Name                     |
/// |---------------------|--------------------------|
/// | TPM2B_SENSITIVE_CREATE | inSensitive           |
/// | TPM2B_PUBLIC        | inPublic                 |
/// | TPM2B_DATA          | outsideInfo              |
/// | TPML_PCR_SELECTION  | creationPCR              |
///
/// See Table 18 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=67.
pub fn build_create_aik_command(
    parent_handle: u32,
    scheme: TpmAlgSigScheme,
    hash_alg: TpmAlgHash,
) -> Vec<u8> {
    let (key_type, curve_id) = match scheme {
        TpmAlgSigScheme::TPM_ALG_RSASSA | TpmAlgSigScheme::TPM_ALG_RSAPSS => {
            (TpmAlgPublic::TPM_ALG_RSA, None)
        }
        TpmAlgSigScheme::TPM_ALG_ECDSA => {
            let curve = match hash_alg {
                TpmAlgHash::TPM_ALG_SHA256 => TpmEccCurve::TPM_ECC_NIST_P256,
                TpmAlgHash::TPM_ALG_SHA384 => TpmEccCurve::TPM_ECC_NIST_P384,
                TpmAlgHash::TPM_ALG_SHA512 => TpmEccCurve::TPM_ECC_NIST_P521,
                _ => panic!("unsupported hash_alg for ECC in build_create_aik_command"),
            };
            (TpmAlgPublic::TPM_ALG_ECC, Some(curve))
        }
        _ => panic!("unsupported scheme in build_create_aik_command"),
    };
    let in_scheme_size = 4; // 2 bytes scheme + 2 bytes hash_alg
    let public_parms_size = match key_type {
        TpmAlgPublic::TPM_ALG_RSA => 2 + in_scheme_size + 2 + 4,
        TpmAlgPublic::TPM_ALG_ECC => 2 + in_scheme_size + 2 + 2,
        _ => unreachable!(),
    };
    let unique_size = match key_type {
        TpmAlgPublic::TPM_ALG_RSA => 2,
        TpmAlgPublic::TPM_ALG_ECC => 4,
        _ => unreachable!(),
    };
    let tpmt_public_size = 2 // type
        + 2 // nameAlg
        + 4 // objectAttributes
        + 2 // authPolicy size (0)
        + public_parms_size
        + unique_size;

    let in_sensitive_size = 2 // size (4)
        + 2 // userAuth size (0)
        + 2; // data size (0)

    let in_public_size = 2 // size
        + tpmt_public_size;

    let outside_info_size = 2; // size (0)
    let creation_pcr_size = 4; // count (0)

    let total_size = TPM_HEADER_SIZE
        + TPM_HANDLE_SIZE // parentHandle
        + TPM_AUTH_SIZE_SIZE
        + TPM_SESSION_SIZE
        + in_sensitive_size
        + in_public_size
        + outside_info_size
        + creation_pcr_size;

    let mut writer = Writer::with_capacity(total_size);

    // 1. Command Header
    writer.write_command_header(TpmSt::TPM_ST_SESSIONS, total_size, TpmCc::TPM_CC_CREATE);

    // 2. Handles
    writer.write_u32(parent_handle);

    // 3. Authorization Area
    writer.write_password_sessions(1);

    // 4. Command Parameters
    // inSensitive (TPM2B_SENSITIVE_CREATE)
    writer.write_u16(4); // size of TPMS_SENSITIVE_CREATE
    writer.write_u16(0); // userAuth size
    writer.write_u16(0); // data size

    // inPublic (TPM2B_PUBLIC)
    writer.write_u16(u16::try_from(tpmt_public_size).unwrap());
    writer.write_u16(key_type.repr);
    writer.write_u16(hash_alg.repr);
    writer.write_u32(AIK_OBJECT_ATTRIBUTES);
    writer.write_u16(0); // authPolicy (empty TPM2B_DIGEST)

    // parameters (TPMU_PUBLIC_PARMS)
    writer.write_u16(TpmAlgSigScheme::TPM_ALG_NULL.repr); // symmetric
    writer.write_u16(scheme.repr);
    writer.write_u16(hash_alg.repr);
    match key_type {
        TpmAlgPublic::TPM_ALG_RSA => {
            writer.write_u16(2048);
            writer.write_u32(0); // exponent (default 2^16 + 1)
                                 // unique (TPM2B_PUBLIC_KEY_RSA)
            writer.write_u16(0);
        }
        TpmAlgPublic::TPM_ALG_ECC => {
            let curve = curve_id.unwrap();
            writer.write_u16(curve.repr);
            writer.write_u16(TpmAlgSigScheme::TPM_ALG_NULL.repr); // kdf scheme
                                                                  // unique (TPMS_ECC_POINT)
            writer.write_u16(0); // x
            writer.write_u16(0); // y
        }
        _ => unreachable!(),
    }

    // outsideInfo (TPM2B_DATA)
    writer.write_u16(0);

    // creationPCR (TPML_PCR_SELECTION)
    writer.write_u32(0);

    writer.into_inner()
}

/// Represents a TPMS_AUTH_RESPONSE structure
///
/// | Type         | Name               |
/// |--------------|--------------------|
/// | TPM2B_NONCE  | nonce              |
/// | TPMA_SESSION | session_attributes |
/// | TPM2B_AUTH   | hmac               |
///
/// See Table 157 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=162.
///
/// Unused here. We read it to verify the TPM returned the right response size.
#[allow(dead_code)]
struct TpmsAuthResponse<'a> {
    nonce: &'a [u8],
    session_attributes: u8,
    hmac: &'a [u8],
}

impl<'a> TpmsAuthResponse<'a> {
    fn parse(reader: &mut Reader<'a>) -> Result<Self, TpmParseError> {
        let nonce = reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
        let session_attributes = reader.read_u8().ok_or(TpmParseError::BufferTooSmall)?;
        let hmac = reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
        Ok(Self { nonce, session_attributes, hmac })
    }
}

/// Represents a TPMS_ATTEST structure.
///
/// | Type             | Name            |
/// |------------------|-----------------|
/// | TPM_CONSTANTS32  | magic           |
/// | TPMI_ST_ATTEST   | type            |
/// | TPM2B_NAME       | qualifiedSigner |
/// | TPM2B_DATA       | extraData       |
/// | TPMS_CLOCK_INFO  | clockInfo       |
/// | UINT64           | firmwareVersion |
/// | TPMU_ATTEST      | attested        |
///
/// See Table 154 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=162.
struct TpmsAttest<'a> {
    pub magic: TpmConstant,
    pub type_: TpmSt,
    // This field must be parsed to correctly advance the buffer offset according to the TPM 2.0
    // spec, but its value is currently unused.
    #[allow(dead_code)]
    pub qualified_signer: &'a [u8],
    pub extra_data: &'a [u8],
}

impl<'a> TpmsAttest<'a> {
    /// Parses a TPMS_ATTEST from the reader.
    fn parse(reader: &mut Reader<'a>) -> Result<Self, TpmParseError> {
        // Read the magic number (should be TPM_GENERATED_VALUE)
        let magic = reader
            .read_u32()
            .map(|repr| TpmConstant { repr })
            .ok_or(TpmParseError::BufferTooSmall)?;
        // Read the attestation type (e.g., TPM_ST_ATTEST_CERTIFY)
        let type_ =
            reader.read_u16().map(|repr| TpmSt { repr }).ok_or(TpmParseError::BufferTooSmall)?;
        // Read the qualified signer name (Name of the object that signed the
        // attestation)
        let qualified_signer = reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
        // Read the extra data (often contains a nonce for freshness)
        let extra_data = reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;

        // Clock info and firmware version are part of TPMS_CLOCK_INFO and are standard
        // trailing fields in all TPMS_ATTEST structures. We read them to advance the
        // cursor.
        let _clock_info = reader.read_bytes(17).ok_or(TpmParseError::BufferTooSmall)?;
        let _firmware_version = reader.read_bytes(8).ok_or(TpmParseError::BufferTooSmall)?;

        // For certify attestations, there are additional fields: the certified object's
        // Name and Qualified Name. We read them to ensure the buffer is fully parsed.
        if type_ == TpmSt::TPM_ST_ATTEST_CERTIFY {
            let _name = reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
            let _qualified_name = reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
        }

        // Ensure the entire buffer for this struct was parsed exactly.
        // If there's data left, the format is unexpected or corrupted.
        reader.ensure_empty()?;

        Ok(Self { magic, type_, qualified_signer, extra_data })
    }
}
/// Internal function to parse a certify response.
/// Returns the attestation statement and signature bytes on success.
struct CertifyData<'a> {
    statement: &'a [u8], // Serialized TPMS_ATTEST
    signature: &'a [u8], // Serialized TPMT_SIGNATURE
}

/// Parse a TPM2_Certify response.
///
/// Header:
///
/// | Type   | Name         |
/// |--------|--------------|
/// | TPM_ST | tag          |
/// | UINT32 | responseSize |
/// | TPM_RC | responseCode |
///
/// Parameters:
///
/// | Type           | Name        |
/// |----------------|-------------|
/// | TPM2B_ATTEST   | certifyInfo |
/// | TPMT_SIGNATURE | signature   |
///
/// See Table 98 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=154.
///
/// Also see https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-1-Architecture_Version-185_pub.pdf#page=97
/// for a general overview of the structure of a TPM response.
fn parse_certify_response_impl<'a>(
    resp: &'a [u8],
    expected_extra_data: &[u8],
) -> Result<CertifyData<'a>, TpmParseError> {
    let mut reader = Reader::new(resp);
    let header = reader.read_response_header(resp.len())?;

    // Determine the size of the parameters section
    let parameter_size = if header.tag == TpmSt::TPM_ST_SESSIONS {
        reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?.try_into().unwrap()
    } else if header.tag == TpmSt::TPM_ST_NO_SESSIONS {
        header.response_size - TPM_HEADER_SIZE
    } else {
        return Err(TpmParseError::WrongType);
    };
    // Create a sub-reader specifically for the parameters section
    let mut param_reader =
        Reader::new(reader.read_bytes(parameter_size).ok_or(TpmParseError::BufferTooSmall)?);

    // Read the inner TPMS_ATTEST structure bytes (size-prefixed in the protocol)
    let statement = param_reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;

    // The remaining data in the parameters section is the signature
    // (TPMT_SIGNATURE)
    // Read the signature algorithm (e.g., TPM_ALG_RSASSA or TPM_ALG_ECDSA)
    // without advancing the reader, so we can return the entire TPMT_SIGNATURE.
    // The entire rest of the parameter section is treated as the signature
    let signature = param_reader.read_all();
    // Sanity check that the signature at least contains the algorithms
    let _algs = SignatureAlgorithms::parse(&mut Reader::new(signature))
        .ok_or(TpmParseError::BufferTooSmall)?;

    // The remaining bytes in the main reader are the response authorization
    // sessions.
    // TPM2_Certify requires two handles (objectHandle and signHandle), so we expect
    // exactly two authorization sessions in the response.
    if header.tag == TpmSt::TPM_ST_SESSIONS {
        let _session1 = TpmsAuthResponse::parse(&mut reader)?;
        let _session2 = TpmsAuthResponse::parse(&mut reader)?;
    }

    reader.ensure_empty()?;

    // Parse the TPMS_ATTEST structure
    let mut attest_reader = Reader::new(statement);
    let attest_info = TpmsAttest::parse(&mut attest_reader)?;

    // Validate the magic number to ensure it's a TPM-generated structure
    if attest_info.magic != TpmConstant::TPM_GENERATED_VALUE {
        return Err(TpmParseError::BadMagicNumber);
    }
    // Ensure this is specifically a certify attestation
    if attest_info.type_ != TpmSt::TPM_ST_ATTEST_CERTIFY {
        return Err(TpmParseError::WrongType);
    }
    // Verify the extra data matches to prevent replay attacks
    if attest_info.extra_data != expected_extra_data {
        return Err(TpmParseError::ChallengeMismatch);
    }

    Ok(CertifyData { statement, signature })
}

impl From<TpmParseError> for ffi::CertifyResponse {
    fn from(err: TpmParseError) -> Self {
        ffi::CertifyResponse { status: err.into(), statement: Vec::new(), signature: Vec::new() }
    }
}

impl<'a> From<Result<CertifyData<'a>, TpmParseError>> for ffi::CertifyResponse {
    fn from(result: Result<CertifyData<'a>, TpmParseError>) -> Self {
        match result {
            Ok(resp) => ffi::CertifyResponse {
                status: ffi::ResponseStatus::OK,
                statement: resp.statement.to_vec(),
                signature: resp.signature.to_vec(),
            },
            Err(err) => err.into(),
        }
    }
}

/// Parses a TPM2_Certify response.
///
/// This function reads the response buffer from a TPM2_Certify command,
/// validates the headers, and extracts the attestation statement and signature.
/// It also verifies that the response is for a certify command, checks the
/// magic number, and ensures `expected_extra_data` matches the `extra_data`
/// field in the attestation to prevent replay attacks (TPM2_Certify operates on
/// `TPM2B_DATA qualifyingData`, which for key attestation protocols is
/// typically the SHA-256 digest of the challenge).
///
/// # Arguments
///
/// * `resp` - The raw byte response from the TPM2_Certify command.
/// * `expected_extra_data` - The extra data expected in the attestation's
///   `extra_data` field (e.g., the SHA-256 digest of the challenge).
///
/// # Returns
///
/// A `CertifyResponse` containing the parsing result, any TPM error code,
/// the serialized `TPMS_ATTEST` statement, and the serialized `TPMT_SIGNATURE`.
pub fn parse_certify_response(resp: &[u8], expected_extra_data: &[u8]) -> ffi::CertifyResponse {
    parse_certify_response_impl(resp, expected_extra_data).into()
}

struct CreateData<'a> {
    out_private: &'a [u8],
    out_public: &'a [u8],
}

fn parse_create_response_impl<'a>(resp: &'a [u8]) -> Result<CreateData<'a>, TpmParseError> {
    let mut reader = Reader::new(resp);
    let header = reader.read_response_header(resp.len())?;

    let parameter_size = if header.tag == TpmSt::TPM_ST_SESSIONS {
        reader
            .read_u32()
            .ok_or(TpmParseError::BufferTooSmall)?
            .try_into()
            .map_err(|_| TpmParseError::BufferTooSmall)?
    } else if header.tag == TpmSt::TPM_ST_NO_SESSIONS {
        header.response_size - TPM_HEADER_SIZE
    } else {
        return Err(TpmParseError::WrongType);
    };

    let mut param_reader =
        Reader::new(reader.read_bytes(parameter_size).ok_or(TpmParseError::BufferTooSmall)?);

    let out_private = param_reader.read_tpm2b_raw().ok_or(TpmParseError::BufferTooSmall)?;
    let out_public = param_reader.read_tpm2b_raw().ok_or(TpmParseError::BufferTooSmall)?;
    let _creation_data = param_reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
    let _creation_hash = param_reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;

    // Consume and validate creationTicket:
    // - tag: TPM_ST_CREATION (0x8021)
    // - hierarchy: TPMI_RH_HIERARCHY (u32)
    // - digest: TPM2B_DIGEST
    let ticket_tag = param_reader.read_u16().ok_or(TpmParseError::BufferTooSmall)?;
    if ticket_tag != TpmSt::TPM_ST_CREATION.repr {
        return Err(TpmParseError::WrongType);
    }
    let _hierarchy = param_reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?;
    let _digest = param_reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;

    if header.tag == TpmSt::TPM_ST_SESSIONS {
        let _session = TpmsAuthResponse::parse(&mut reader)?;
    }

    reader.ensure_empty()?;

    Ok(CreateData { out_private, out_public })
}

impl From<TpmParseError> for ffi::CreateResponse {
    fn from(err: TpmParseError) -> Self {
        ffi::CreateResponse { status: err.into(), out_private: Vec::new(), out_public: Vec::new() }
    }
}

impl<'a> From<Result<CreateData<'a>, TpmParseError>> for ffi::CreateResponse {
    fn from(result: Result<CreateData<'a>, TpmParseError>) -> Self {
        match result {
            Ok(data) => ffi::CreateResponse {
                status: ffi::ResponseStatus::OK,
                out_private: data.out_private.to_vec(),
                out_public: data.out_public.to_vec(),
            },
            Err(err) => err.into(),
        }
    }
}

/// Parses a TPM2_Create response.
///
/// This function reads the response buffer from a TPM2_Create command,
/// validates the response headers and creation ticket, and extracts the
/// private and public key areas.
///
/// # Arguments
///
/// * `resp` - The raw byte response from the TPM2_Create command.
///
/// # Returns
///
/// A `CreateResponse` containing the parsing result, any TPM error code,
/// the serialized `TPM2B_PRIVATE` structure, and the serialized
/// `TPM2B_PUBLIC` structure.
pub fn parse_create_response(resp: &[u8]) -> ffi::CreateResponse {
    parse_create_response_impl(resp).into()
}

/// Enum representing the signature data for different algorithms.
enum SignatureData<'a> {
    Rsa(&'a [u8]),
    Ecdsa { r: &'a [u8], s: &'a [u8] },
}

/// Information about algorithms used in a TPM signature.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SignatureAlgorithms {
    pub sig_alg: TpmAlgSigScheme,
    pub hash_alg: TpmAlgHash,
}

impl SignatureAlgorithms {
    pub fn parse(reader: &mut Reader<'_>) -> Option<Self> {
        let sig_alg = reader.read_u16().map(|repr| TpmAlgSigScheme { repr })?;
        let hash_alg = reader.read_u16().map(|repr| TpmAlgHash { repr })?;
        Some(Self { sig_alg, hash_alg })
    }
}

/// Represents a TPMT_SIGNATURE structure.
/// See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=187 for details.
struct TpmtSignature<'a> {
    pub algorithms: SignatureAlgorithms,
    pub signature_data: SignatureData<'a>,
}

impl<'a> TpmtSignature<'a> {
    /// Parses a TPMT_SIGNATURE from the reader.
    fn parse(reader: &mut Reader<'a>) -> Result<Self, TpmSignatureParseError> {
        let algorithms =
            SignatureAlgorithms::parse(reader).ok_or(TpmSignatureParseError::BufferTooSmall)?;

        let signature_data = match algorithms.sig_alg {
            TpmAlgSigScheme::TPM_ALG_RSASSA | TpmAlgSigScheme::TPM_ALG_RSAPSS => {
                let rsa_sig = reader.read_tpm2b().ok_or(TpmSignatureParseError::BufferTooSmall)?;
                SignatureData::Rsa(rsa_sig)
            }
            TpmAlgSigScheme::TPM_ALG_ECDSA => {
                let r = reader.read_tpm2b().ok_or(TpmSignatureParseError::BufferTooSmall)?;
                let s = reader.read_tpm2b().ok_or(TpmSignatureParseError::BufferTooSmall)?;
                SignatureData::Ecdsa { r, s }
            }
            _ => {
                return Err(TpmSignatureParseError::UnsupportedSignatureAlgorithm);
            }
        };

        Ok(Self { algorithms, signature_data })
    }
}

/// Parses a serialized `TPMT_SIGNATURE` and returns its raw components.
pub fn parse_tpm_signature(signature: &[u8]) -> ffi::RawSignatureComponents {
    match parse_tpm_signature_impl(signature) {
        Ok(components) => components,
        Err(err) => ffi::RawSignatureComponents {
            status: match err {
                TpmSignatureParseError::BufferTooSmall => ffi::SignatureParseResult::BufferTooSmall,
                TpmSignatureParseError::TrailingBytes => ffi::SignatureParseResult::TrailingBytes,
                TpmSignatureParseError::UnsupportedSignatureAlgorithm => {
                    ffi::SignatureParseResult::UnsupportedSignatureAlgorithm
                }
            },
            sig_alg: ffi::TpmAlgSigScheme::TPM_ALG_NULL,
            hash_alg: ffi::TpmAlgHash::TPM_ALG_SHA256,
            rsa_sig: Vec::new(),
            ecdsa_r: Vec::new(),
            ecdsa_s: Vec::new(),
        },
    }
}

fn parse_tpm_signature_impl(
    signature: &[u8],
) -> Result<ffi::RawSignatureComponents, TpmSignatureParseError> {
    let mut sig_reader = Reader::new(signature);
    let tpm_sig = TpmtSignature::parse(&mut sig_reader)?;

    // Reject trailing garbage after the signature.
    if !sig_reader.is_empty() {
        return Err(TpmSignatureParseError::TrailingBytes);
    }

    let (rsa_sig, ecdsa_r, ecdsa_s) = match tpm_sig.signature_data {
        SignatureData::Rsa(sig) => (sig.to_vec(), Vec::new(), Vec::new()),
        SignatureData::Ecdsa { r, s } => (Vec::new(), r.to_vec(), s.to_vec()),
    };

    Ok(ffi::RawSignatureComponents {
        status: ffi::SignatureParseResult::Ok,
        sig_alg: tpm_sig.algorithms.sig_alg,
        hash_alg: tpm_sig.algorithms.hash_alg,
        rsa_sig,
        ecdsa_r,
        ecdsa_s,
    })
}

/// Builds a TPM2_Hash command.
///
/// Note: This command builder sets the ticket hierarchy to `TPM_RH_OWNER`,
/// matching the Storage hierarchy used by Chromium's unexportable keys.
pub fn build_hash_command(data: &[u8], hash_alg: TpmAlgHash) -> Vec<u8> {
    assert!(
        data.len() <= TPM_MAX_BUFFER_SIZE,
        "TPM2_Hash data exceeds TPM_MAX_BUFFER_SIZE ({} bytes)",
        TPM_MAX_BUFFER_SIZE
    );
    let total_size = TPM_HEADER_SIZE
        + 2 // data size prefix
        + data.len()
        + 2 // hashAlg
        + 4; // hierarchy

    let mut writer = Writer::with_capacity(total_size);

    // 1. Command Header
    writer.write_command_header(TpmSt::TPM_ST_NO_SESSIONS, total_size, TpmCc::TPM_CC_HASH);

    // 2. Command Parameters
    writer.write_tpm2b(data);
    writer.write_u16(hash_alg.repr);
    writer.write_u32(TpmRh::TPM_RH_OWNER.repr);

    writer.into_inner()
}

/// Represents a TPMT_TK_HASHCHECK ticket structure.
///
/// | Type              | Name      |
/// |-------------------|-----------|
/// | TPMI_ST_CHECK     | tag       |
/// | TPMI_RH_HIERARCHY | hierarchy |
/// | TPM2B_DIGEST      | digest    |
///
/// See Table 115 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=148.
#[allow(dead_code)]
struct TpmtTkHashcheck<'a> {
    pub tag: TpmSt,
    pub hierarchy: TpmRh,
    pub digest: &'a [u8],
}

impl<'a> TpmtTkHashcheck<'a> {
    fn parse(reader: &mut Reader<'a>) -> Result<Self, TpmParseError> {
        let tag =
            reader.read_u16().map(|repr| TpmSt { repr }).ok_or(TpmParseError::BufferTooSmall)?;
        if tag != TpmSt::TPM_ST_HASHCHECK {
            return Err(TpmParseError::WrongType);
        }
        let hierarchy =
            reader.read_u32().map(|repr| TpmRh { repr }).ok_or(TpmParseError::BufferTooSmall)?;
        let digest = reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
        reader.ensure_empty()?;
        Ok(Self { tag, hierarchy, digest })
    }
}

struct HashData<'a> {
    digest: &'a [u8],
    validation: &'a [u8],
}

fn parse_hash_response_impl<'a>(resp: &'a [u8]) -> Result<HashData<'a>, TpmParseError> {
    let mut reader = Reader::new(resp);
    let header = reader.read_response_header(resp.len())?;
    if header.tag != TpmSt::TPM_ST_NO_SESSIONS {
        return Err(TpmParseError::WrongType);
    }

    let parameter_size = header.response_size - TPM_HEADER_SIZE;
    let mut param_reader =
        Reader::new(reader.read_bytes(parameter_size).ok_or(TpmParseError::BufferTooSmall)?);

    let digest = param_reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
    let validation = param_reader.read_all();

    let _ticket = TpmtTkHashcheck::parse(&mut Reader::new(validation))?;

    reader.ensure_empty()?;

    Ok(HashData { digest, validation })
}

/// Builds a TPM2_Sign command.
pub fn build_sign_command(key_handle: u32, digest: &[u8], validation_ticket: &[u8]) -> Vec<u8> {
    let total_size = TPM_HEADER_SIZE
        + TPM_HANDLE_SIZE
        + TPM_AUTH_SIZE_SIZE
        + TPM_SESSION_SIZE
        + 2 // digest size prefix
        + digest.len()
        + 2 // inScheme: TPM_ALG_NULL (2 bytes)
        + validation_ticket.len();

    let mut writer = Writer::with_capacity(total_size);

    // 1. Command Header
    writer.write_command_header(TpmSt::TPM_ST_SESSIONS, total_size, TpmCc::TPM_CC_SIGN);

    // 2. Handles
    writer.write_u32(key_handle);

    // 3. Authorization Area
    writer.write_password_sessions(1);

    // 4. Command Parameters
    writer.write_tpm2b(digest);
    writer.write_u16(TpmAlgSigScheme::TPM_ALG_NULL.repr);
    writer.write_bytes(validation_ticket);

    writer.into_inner()
}

fn parse_sign_response_impl(resp: &[u8]) -> Result<&[u8], TpmParseError> {
    let mut reader = Reader::new(resp);
    let header = reader.read_response_header(resp.len())?;

    let parameter_size = if header.tag == TpmSt::TPM_ST_SESSIONS {
        reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?.try_into().unwrap()
    } else if header.tag == TpmSt::TPM_ST_NO_SESSIONS {
        header.response_size - TPM_HEADER_SIZE
    } else {
        return Err(TpmParseError::WrongType);
    };

    let signature = reader.read_bytes(parameter_size).ok_or(TpmParseError::BufferTooSmall)?;

    let _algs = SignatureAlgorithms::parse(&mut Reader::new(signature))
        .ok_or(TpmParseError::BufferTooSmall)?;

    if header.tag == TpmSt::TPM_ST_SESSIONS {
        let _session1 = TpmsAuthResponse::parse(&mut reader)?;
    }

    reader.ensure_empty()?;

    Ok(signature)
}

impl From<TpmParseError> for ffi::HashResponse {
    fn from(err: TpmParseError) -> Self {
        ffi::HashResponse { status: err.into(), digest: Vec::new(), validation_ticket: Vec::new() }
    }
}

impl<'a> From<Result<HashData<'a>, TpmParseError>> for ffi::HashResponse {
    fn from(result: Result<HashData<'a>, TpmParseError>) -> Self {
        match result {
            Ok(data) => ffi::HashResponse {
                status: ffi::ResponseStatus::OK,
                digest: data.digest.to_vec(),
                validation_ticket: data.validation.to_vec(),
            },
            Err(err) => err.into(),
        }
    }
}

impl From<TpmParseError> for ffi::SignResponse {
    fn from(err: TpmParseError) -> Self {
        ffi::SignResponse { status: err.into(), signature: Vec::new() }
    }
}

impl<'a> From<Result<&'a [u8], TpmParseError>> for ffi::SignResponse {
    fn from(result: Result<&'a [u8], TpmParseError>) -> Self {
        match result {
            Ok(sig) => {
                ffi::SignResponse { status: ffi::ResponseStatus::OK, signature: sig.to_vec() }
            }
            Err(err) => err.into(),
        }
    }
}

/// Parses a TPM2_Hash response.
///
/// This function reads the response buffer from a TPM2_Hash command,
/// validates the header, and extracts the digest and validation ticket.
///
/// Note that if the TPM returns an error code (`tpm_response_code != 0`),
/// parsing stops early after reading the header. In that case, `digest` and
/// `validation_ticket` in the returned `HashResponse` will be empty Vecs.
pub fn parse_hash_response(resp: &[u8]) -> ffi::HashResponse {
    parse_hash_response_impl(resp).into()
}

pub fn parse_sign_response(resp: &[u8]) -> ffi::SignResponse {
    parse_sign_response_impl(resp).into()
}

/// Builds a TPM2_HashSequenceStart command.
///
/// * `hash_alg` - The hash algorithm to use for the sequence.
///
/// Note: This function sets an empty authorization value for the sequence.
///
/// A TPM HashSequenceStart command has the following structure (Table 85):
///
/// Header:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPMI_ST_COMMAND_TAG | tag            |
/// | UINT32              | commandSize    |
/// | TPM_CC              | commandCode    |
///
/// Parameters:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPM2B_AUTH          | auth           |
/// | TPMI_ALG_HASH       | hashAlg        |
///
/// See Table 85 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=140.
///
/// Also see https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-1-Architecture_Version-185_pub.pdf#page=97
/// for a general overview of the structure of a TPM command.
pub fn build_hash_sequence_start_command(hash_alg: TpmAlgHash) -> Vec<u8> {
    let total_size = TPM_HEADER_SIZE
        + 2 // auth size prefix (0x0000)
        + 2; // hashAlg

    let mut writer = Writer::with_capacity(total_size);

    // 1. Command Header
    writer.write_command_header(
        TpmSt::TPM_ST_NO_SESSIONS,
        total_size,
        TpmCc::TPM_CC_HASH_SEQUENCE_START,
    );

    // 2. Command Parameters
    writer.write_u16(0); // empty auth (TPM2B_AUTH with size 0)
    writer.write_u16(hash_alg.repr);

    writer.into_inner()
}

/// Parses a TPM2_HashSequenceStart response.
///
/// Header:
///
/// | Type   | Name         |
/// |--------|--------------|
/// | TPM_ST | tag          |
/// | UINT32 | responseSize |
/// | TPM_RC | responseCode |
///
/// Handles:
///
/// | Type           | Name           |
/// |----------------|----------------|
/// | TPMI_DH_OBJECT | sequenceHandle |
///
/// See Table 86 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=140.
///
/// Also see https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-1-Architecture_Version-185_pub.pdf#page=97
/// for a general overview of the structure of a TPM response.
fn parse_hash_sequence_start_response_impl(resp: &[u8]) -> Result<u32, TpmParseError> {
    let mut reader = Reader::new(resp);
    let header = reader.read_response_header(resp.len())?;
    if header.tag != TpmSt::TPM_ST_NO_SESSIONS {
        return Err(TpmParseError::WrongType);
    }

    let sequence_handle = reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?;

    reader.ensure_empty()?;

    Ok(sequence_handle)
}

impl From<TpmParseError> for ffi::HashSequenceStartResponse {
    fn from(err: TpmParseError) -> Self {
        ffi::HashSequenceStartResponse { status: err.into(), sequence_handle: 0 }
    }
}

impl From<Result<u32, TpmParseError>> for ffi::HashSequenceStartResponse {
    fn from(result: Result<u32, TpmParseError>) -> Self {
        match result {
            Ok(sequence_handle) => {
                ffi::HashSequenceStartResponse { status: ffi::ResponseStatus::OK, sequence_handle }
            }
            Err(err) => err.into(),
        }
    }
}

/// Parses a TPM2_HashSequenceStart response.
pub fn parse_hash_sequence_start_response(resp: &[u8]) -> ffi::HashSequenceStartResponse {
    parse_hash_sequence_start_response_impl(resp).into()
}

/// Builds a TPM2_SequenceUpdate command.
///
/// * `sequence_handle` - Handle of the sequence object.
/// * `data` - Data to be added to the sequence hash.
///
/// Note: This function assumes empty password authorization for the sequence
/// handle.
///
/// # Panics
///
/// Panics if `data` exceeds `TPM_MAX_BUFFER_SIZE` bytes.
///
/// A TPM SequenceUpdate command has the following structure (Table 91):
///
/// Header:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPMI_ST_COMMAND_TAG | tag            |
/// | UINT32              | commandSize    |
/// | TPM_CC              | commandCode    |
///
/// Handles:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPMI_DH_OBJECT      | sequenceHandle |
///
/// Parameters:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPM2B_MAX_BUFFER    | buffer         |
///
/// See Table 91 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=146.
///
/// Also see https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-1-Architecture_Version-185_pub.pdf#page=97
/// for a general overview of the structure of a TPM command.
pub fn build_sequence_update_command(sequence_handle: u32, data: &[u8]) -> Vec<u8> {
    assert!(
        data.len() <= TPM_MAX_BUFFER_SIZE,
        "TPM2_SequenceUpdate data exceeds TPM_MAX_BUFFER_SIZE ({} bytes)",
        TPM_MAX_BUFFER_SIZE
    );
    let total_size = TPM_HEADER_SIZE
        + TPM_HANDLE_SIZE
        + TPM_AUTH_SIZE_SIZE
        + TPM_SESSION_SIZE
        + 2 // data size prefix
        + data.len();

    let mut writer = Writer::with_capacity(total_size);

    // 1. Command Header
    writer.write_command_header(TpmSt::TPM_ST_SESSIONS, total_size, TpmCc::TPM_CC_SEQUENCE_UPDATE);

    // 2. Handles
    writer.write_u32(sequence_handle);

    // 3. Authorization Area
    writer.write_password_sessions(1);

    // 4. Command Parameters
    writer.write_tpm2b(data);

    writer.into_inner()
}

/// Parses a TPM2_SequenceUpdate response.
///
/// Header:
///
/// | Type   | Name         |
/// |--------|--------------|
/// | TPM_ST | tag          |
/// | UINT32 | responseSize |
/// | TPM_RC | responseCode |
///
/// See Table 92 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=146.
///
/// Also see https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-1-Architecture_Version-185_pub.pdf#page=97
/// for a general overview of the structure of a TPM response.
fn parse_sequence_update_response_impl(resp: &[u8]) -> Result<(), TpmParseError> {
    let mut reader = Reader::new(resp);
    let header = reader.read_response_header(resp.len())?;
    if header.tag != TpmSt::TPM_ST_SESSIONS {
        return Err(TpmParseError::WrongType);
    }

    let parameter_size: usize = reader
        .read_u32()
        .ok_or(TpmParseError::BufferTooSmall)?
        .try_into()
        .map_err(|_| TpmParseError::BufferTooSmall)?;

    if parameter_size != 0 {
        return Err(TpmParseError::TrailingBytes);
    }

    let _session = TpmsAuthResponse::parse(&mut reader)?;

    reader.ensure_empty()?;

    Ok(())
}

/// Parses a TPM2_SequenceUpdate response.
pub fn parse_sequence_update_response(resp: &[u8]) -> ffi::ResponseStatus {
    parse_sequence_update_response_impl(resp).into()
}

/// Builds a TPM2_SequenceComplete command.
///
/// * `sequence_handle` - Handle of the sequence object.
/// * `data` - Data to be added to the sequence hash.
///
/// Note: This function assumes empty password authorization for the sequence
/// handle and sets the ticket hierarchy to `TPM_RH_OWNER`, matching the Storage
/// hierarchy used by Chromium's unexportable keys.
///
/// # Panics
///
/// Panics if `data` exceeds `TPM_MAX_BUFFER_SIZE` bytes.
///
/// A TPM SequenceComplete command has the following structure (Table 93):
///
/// Header:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPMI_ST_COMMAND_TAG | tag            |
/// | UINT32              | commandSize    |
/// | TPM_CC              | commandCode    |
///
/// Handles:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPMI_DH_OBJECT      | sequenceHandle |
///
/// Parameters:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPM2B_MAX_BUFFER    | buffer         |
/// | TPMI_RH_HIERARCHY   | hierarchy      |
///
/// See Table 93 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=148.
///
/// Also see https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-1-Architecture_Version-185_pub.pdf#page=97
/// for a general overview of the structure of a TPM command.
pub fn build_sequence_complete_command(sequence_handle: u32, data: &[u8]) -> Vec<u8> {
    assert!(
        data.len() <= TPM_MAX_BUFFER_SIZE,
        "TPM2_SequenceComplete data exceeds TPM_MAX_BUFFER_SIZE ({} bytes)",
        TPM_MAX_BUFFER_SIZE
    );
    let total_size = TPM_HEADER_SIZE
        + TPM_HANDLE_SIZE
        + TPM_AUTH_SIZE_SIZE
        + TPM_SESSION_SIZE
        + 2 // data size prefix
        + data.len()
        + TPM_HANDLE_SIZE; // hierarchy

    let mut writer = Writer::with_capacity(total_size);

    // 1. Command Header
    writer.write_command_header(
        TpmSt::TPM_ST_SESSIONS,
        total_size,
        TpmCc::TPM_CC_SEQUENCE_COMPLETE,
    );

    // 2. Handles
    writer.write_u32(sequence_handle);

    // 3. Authorization Area
    writer.write_password_sessions(1);

    // 4. Command Parameters
    writer.write_tpm2b(data);
    writer.write_u32(TpmRh::TPM_RH_OWNER.repr);

    writer.into_inner()
}

/// Parses a TPM2_SequenceComplete response.
///
/// Header:
///
/// | Type   | Name         |
/// |--------|--------------|
/// | TPM_ST | tag          |
/// | UINT32 | responseSize |
/// | TPM_RC | responseCode |
///
/// Parameters:
///
/// | Type               | Name             |
/// |--------------------|------------------|
/// | TPM2B_DIGEST       | result           |
/// | TPMT_TK_HASHCHECK  | validation       |
///
/// See Table 94 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=148.
///
/// Also see https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-1-Architecture_Version-185_pub.pdf#page=97
/// for a general overview of the structure of a TPM response.
fn parse_sequence_complete_response_impl<'a>(
    resp: &'a [u8],
) -> Result<HashData<'a>, TpmParseError> {
    let mut reader = Reader::new(resp);
    let header = reader.read_response_header(resp.len())?;
    if header.tag != TpmSt::TPM_ST_SESSIONS {
        return Err(TpmParseError::WrongType);
    }

    let parameter_size: usize = reader
        .read_u32()
        .ok_or(TpmParseError::BufferTooSmall)?
        .try_into()
        .map_err(|_| TpmParseError::BufferTooSmall)?;

    let mut param_reader =
        Reader::new(reader.read_bytes(parameter_size).ok_or(TpmParseError::BufferTooSmall)?);

    let digest = param_reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
    let validation = param_reader.read_all();

    let _ticket = TpmtTkHashcheck::parse(&mut Reader::new(validation))?;

    let _session = TpmsAuthResponse::parse(&mut reader)?;

    reader.ensure_empty()?;

    Ok(HashData { digest, validation })
}

/// Parses a TPM2_SequenceComplete response.
pub fn parse_sequence_complete_response(resp: &[u8]) -> ffi::HashResponse {
    parse_sequence_complete_response_impl(resp).into()
}

/// Builds a TPM2_FlushContext command.
///
/// * `handle` - The handle of the item to flush.
///
/// A TPM FlushContext command has the following structure (Table 164):
///
/// Header:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPMI_ST_COMMAND_TAG | tag            |
/// | UINT32              | commandSize    |
/// | TPM_CC              | commandCode    |
///
/// Parameters:
///
/// | Type                | Name           |
/// |---------------------|----------------|
/// | TPMI_DH_CONTEXT     | flushHandle    |
///
/// See Table 164 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=236.
///
/// Also see https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-1-Architecture_Version-185_pub.pdf#page=97
/// for a general overview of the structure of a TPM command.
pub fn build_flush_context_command(handle: u32) -> Vec<u8> {
    let total_size = TPM_HEADER_SIZE + TPM_HANDLE_SIZE;

    let mut writer = Writer::with_capacity(total_size);

    // 1. Command Header
    writer.write_command_header(TpmSt::TPM_ST_NO_SESSIONS, total_size, TpmCc::TPM_CC_FLUSH_CONTEXT);

    // 2. Command Parameters
    writer.write_u32(handle);

    writer.into_inner()
}

/// Parses a TPM2_FlushContext response.
///
/// Header:
///
/// | Type   | Name         |
/// |--------|--------------|
/// | TPM_ST | tag          |
/// | UINT32 | responseSize |
/// | TPM_RC | responseCode |
///
/// See Table 165 in https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=236.
///
/// Also see https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-1-Architecture_Version-185_pub.pdf#page=97
/// for a general overview of the structure of a TPM response.
fn parse_flush_context_response_impl(resp: &[u8]) -> Result<(), TpmParseError> {
    let mut reader = Reader::new(resp);
    let header = reader.read_response_header(resp.len())?;
    if header.tag != TpmSt::TPM_ST_NO_SESSIONS {
        return Err(TpmParseError::WrongType);
    }

    reader.ensure_empty()?;

    Ok(())
}

/// Parses a TPM2_FlushContext response.
pub fn parse_flush_context_response(resp: &[u8]) -> ffi::ResponseStatus {
    parse_flush_context_response_impl(resp).into()
}

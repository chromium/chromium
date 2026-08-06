// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// TPM Constants. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=41 for details.
/// TPM_GENERATED_VALUE is the magic number in TPM generated structures.
pub const TPM_GENERATED_VALUE: u32 = ffi::TpmConstant::TPM_GENERATED_VALUE.repr;

// TPM Command Codes. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=47 for details.
/// TPM_CC_CERTIFY is the command code for TPM2_Certify.
pub const TPM_CC_CERTIFY: u32 = ffi::TpmCc::TPM_CC_CERTIFY.repr;
/// TPM_CC_HASH is the command code for TPM2_Hash.
pub const TPM_CC_HASH: u32 = ffi::TpmCc::TPM_CC_HASH.repr;
/// TPM_CC_SIGN is the command code for TPM2_Sign.
pub const TPM_CC_SIGN: u32 = ffi::TpmCc::TPM_CC_SIGN.repr;

// TPM Structure Tags. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=65 for details.
/// TPM_ST_NO_SESSIONS indicates that the command has no sessions.
pub const TPM_ST_NO_SESSIONS: u16 = ffi::TpmSt::TPM_ST_NO_SESSIONS.repr;
/// TPM_ST_SESSIONS indicates that the command has sessions.
pub const TPM_ST_SESSIONS: u16 = ffi::TpmSt::TPM_ST_SESSIONS.repr;
/// TPM_ST_ATTEST_CERTIFY is the tag for a certify attestation statement.
pub const TPM_ST_ATTEST_CERTIFY: u16 = ffi::TpmSt::TPM_ST_ATTEST_CERTIFY.repr;
/// TPM_ST_HASHCHECK is the tag for a hashcheck validation ticket.
pub const TPM_ST_HASHCHECK: u16 = ffi::TpmSt::TPM_ST_HASHCHECK.repr;

// TPM Handles. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=88 for details.
/// TPM_RH_OWNER is the handle for the storage hierarchy (0x40000001). This is
/// used for standard keys and as the hierarchy for validation tickets in unit
/// tests.
pub const TPM_RH_OWNER: u32 = 0x40000001;
/// TPM_RH_ENDORSEMENT is the handle for the endorsement hierarchy (0x4000000B).
/// This MUST be used for Windows Attestation Identity Keys (AIKs), because AIKs
/// belong to the endorsement hierarchy.
pub const TPM_RH_ENDORSEMENT: u32 = 0x4000000B;
/// TPM_RS_PW is the handle for a password session.
pub const TPM_RS_PW: u32 = 0x40000009;

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
    // LINT.ThenChange(//crypto/tpm_parser.h:TpmCertifyParseResult)

    /// Response from parsing a TPM2_Certify command.
    #[cxx_name = "RawCertifyResponse"]
    struct CertifyResponse {
        /// The outcome of the parsing operation.
        result: ParseResult,
        /// The TPM response code, if the TPM returned an error.
        tpm_response_code: u32,
        /// The serialized `TPMS_ATTEST` statement returned by the TPM.
        statement: Vec<u8>,
        /// The serialized `TPMT_SIGNATURE` returned by the TPM.
        signature: Vec<u8>,
    }

    /// Response from parsing a TPM2_Hash command.
    #[cxx_name = "RawHashResponse"]
    struct HashResponse {
        /// The outcome of the parsing operation.
        result: ParseResult,
        /// The TPM response code, if the TPM returned an error.
        tpm_response_code: u32,
        /// The hash digest returned by the TPM.
        digest: Vec<u8>,
        /// The validation ticket (`TPMT_TK_HASHCHECK`) returned by the TPM.
        validation_ticket: Vec<u8>,
    }

    /// Response from parsing a TPM2_Sign command.
    #[cxx_name = "RawSignResponse"]
    struct SignResponse {
        /// The outcome of the parsing operation.
        result: ParseResult,
        /// The TPM response code, if the TPM returned an error.
        tpm_response_code: u32,
        /// The serialized `TPMT_SIGNATURE` returned by the TPM.
        signature: Vec<u8>,
    }

    /// Results that can occur during TPM signature parsing.
    // LINT.IfChange(SignatureParseResult)
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

    /// TPM Algorithms. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=41 for details.
    #[derive(Debug)]
    #[repr(u16)]
    enum TpmAlg {
        /// TPM_ALG_NULL is the null algorithm.
        TPM_ALG_NULL = 0x0010,
        /// TPM_ALG_RSASSA is the RSASSA signature algorithm.
        TPM_ALG_RSASSA = 0x0014,
        /// TPM_ALG_ECDSA is the ECDSA signature algorithm.
        TPM_ALG_ECDSA = 0x0018,
        /// TPM_ALG_SHA1 is the SHA-1 hash algorithm.
        TPM_ALG_SHA1 = 0x0004,
        /// TPM_ALG_SHA256 is the SHA-256 hash algorithm.
        TPM_ALG_SHA256 = 0x000B,
        /// TPM_ALG_SHA384 is the SHA-384 hash algorithm.
        TPM_ALG_SHA384 = 0x000C,
        /// TPM_ALG_SHA512 is the SHA-512 hash algorithm.
        TPM_ALG_SHA512 = 0x000D,
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
        /// TPM_CC_CERTIFY is the command code for TPM2_Certify.
        TPM_CC_CERTIFY = 0x00000148,
        /// TPM_CC_HASH is the command code for TPM2_Hash.
        TPM_CC_HASH = 0x0000017d,
        /// TPM_CC_SIGN is the command code for TPM2_Sign.
        TPM_CC_SIGN = 0x0000015d,
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
        /// TPM_ST_HASHCHECK is the tag for a hashcheck validation ticket.
        TPM_ST_HASHCHECK = 0x8024,
    }

    /// Struct containing the parsed raw components of a TPM signature.
    struct RawSignatureComponents {
        /// The outcome of the parsing operation.
        status: SignatureParseResult,
        /// The signature algorithm ID (e.g., TPM_ALG_RSASSA or TPM_ALG_ECDSA).
        sig_alg: TpmAlg,
        /// The hash algorithm ID (e.g., TPM_ALG_SHA256).
        hash_alg: TpmAlg,
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
        /// and ensures the provided challenge matches the one in the
        /// attestation's extra data to prevent replay attacks.
        ///
        /// # Arguments
        ///
        /// * `resp` - The raw byte response from the TPM2_Certify command.
        /// * `challenge` - The challenge expected in the attestation's
        ///   `extra_data` field.
        ///
        /// # Returns
        ///
        /// A `CertifyResponse` containing the parsing result, any TPM error
        /// code, the serialized `TPMS_ATTEST` statement, and the
        /// serialized `TPMT_SIGNATURE`.
        fn parse_certify_response(resp: &[u8], challenge: &[u8]) -> CertifyResponse;

        /// Builds a TPM2_Hash command buffer.
        fn build_hash_command(data: &[u8], hash_alg: u16, hierarchy: u32) -> Vec<u8>;

        /// Parses a TPM2_Hash response.
        ///
        /// Note that if the TPM returns an error code, the `digest` and
        /// `validation_ticket` fields will be empty.
        fn parse_hash_response(resp: &[u8]) -> HashResponse;

        /// Builds a TPM2_Sign command buffer.
        fn build_sign_command(
            key_handle: u32,
            digest: &[u8],
            sig_alg: u16,
            hash_alg: u16,
            validation_ticket: &[u8],
        ) -> Vec<u8>;

        /// Parses a TPM2_Sign response.
        fn parse_sign_response(resp: &[u8]) -> SignResponse;

        /// Parses a serialized `TPMT_SIGNATURE` and returns its raw components.
        fn parse_tpm_signature(signature: &[u8]) -> RawSignatureComponents;
    }
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
pub fn build_certify_command_impl(
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
    writer.write_u16(TPM_ST_SESSIONS);
    writer.write_u32(total_size.try_into().unwrap());
    writer.write_u32(TPM_CC_CERTIFY);

    // 2. Handles
    writer.write_u32(object_handle);
    writer.write_u32(sign_handle);

    // 3. Authorization Area (TPMS_AUTH_COMMAND)
    writer.write_u32(2 * u32::try_from(TPM_SESSION_SIZE).unwrap()); // Authorization block size

    // 3a. Auth Session 1 (For object_handle)
    writer.write_u32(TPM_RS_PW);
    writer.write_u16(0); // nonce size: 0
    writer.write_u8(0); // sessionAttributes: 0
    writer.write_u16(0); // hmac size: 0

    // 3b. Auth Session 2 (For sign_handle)
    writer.write_u32(TPM_RS_PW);
    writer.write_u16(0); // nonce size: 0
    writer.write_u8(0); // sessionAttributes: 0
    writer.write_u16(0); // hmac size: 0

    // 4. Command Parameters
    // qualifyingData (TPM2B_DATA)
    writer.write_tpm2b(qualifying_data);

    // inScheme (TPMT_SIG_SCHEME)
    writer.write_u16(ffi::TpmAlg::TPM_ALG_NULL.repr);

    writer.into_inner()
}

/// Builds a TPM2_Certify command.
pub fn build_certify_command(
    object_handle: u32,
    sign_handle: u32,
    qualifying_data: &[u8],
) -> Vec<u8> {
    build_certify_command_impl(object_handle, sign_handle, qualifying_data)
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
    pub magic: u32,
    pub type_: u16,
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
        let magic = reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?;
        // Read the attestation type (e.g., TPM_ST_ATTEST_CERTIFY)
        let type_ = reader.read_u16().ok_or(TpmParseError::BufferTooSmall)?;
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
        if type_ == TPM_ST_ATTEST_CERTIFY {
            let _name = reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
            let _qualified_name = reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
        }

        // Ensure the entire buffer for this struct was parsed exactly.
        // If there's data left, the format is unexpected or corrupted.
        if !reader.is_empty() {
            return Err(TpmParseError::TrailingBytes);
        }

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
    challenge: &[u8],
) -> Result<CertifyData<'a>, TpmParseError> {
    let mut reader = Reader::new(resp);

    // Read the response tag (e.g., TPM_ST_SESSIONS or TPM_ST_NO_SESSIONS)
    let tag = reader.read_u16().ok_or(TpmParseError::BufferTooSmall)?;
    // Read the total response size
    let response_size: usize =
        reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?.try_into().unwrap();
    // Read the TPM response code (0 means success)
    let response_code = reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?;

    // Verify that the size matches exactly.
    if resp.len() != response_size {
        return Err(TpmParseError::TrailingBytes);
    }

    if response_code != 0 {
        return Err(TpmParseError::TpmErrorResponse(response_code));
    }

    // Determine the size of the parameters section
    let parameter_size = match tag {
        TPM_ST_SESSIONS => {
            reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?.try_into().unwrap()
        }
        // Everything after the header
        TPM_ST_NO_SESSIONS => response_size - TPM_HEADER_SIZE,
        _ => return Err(TpmParseError::WrongType),
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
    if tag == TPM_ST_SESSIONS {
        let _session1 = TpmsAuthResponse::parse(&mut reader)?;
        let _session2 = TpmsAuthResponse::parse(&mut reader)?;
    }

    // Ensure the entire buffer for this struct was parsed exactly.
    // If there's data left, the format is unexpected or corrupted.
    if !reader.is_empty() {
        return Err(TpmParseError::TrailingBytes);
    }

    // Parse the TPMS_ATTEST structure
    let mut attest_reader = Reader::new(statement);
    let attest_info = TpmsAttest::parse(&mut attest_reader)?;

    // Validate the magic number to ensure it's a TPM-generated structure
    if attest_info.magic != TPM_GENERATED_VALUE {
        return Err(TpmParseError::BadMagicNumber);
    }
    // Ensure this is specifically a certify attestation
    if attest_info.type_ != TPM_ST_ATTEST_CERTIFY {
        return Err(TpmParseError::WrongType);
    }
    // Verify the challenge matches to prevent replay attacks
    if attest_info.extra_data != challenge {
        return Err(TpmParseError::ChallengeMismatch);
    }

    Ok(CertifyData { statement, signature })
}

impl From<TpmParseError> for ffi::CertifyResponse {
    fn from(err: TpmParseError) -> Self {
        let (result, tpm_response_code) = match err {
            TpmParseError::BufferTooSmall => (ffi::ParseResult::BufferTooSmall, 0),
            TpmParseError::TrailingBytes => (ffi::ParseResult::TrailingBytes, 0),
            TpmParseError::TpmErrorResponse(code) => (ffi::ParseResult::TpmErrorResponse, code),
            TpmParseError::BadMagicNumber => (ffi::ParseResult::BadMagicNumber, 0),
            TpmParseError::WrongType => (ffi::ParseResult::WrongType, 0),
            TpmParseError::ChallengeMismatch => (ffi::ParseResult::ChallengeMismatch, 0),
        };
        ffi::CertifyResponse {
            result,
            tpm_response_code,
            statement: Vec::new(),
            signature: Vec::new(),
        }
    }
}

impl<'a> From<Result<CertifyData<'a>, TpmParseError>> for ffi::CertifyResponse {
    fn from(result: Result<CertifyData<'a>, TpmParseError>) -> Self {
        match result {
            Ok(resp) => ffi::CertifyResponse {
                result: ffi::ParseResult::Ok,
                tpm_response_code: 0,
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
/// magic number, and ensures the provided challenge matches the one in the
/// attestation's extra data to prevent replay attacks.
///
/// # Arguments
///
/// * `resp` - The raw byte response from the TPM2_Certify command.
/// * `challenge` - The challenge expected in the attestation's `extra_data`
///   field.
///
/// # Returns
///
/// A `CertifyResponse` containing the parsing result, any TPM error code,
/// the serialized `TPMS_ATTEST` statement, and the serialized `TPMT_SIGNATURE`.
pub fn parse_certify_response(resp: &[u8], challenge: &[u8]) -> ffi::CertifyResponse {
    parse_certify_response_impl(resp, challenge).into()
}

/// Enum representing the signature data for different algorithms.
enum SignatureData<'a> {
    Rsa(&'a [u8]),
    Ecdsa { r: &'a [u8], s: &'a [u8] },
}

/// Information about algorithms used in a TPM signature.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SignatureAlgorithms {
    pub sig_alg: u16,
    pub hash_alg: u16,
}

impl SignatureAlgorithms {
    pub fn parse(reader: &mut Reader<'_>) -> Option<Self> {
        let sig_alg = reader.read_u16()?;
        let hash_alg = reader.read_u16()?;
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

        let sig_alg = ffi::TpmAlg { repr: algorithms.sig_alg };
        let signature_data = match sig_alg {
            ffi::TpmAlg::TPM_ALG_RSASSA => {
                let rsa_sig = reader.read_tpm2b().ok_or(TpmSignatureParseError::BufferTooSmall)?;
                SignatureData::Rsa(rsa_sig)
            }
            ffi::TpmAlg::TPM_ALG_ECDSA => {
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
            sig_alg: ffi::TpmAlg { repr: 0 },
            hash_alg: ffi::TpmAlg { repr: 0 },
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

    let sig_alg = tpm_sig.algorithms.sig_alg;
    let hash_alg = tpm_sig.algorithms.hash_alg;

    let (rsa_sig, ecdsa_r, ecdsa_s) = match tpm_sig.signature_data {
        SignatureData::Rsa(sig) => (sig.to_vec(), Vec::new(), Vec::new()),
        SignatureData::Ecdsa { r, s } => (Vec::new(), r.to_vec(), s.to_vec()),
    };

    Ok(ffi::RawSignatureComponents {
        status: ffi::SignatureParseResult::Ok,
        sig_alg: ffi::TpmAlg { repr: sig_alg },
        hash_alg: ffi::TpmAlg { repr: hash_alg },
        rsa_sig,
        ecdsa_r,
        ecdsa_s,
    })
}

pub fn build_hash_command_impl(data: &[u8], hash_alg: u16, hierarchy: u32) -> Vec<u8> {
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
    writer.write_u16(TPM_ST_NO_SESSIONS);
    writer.write_u32(total_size.try_into().unwrap());
    writer.write_u32(TPM_CC_HASH);

    // 2. Command Parameters
    writer.write_tpm2b(data);
    writer.write_u16(hash_alg);
    writer.write_u32(hierarchy);

    writer.into_inner()
}

struct HashData<'a> {
    digest: &'a [u8],
    validation: &'a [u8],
}

fn parse_hash_response_impl<'a>(resp: &'a [u8]) -> Result<HashData<'a>, TpmParseError> {
    let mut reader = Reader::new(resp);

    let tag = reader.read_u16().ok_or(TpmParseError::BufferTooSmall)?;
    if tag != TPM_ST_NO_SESSIONS {
        return Err(TpmParseError::WrongType);
    }
    let response_size: usize = reader
        .read_u32()
        .ok_or(TpmParseError::BufferTooSmall)?
        .try_into()
        .map_err(|_| TpmParseError::BufferTooSmall)?;
    let response_code = reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?;

    if resp.len() != response_size {
        return Err(TpmParseError::TrailingBytes);
    }

    if response_code != 0 {
        return Err(TpmParseError::TpmErrorResponse(response_code));
    }

    let parameter_size = response_size - TPM_HEADER_SIZE;
    let mut param_reader =
        Reader::new(reader.read_bytes(parameter_size).ok_or(TpmParseError::BufferTooSmall)?);

    let digest = param_reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;
    let validation = param_reader.read_all();

    let mut ticket_reader = Reader::new(validation);
    let ticket_tag = ticket_reader.read_u16().ok_or(TpmParseError::BufferTooSmall)?;
    if ticket_tag != TPM_ST_HASHCHECK {
        return Err(TpmParseError::WrongType);
    }
    let _hierarchy = ticket_reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?;
    let _ticket_digest = ticket_reader.read_tpm2b().ok_or(TpmParseError::BufferTooSmall)?;

    if !ticket_reader.is_empty() {
        return Err(TpmParseError::TrailingBytes);
    }

    Ok(HashData { digest, validation })
}

pub fn build_sign_command_impl(
    key_handle: u32,
    digest: &[u8],
    sig_alg: u16,
    hash_alg: u16,
    validation_ticket: &[u8],
) -> Vec<u8> {
    let mut in_scheme_size = 2;
    if sig_alg != ffi::TpmAlg::TPM_ALG_NULL.repr {
        in_scheme_size += 2;
    }

    let total_size = TPM_HEADER_SIZE
        + TPM_HANDLE_SIZE
        + TPM_AUTH_SIZE_SIZE
        + TPM_SESSION_SIZE
        + 2 // digest size prefix
        + digest.len()
        + in_scheme_size
        + validation_ticket.len();

    let mut writer = Writer::with_capacity(total_size);

    // 1. Command Header
    writer.write_u16(TPM_ST_SESSIONS);
    writer.write_u32(total_size.try_into().unwrap());
    writer.write_u32(TPM_CC_SIGN);

    // 2. Handles
    writer.write_u32(key_handle);

    // 3. Authorization Area
    writer.write_u32(u32::try_from(TPM_SESSION_SIZE).unwrap());
    writer.write_u32(TPM_RS_PW);
    writer.write_u16(0); // nonce size
    writer.write_u8(0); // sessionAttributes
    writer.write_u16(0); // hmac size

    // 4. Command Parameters
    writer.write_tpm2b(digest);

    writer.write_u16(sig_alg);
    if sig_alg != ffi::TpmAlg::TPM_ALG_NULL.repr {
        writer.write_u16(hash_alg);
    }

    writer.write_bytes(validation_ticket);

    writer.into_inner()
}

fn parse_sign_response_impl(resp: &[u8]) -> Result<&[u8], TpmParseError> {
    let mut reader = Reader::new(resp);

    let tag = reader.read_u16().ok_or(TpmParseError::BufferTooSmall)?;
    let response_size: usize =
        reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?.try_into().unwrap();
    let response_code = reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?;

    if resp.len() != response_size {
        return Err(TpmParseError::TrailingBytes);
    }

    if response_code != 0 {
        return Err(TpmParseError::TpmErrorResponse(response_code));
    }

    let parameter_size = match tag {
        TPM_ST_SESSIONS => {
            reader.read_u32().ok_or(TpmParseError::BufferTooSmall)?.try_into().unwrap()
        }
        TPM_ST_NO_SESSIONS => response_size - TPM_HEADER_SIZE,
        _ => return Err(TpmParseError::WrongType),
    };

    let signature = reader.read_bytes(parameter_size).ok_or(TpmParseError::BufferTooSmall)?;

    let _algs = SignatureAlgorithms::parse(&mut Reader::new(signature))
        .ok_or(TpmParseError::BufferTooSmall)?;

    if tag == TPM_ST_SESSIONS {
        let _session1 = TpmsAuthResponse::parse(&mut reader)?;
    }

    if !reader.is_empty() {
        return Err(TpmParseError::TrailingBytes);
    }

    Ok(signature)
}

impl From<TpmParseError> for ffi::HashResponse {
    fn from(err: TpmParseError) -> Self {
        let (result, tpm_response_code) = match err {
            TpmParseError::BufferTooSmall => (ffi::ParseResult::BufferTooSmall, 0),
            TpmParseError::TrailingBytes => (ffi::ParseResult::TrailingBytes, 0),
            TpmParseError::TpmErrorResponse(code) => (ffi::ParseResult::TpmErrorResponse, code),
            TpmParseError::BadMagicNumber => (ffi::ParseResult::BadMagicNumber, 0),
            TpmParseError::WrongType => (ffi::ParseResult::WrongType, 0),
            TpmParseError::ChallengeMismatch => (ffi::ParseResult::ChallengeMismatch, 0),
        };
        ffi::HashResponse {
            result,
            tpm_response_code,
            digest: Vec::new(),
            validation_ticket: Vec::new(),
        }
    }
}

impl<'a> From<Result<HashData<'a>, TpmParseError>> for ffi::HashResponse {
    fn from(result: Result<HashData<'a>, TpmParseError>) -> Self {
        match result {
            Ok(data) => ffi::HashResponse {
                result: ffi::ParseResult::Ok,
                tpm_response_code: 0,
                digest: data.digest.to_vec(),
                validation_ticket: data.validation.to_vec(),
            },
            Err(err) => err.into(),
        }
    }
}

impl From<TpmParseError> for ffi::SignResponse {
    fn from(err: TpmParseError) -> Self {
        let (result, tpm_response_code) = match err {
            TpmParseError::BufferTooSmall => (ffi::ParseResult::BufferTooSmall, 0),
            TpmParseError::TrailingBytes => (ffi::ParseResult::TrailingBytes, 0),
            TpmParseError::TpmErrorResponse(code) => (ffi::ParseResult::TpmErrorResponse, code),
            TpmParseError::BadMagicNumber => (ffi::ParseResult::BadMagicNumber, 0),
            TpmParseError::WrongType => (ffi::ParseResult::WrongType, 0),
            TpmParseError::ChallengeMismatch => (ffi::ParseResult::ChallengeMismatch, 0),
        };
        ffi::SignResponse { result, tpm_response_code, signature: Vec::new() }
    }
}

impl<'a> From<Result<&'a [u8], TpmParseError>> for ffi::SignResponse {
    fn from(result: Result<&'a [u8], TpmParseError>) -> Self {
        match result {
            Ok(sig) => ffi::SignResponse {
                result: ffi::ParseResult::Ok,
                tpm_response_code: 0,
                signature: sig.to_vec(),
            },
            Err(err) => err.into(),
        }
    }
}

pub fn build_hash_command(data: &[u8], hash_alg: u16, hierarchy: u32) -> Vec<u8> {
    build_hash_command_impl(data, hash_alg, hierarchy)
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

pub fn build_sign_command(
    key_handle: u32,
    digest: &[u8],
    sig_alg: u16,
    hash_alg: u16,
    validation_ticket: &[u8],
) -> Vec<u8> {
    build_sign_command_impl(key_handle, digest, sig_alg, hash_alg, validation_ticket)
}

pub fn parse_sign_response(resp: &[u8]) -> ffi::SignResponse {
    parse_sign_response_impl(resp).into()
}

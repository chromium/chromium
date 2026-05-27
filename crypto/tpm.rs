// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// TPM Constants. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=41 for details.
/// TPM_GENERATED_VALUE is the magic number in TPM generated structures.
pub const TPM_GENERATED_VALUE: u32 = 0xFF544347;

// TPM Command Codes. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=47 for details.
/// TPM_CC_CERTIFY is the command code for TPM2_Certify.
pub const TPM_CC_CERTIFY: u32 = 0x00000148;

// TPM Structure Tags. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=65 for details.
/// TPM_ST_NO_SESSIONS indicates that the command has no sessions.
pub const TPM_ST_NO_SESSIONS: u16 = 0x8001;
/// TPM_ST_SESSIONS indicates that the command has sessions.
pub const TPM_ST_SESSIONS: u16 = 0x8002;
/// TPM_ST_ATTEST_CERTIFY is the tag for a certify attestation statement.
pub const TPM_ST_ATTEST_CERTIFY: u16 = 0x8017;

// TPM Handles. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=88 for details.
/// TPM_RS_PW is the handle for a password session.
pub const TPM_RS_PW: u32 = 0x40000009;

/// TPM Algorithms. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=41 for details.
/// TPM_ALG_NULL is the null algorithm.
pub const TPM_ALG_NULL: u16 = 0x0010;
/// TPM_ALG_SHA1 is the SHA-1 hash algorithm.
pub const TPM_ALG_SHA1: u16 = 0x0004;
/// TPM_ALG_SHA256 is the SHA-256 hash algorithm.
pub const TPM_ALG_SHA256: u16 = 0x000B;
/// TPM_ALG_RSASSA is the RSASSA signature algorithm.
pub const TPM_ALG_RSASSA: u16 = 0x0014;
/// TPM_ALG_ECDSA is the ECDSA signature algorithm.
pub const TPM_ALG_ECDSA: u16 = 0x0018;

/// Size of a standard TPM command header (Tag + Size + CommandCode).
pub const TPM_HEADER_SIZE: usize = 10;
/// Size of a TPM handle in bytes.
pub const TPM_HANDLE_SIZE: usize = 4;
/// Size of the auth size field in bytes.
pub const TPM_AUTH_SIZE_SIZE: usize = 4;
/// Size of a password session authorization area in bytes.
pub const TPM_SESSION_SIZE: usize = 9;

/// Errors that can occur during TPM response parsing and verification.
#[derive(Debug)]
pub enum TpmError {
    /// The provided buffer contained had an unexpected size.
    WrongBufferSize,
    /// The TPM returned an error code. Contains the TPM response code.
    TpmErrorResponse(u32),
    /// The structure did not contain the expected TPM magic number.
    BadMagicNumber,
    /// The structure type did not match the expected type.
    WrongType,
    /// The provided nonce did not match the nonce in the attestation.
    NonceMismatch,
}

impl std::fmt::Display for TpmError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            TpmError::WrongBufferSize => write!(f, "wrong buffer size"),
            TpmError::TpmErrorResponse(code) => {
                write!(f, "TPM returned an error response: {:#010x}", code)
            }
            TpmError::BadMagicNumber => write!(f, "bad magic number in TPM response"),
            TpmError::WrongType => write!(f, "wrong type in TPM response"),
            TpmError::NonceMismatch => write!(f, "nonce mismatch in TPM response"),
        }
    }
}

impl std::error::Error for TpmError {}

/// CXX bridge for TPM FFI.
#[cxx::bridge(namespace = "crypto::tpm")]
pub mod ffi {
    /// Results that can occur during TPM response parsing.
    enum ParseResult {
        Ok = 0,
        WrongBufferSize = 1,
        TpmErrorResponse = 2,
        BadMagicNumber = 3,
        WrongType = 4,
        NonceMismatch = 5,
    }

    /// Response from parsing a TPM2_Certify command.
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

    extern "Rust" {
        /// Builds a TPM2_Certify command buffer.
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
        /// and ensures the provided nonce matches the one in the attestation's
        /// extra data to prevent replay attacks.
        ///
        /// # Arguments
        ///
        /// * `resp` - The raw byte response from the TPM2_Certify command.
        /// * `nonce` - The nonce expected in the attestation's `extra_data`
        ///   field.
        ///
        /// # Returns
        ///
        /// A `CertifyResponse` containing the parsing result, any TPM error
        /// code, the serialized `TPMS_ATTEST` statement, and the
        /// serialized `TPMT_SIGNATURE`.
        fn parse_certify_response(resp: &[u8], nonce: &[u8]) -> CertifyResponse;
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
    pub fn read_bytes(&mut self, len: usize) -> Result<&'a [u8], TpmError> {
        let (val, rest) = self.data.split_at_checked(len).ok_or(TpmError::WrongBufferSize)?;
        self.data = rest;
        Ok(val)
    }

    /// Safely extracts a fixed-size chunk from the reader, advancing the
    /// internal cursor.
    fn take<const N: usize>(&mut self) -> Result<&[u8; N], TpmError> {
        let (chunk, rest) = self.data.split_first_chunk().ok_or(TpmError::WrongBufferSize)?;
        self.data = rest;
        Ok(chunk)
    }

    /// Reads a single byte.
    pub fn read_u8(&mut self) -> Result<u8, TpmError> {
        Ok(u8::from_be_bytes(*self.take()?))
    }

    /// Reads a u16 in big-endian format.
    pub fn read_u16(&mut self) -> Result<u16, TpmError> {
        Ok(u16::from_be_bytes(*self.take()?))
    }

    /// Reads a u32 in big-endian format.
    pub fn read_u32(&mut self) -> Result<u32, TpmError> {
        Ok(u32::from_be_bytes(*self.take()?))
    }

    /// Reads a TPM2B structure (a 2-byte size prefix followed by that many
    /// bytes) and returns the payload (excluding the size prefix).
    ///
    /// See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=141 for details.
    pub fn read_tpm2b(&mut self) -> Result<&'a [u8], TpmError> {
        let size: usize = self.read_u16()?.into();
        self.read_bytes(size)
    }

    /// Peeks a u16 in big-endian format without advancing the reader.
    pub fn peek_u16(&self) -> Result<u16, TpmError> {
        Ok(u16::from_be_bytes(*self.data.first_chunk().ok_or(TpmError::WrongBufferSize)?))
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
///
/// Returns an empty `Vec` if `qualifying_data` exceeds `u16::MAX` bytes.
pub fn build_certify_command(
    object_handle: u32,
    sign_handle: u32,
    qualifying_data: &[u8],
) -> Vec<u8> {
    let Ok(len_as_u16) = u16::try_from(qualifying_data.len()) else {
        return Vec::new();
    };

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
    writer.write_u16(len_as_u16);
    writer.write_bytes(qualifying_data);

    // inScheme (TPMT_SIG_SCHEME)
    writer.write_u16(TPM_ALG_NULL);

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
    fn parse(reader: &mut Reader<'a>) -> Result<Self, TpmError> {
        let nonce = reader.read_tpm2b()?;
        let session_attributes = reader.read_u8()?;
        let hmac = reader.read_tpm2b()?;
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
    fn parse(reader: &mut Reader<'a>) -> Result<Self, TpmError> {
        // Read the magic number (should be TPM_GENERATED_VALUE)
        let magic = reader.read_u32()?;
        // Read the attestation type (e.g., TPM_ST_ATTEST_CERTIFY)
        let type_ = reader.read_u16()?;
        // Read the qualified signer name (Name of the object that signed the
        // attestation)
        let qualified_signer = reader.read_tpm2b()?;
        // Read the extra data (often contains a nonce for freshness)
        let extra_data = reader.read_tpm2b()?;

        // Clock info and firmware version are part of TPMS_CLOCK_INFO and are standard
        // trailing fields in all TPMS_ATTEST structures. We read them to advance the
        // cursor.
        let _clock_info = reader.read_bytes(17)?;
        let _firmware_version = reader.read_bytes(8)?;

        // For certify attestations, there are additional fields: the certified object's
        // Name and Qualified Name. We read them to ensure the buffer is fully parsed.
        if type_ == TPM_ST_ATTEST_CERTIFY {
            let _name = reader.read_tpm2b()?;
            let _qualified_name = reader.read_tpm2b()?;
        }

        // Ensure the entire buffer for this struct was parsed exactly.
        // If there's data left, the format is unexpected or corrupted.
        if !reader.is_empty() {
            return Err(TpmError::WrongBufferSize);
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
    nonce: &[u8],
) -> Result<CertifyData<'a>, TpmError> {
    let mut reader = Reader::new(resp);

    // Read the response tag (e.g., TPM_ST_SESSIONS or TPM_ST_NO_SESSIONS)
    let tag = reader.read_u16()?;
    // Read the total response size
    let response_size: usize = reader.read_u32()?.try_into().unwrap();
    // Read the TPM response code (0 means success)
    let response_code = reader.read_u32()?;

    // Verify that the size matches exactly.
    if resp.len() != response_size {
        return Err(TpmError::WrongBufferSize);
    }

    if response_code != 0 {
        return Err(TpmError::TpmErrorResponse(response_code));
    }

    // Determine the size of the parameters section
    let parameter_size = match tag {
        TPM_ST_SESSIONS => reader.read_u32()?.try_into().unwrap(),
        // Everything after the header
        TPM_ST_NO_SESSIONS => response_size - TPM_HEADER_SIZE,
        _ => return Err(TpmError::WrongType),
    };
    // Create a sub-reader specifically for the parameters section
    let mut param_reader = Reader::new(reader.read_bytes(parameter_size)?);

    // Read the inner TPMS_ATTEST structure bytes (size-prefixed in the protocol)
    let statement = param_reader.read_tpm2b()?;

    // The remaining data in the parameters section is the signature
    // (TPMT_SIGNATURE)
    // Read the signature algorithm (e.g., TPM_ALG_RSASSA or TPM_ALG_ECDSA)
    // without advancing the reader, so we can return the entire TPMT_SIGNATURE.
    let _sig_alg = param_reader.peek_u16()?;
    // The entire rest of the parameter section is treated as the signature
    let signature = param_reader.read_all();

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
        return Err(TpmError::WrongBufferSize);
    }

    // Parse the TPMS_ATTEST structure
    let mut attest_reader = Reader::new(statement);
    let attest_info = TpmsAttest::parse(&mut attest_reader)?;

    // Validate the magic number to ensure it's a TPM-generated structure
    if attest_info.magic != TPM_GENERATED_VALUE {
        return Err(TpmError::BadMagicNumber);
    }
    // Ensure this is specifically a certify attestation
    if attest_info.type_ != TPM_ST_ATTEST_CERTIFY {
        return Err(TpmError::WrongType);
    }
    // Verify the nonce matches to prevent replay attacks
    if attest_info.extra_data != nonce {
        return Err(TpmError::NonceMismatch);
    }

    Ok(CertifyData { statement, signature })
}

impl From<TpmError> for ffi::CertifyResponse {
    fn from(err: TpmError) -> Self {
        let (result, tpm_response_code) = match err {
            TpmError::WrongBufferSize => (ffi::ParseResult::WrongBufferSize, 0),
            TpmError::TpmErrorResponse(code) => (ffi::ParseResult::TpmErrorResponse, code),
            TpmError::BadMagicNumber => (ffi::ParseResult::BadMagicNumber, 0),
            TpmError::WrongType => (ffi::ParseResult::WrongType, 0),
            TpmError::NonceMismatch => (ffi::ParseResult::NonceMismatch, 0),
        };
        ffi::CertifyResponse {
            result,
            tpm_response_code,
            statement: Vec::new(),
            signature: Vec::new(),
        }
    }
}

impl<'a> From<Result<CertifyData<'a>, TpmError>> for ffi::CertifyResponse {
    fn from(result: Result<CertifyData<'a>, TpmError>) -> Self {
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
/// magic number, and ensures the provided nonce matches the one in the
/// attestation's extra data to prevent replay attacks.
///
/// # Arguments
///
/// * `resp` - The raw byte response from the TPM2_Certify command.
/// * `nonce` - The nonce expected in the attestation's `extra_data` field.
///
/// # Returns
///
/// A `CertifyResponse` containing the parsing result, any TPM error code,
/// the serialized `TPMS_ATTEST` statement, and the serialized `TPMT_SIGNATURE`.
pub fn parse_certify_response(resp: &[u8], nonce: &[u8]) -> ffi::CertifyResponse {
    parse_certify_response_impl(resp, nonce).into()
}

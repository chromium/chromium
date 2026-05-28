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
    /// The provided nonce did not match the nonce in the attestation.
    NonceMismatch,
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
            TpmParseError::NonceMismatch => write!(f, "nonce mismatch in TPM response"),
        }
    }
}

impl std::error::Error for TpmParseError {}

/// Errors that can occur during TPM signature verification.
#[derive(Debug)]
pub enum TpmVerifyError {
    /// The provided signature buffer was too small to read the required data.
    BufferTooSmall,
    /// The provided signature buffer had trailing bytes after parsing the
    /// signature.
    TrailingBytes,
    /// The signature algorithm identified is not supported by this verification
    /// implementation.
    UnsupportedSignatureAlgorithm,
    /// The hash algorithm identified is not supported by this verification
    /// implementation.
    UnsupportedHashAlgorithm,
    /// The provided public key (SPKI) could not be parsed or is invalid for the
    /// specified signature algorithm.
    InvalidPublicKey,
    /// Cryptographic verification of the signature over the payload failed.
    InvalidSignature,
}

impl std::fmt::Display for TpmVerifyError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            TpmVerifyError::BufferTooSmall => write!(f, "buffer too small"),
            TpmVerifyError::TrailingBytes => write!(f, "trailing bytes in buffer"),
            TpmVerifyError::UnsupportedSignatureAlgorithm => {
                write!(f, "unsupported signature algorithm")
            }
            TpmVerifyError::UnsupportedHashAlgorithm => write!(f, "unsupported hash algorithm"),
            TpmVerifyError::InvalidPublicKey => write!(f, "invalid public key"),
            TpmVerifyError::InvalidSignature => write!(f, "invalid signature"),
        }
    }
}

impl std::error::Error for TpmVerifyError {}

/// CXX bridge for TPM FFI.
#[cxx::bridge(namespace = "crypto::tpm")]
pub mod ffi {
    /// Results that can occur during TPM response parsing.
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
        /// The provided nonce did not match the nonce in the attestation.
        NonceMismatch = 6,
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

    /// Response containing the extracted signature algorithms from a TPM
    /// signature. Used to pass algorithm information across the FFI
    /// boundary to C++.
    struct SignatureAlgorithmsResponse {
        /// Indicates whether signature algorithms were successfully extracted.
        /// If false, the following fields will be zero.
        has_algorithms: bool,
        /// The signature algorithm ID (e.g., TPM_ALG_RSASSA or TPM_ALG_ECDSA).
        sig_alg: u16,
        /// The hash algorithm ID (e.g., TPM_ALG_SHA256).
        hash_alg: u16,
    }

    /// Results that can occur during TPM signature verification.
    enum VerificationResult {
        /// Verification completed successfully.
        Ok = 0,
        /// The signature buffer was too small to read the required fields.
        BufferTooSmall = 1,
        /// The signature buffer had trailing bytes after parsing.
        TrailingBytes = 2,
        /// The signature algorithm is not supported.
        UnsupportedSignatureAlgorithm = 3,
        /// The hash algorithm is not supported.
        UnsupportedHashAlgorithm = 4,
        /// The provided public key could not be parsed or was invalid.
        InvalidPublicKey = 5,
        /// The cryptographic verification of the signature failed.
        InvalidSignature = 6,
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
        ///   freshness (e.g., a nonce).
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

        /// Verifies a generic TPM signature over an arbitrary byte payload.
        ///
        /// * `payload` - The arbitrary bytes over which the signature was
        ///   computed.
        /// * `signature` - The serialized `TPMT_SIGNATURE` returned by the TPM.
        /// * `spki` - The Subject Public Key Info (SPKI) of the key used to
        ///   verify the signature.
        ///
        /// Note: This verification is strictly best-effort. It is entirely
        /// possible that the signature is mathematically valid, but we
        /// simply haven't implemented the necessary verification
        /// algorithms yet (e.g., P-384 or specific hash algorithms).
        fn verify_signature(payload: &[u8], signature: &[u8], spki: &[u8]) -> VerificationResult;

        /// Extracts the signature algorithms from a serialized `TPMT_SIGNATURE`
        /// payload.
        fn extract_signature_algorithms(payload: &[u8]) -> SignatureAlgorithmsResponse;
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
    writer.write_u16(TPM_ALG_NULL);

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
    nonce: &[u8],
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
    let _algs =
        extract_signature_algorithms_impl(signature).ok_or(TpmParseError::BufferTooSmall)?;

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
    // Verify the nonce matches to prevent replay attacks
    if attest_info.extra_data != nonce {
        return Err(TpmParseError::NonceMismatch);
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
            TpmParseError::NonceMismatch => (ffi::ParseResult::NonceMismatch, 0),
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
    fn parse(reader: &mut Reader<'a>) -> Result<Self, TpmVerifyError> {
        let algorithms =
            SignatureAlgorithms::parse(reader).ok_or(TpmVerifyError::BufferTooSmall)?;

        let signature_data = match algorithms.sig_alg {
            TPM_ALG_RSASSA => {
                let rsa_sig = reader.read_tpm2b().ok_or(TpmVerifyError::BufferTooSmall)?;
                SignatureData::Rsa(rsa_sig)
            }
            TPM_ALG_ECDSA => {
                let r = reader.read_tpm2b().ok_or(TpmVerifyError::BufferTooSmall)?;
                let s = reader.read_tpm2b().ok_or(TpmVerifyError::BufferTooSmall)?;
                SignatureData::Ecdsa { r, s }
            }
            _ => {
                return Err(TpmVerifyError::UnsupportedSignatureAlgorithm);
            }
        };

        Ok(Self { algorithms, signature_data })
    }
}

/// Verifies a generic TPM signature over an arbitrary byte payload.
///
/// * `payload` - The arbitrary bytes over which the signature was computed.
/// * `signature` - The serialized `TPMT_SIGNATURE` returned by the TPM.
/// * `spki` - The Subject Public Key Info (SPKI) of the key used to verify the
///   signature.
///
/// Note: This verification is strictly best-effort. It is entirely possible
/// that the signature is mathematically valid, but we simply haven't
/// implemented the necessary verification algorithms yet (e.g., P-384 or
/// specific hash algorithms).
fn verify_signature_impl(
    payload: &[u8],
    signature: &[u8],
    spki: &[u8],
) -> Result<(), TpmVerifyError> {
    let mut sig_reader = Reader::new(signature);
    let tpm_sig = TpmtSignature::parse(&mut sig_reader)?;
    let hash_alg = tpm_sig.algorithms.hash_alg;

    // Reject trailing garbage after the signature.
    if !sig_reader.is_empty() {
        return Err(TpmVerifyError::TrailingBytes);
    }

    match tpm_sig.signature_data {
        SignatureData::Rsa(sig) => {
            bssl_crypto::rsa::PublicKey::from_der_subject_public_key_info(spki)
                .ok_or(TpmVerifyError::InvalidPublicKey)
                .and_then(|key| match hash_alg {
                    TPM_ALG_SHA256 => key
                        .verify_pkcs1::<bssl_crypto::digest::Sha256>(payload, sig)
                        .map_err(|_| TpmVerifyError::InvalidSignature),
                    TPM_ALG_SHA1 => key
                        .verify_pkcs1::<bssl_crypto::digest::InsecureSha1>(payload, sig)
                        .map_err(|_| TpmVerifyError::InvalidSignature),
                    _ => Err(TpmVerifyError::UnsupportedHashAlgorithm),
                })
        }
        SignatureData::Ecdsa { r, s } => {
            // Note: We perform the coordinate size bounds check here rather than in the
            // parser because the TPM 2.0 ECC signature structure does not
            // encode the curve ID. We currently only support P-256 (and
            // SHA-256) for ECDSA. If P-384 support is ever added, this bounds
            // check must dynamically inspect the curve type from the public key.
            const P256_COORDINATE_SIZE: usize = 32;
            if hash_alg != TPM_ALG_SHA256 {
                Err(TpmVerifyError::UnsupportedHashAlgorithm)
            } else if r.len() != P256_COORDINATE_SIZE || s.len() != P256_COORDINATE_SIZE {
                Err(TpmVerifyError::InvalidSignature)
            } else {
                bssl_crypto::ecdsa::PublicKey::<bssl_crypto::ec::P256>::from_der_subject_public_key_info(spki)
                    .ok_or(TpmVerifyError::InvalidPublicKey)
                    .and_then(|key| {
                        key.verify_p1363(payload, &[r, s].concat())
                            .map_err(|_| TpmVerifyError::InvalidSignature)
                    })
            }
        }
    }?;

    Ok(())
}

/// Verifies a generic TPM signature over an arbitrary byte payload.
pub fn verify_signature(payload: &[u8], signature: &[u8], spki: &[u8]) -> ffi::VerificationResult {
    match verify_signature_impl(payload, signature, spki) {
        Ok(()) => ffi::VerificationResult::Ok,
        Err(err) => match err {
            TpmVerifyError::BufferTooSmall => ffi::VerificationResult::BufferTooSmall,
            TpmVerifyError::TrailingBytes => ffi::VerificationResult::TrailingBytes,
            TpmVerifyError::UnsupportedSignatureAlgorithm => {
                ffi::VerificationResult::UnsupportedSignatureAlgorithm
            }
            TpmVerifyError::UnsupportedHashAlgorithm => {
                ffi::VerificationResult::UnsupportedHashAlgorithm
            }
            TpmVerifyError::InvalidPublicKey => ffi::VerificationResult::InvalidPublicKey,
            TpmVerifyError::InvalidSignature => ffi::VerificationResult::InvalidSignature,
        },
    }
}

/// Extracts the signature algorithms from a serialized `TPMT_SIGNATURE`
/// payload.
pub fn extract_signature_algorithms_impl(payload: &[u8]) -> Option<SignatureAlgorithms> {
    let mut reader = Reader::new(payload);
    SignatureAlgorithms::parse(&mut reader)
}

impl From<Option<SignatureAlgorithms>> for ffi::SignatureAlgorithmsResponse {
    fn from(result: Option<SignatureAlgorithms>) -> Self {
        ffi::SignatureAlgorithmsResponse {
            has_algorithms: result.is_some(),
            sig_alg: result.map(|a| a.sig_alg).unwrap_or(0),
            hash_alg: result.map(|a| a.hash_alg).unwrap_or(0),
        }
    }
}

pub fn extract_signature_algorithms(payload: &[u8]) -> ffi::SignatureAlgorithmsResponse {
    extract_signature_algorithms_impl(payload).into()
}

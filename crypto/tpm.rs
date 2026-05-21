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

// TPM Algorithms. See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=41 for details.
pub const TPM_ALG_NULL: u16 = 0x0010;

/// Size of a standard TPM command header (Tag + Size + CommandCode).
pub const TPM_HEADER_SIZE: u32 = 10;
/// Size of a TPM handle in bytes.
pub const TPM_HANDLE_SIZE: u32 = 4;
/// Size of the auth size field in bytes.
pub const TPM_AUTH_SIZE_SIZE: u32 = 4;
/// Size of a password session authorization area in bytes.
pub const TPM_SESSION_SIZE: u32 = 9;

/// CXX bridge for TPM FFI.
#[cxx::bridge(namespace = "crypto::tpm")]
pub mod ffi {
    /// Results that can occur during TPM response parsing.
    #[derive(Debug)]
    enum ParseResult {
        Ok = 0,
        BufferTooSmall = 1,
    }

    extern "Rust" {
        /// Builds a TPM2_Certify command buffer.
        fn build_certify_command(
            object_handle: u32,
            sign_handle: u32,
            qualifying_data: &[u8],
        ) -> Vec<u8>;
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

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    pub fn remaining(&self) -> usize {
        self.data.len()
    }

    /// Reads `len` bytes from the slice. Returns error if buffer is too small.
    pub fn read_bytes(&mut self, len: usize) -> Result<&'a [u8], ffi::ParseResult> {
        if self.data.len() < len {
            return Err(ffi::ParseResult::BufferTooSmall);
        }
        let (val, rest) = self.data.split_at(len);
        self.data = rest;
        Ok(val)
    }

    /// Reads a single byte.
    pub fn read_u8(&mut self) -> Result<u8, ffi::ParseResult> {
        let bytes = self.read_bytes(std::mem::size_of::<u8>())?;
        Ok(bytes[0])
    }

    /// Reads a u16 in big-endian format.
    pub fn read_u16(&mut self) -> Result<u16, ffi::ParseResult> {
        let bytes = self.read_bytes(std::mem::size_of::<u16>())?;
        Ok(u16::from_be_bytes(bytes.try_into().expect("slice length is guaranteed by read_bytes")))
    }

    /// Reads a u32 in big-endian format.
    pub fn read_u32(&mut self) -> Result<u32, ffi::ParseResult> {
        let bytes = self.read_bytes(std::mem::size_of::<u32>())?;
        Ok(u32::from_be_bytes(bytes.try_into().expect("slice length is guaranteed by read_bytes")))
    }
}

/// A simple byte writer to safely and cleanly construct TPM command buffers.
pub struct Writer {
    buffer: Vec<u8>,
}

impl Default for Writer {
    fn default() -> Self {
        Self::new()
    }
}

impl Writer {
    pub fn new() -> Self {
        Self { buffer: Vec::new() }
    }

    pub fn with_capacity(capacity: usize) -> Self {
        Self { buffer: Vec::with_capacity(capacity) }
    }

    pub fn write_u8(&mut self, val: u8) {
        self.buffer.push(val);
    }

    pub fn write_u16(&mut self, val: u16) {
        self.buffer.extend_from_slice(&val.to_be_bytes());
    }

    pub fn write_u32(&mut self, val: u32) {
        self.buffer.extend_from_slice(&val.to_be_bytes());
    }

    pub fn write_bytes(&mut self, data: &[u8]) {
        self.buffer.extend_from_slice(data);
    }

    pub fn into_inner(self) -> Vec<u8> {
        self.buffer
    }
}

/// Builds a TPM2_Certify command.
///
/// A TPM command has the following structure:
/// - **Header**: 10 bytes (Tag, Size, CommandCode)
/// - **Handles**: 0 or more 4-byte handles
/// - **Authorization Area**: Optional, contains sessions for authorization
/// - **Parameters**: Command-specific parameters
///
/// * `object_handle` - Handle of the object to be certified (the signing key).
/// * `sign_handle` - Handle of the key used to sign the attestation (the AIK).
/// * `qualifying_data` - Data provided by the caller to ensure freshness (e.g.,
///   a challenge).
///
/// Note: This function currently assumes empty password authorizations for both
/// the object and sign handles.
///
/// See https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-3-Commands_Version-185_pub.pdf#page=154 for details.
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
        + u32::from(len_as_u16) // Qualifying data
        + 2; // inScheme (Null)

    let mut writer = Writer::with_capacity(total_size as usize);

    // 1. Command Header
    writer.write_u16(TPM_ST_SESSIONS);
    writer.write_u32(total_size);
    writer.write_u32(TPM_CC_CERTIFY);

    // 2. Handles
    writer.write_u32(object_handle);
    writer.write_u32(sign_handle);

    // 3. Authorization Area
    writer.write_u32(2 * TPM_SESSION_SIZE); // Authorization block size

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

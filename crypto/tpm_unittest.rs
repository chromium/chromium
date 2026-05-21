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
    expect_eq!(reader.read_u16().unwrap(), QUALIFYING_DATA.len() as u16); // length
    expect_eq!(reader.read_bytes(QUALIFYING_DATA.len()).unwrap(), QUALIFYING_DATA);

    // Scheme
    expect_eq!(reader.read_u16().unwrap(), tpm::TPM_ALG_NULL);
}

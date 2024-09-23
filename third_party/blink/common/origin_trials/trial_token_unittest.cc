// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_trials/trial_token.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace blink {

namespace {

const uint8_t kVersion2 = 2;
const uint8_t kVersion3 = 3;

// This is a sample public key for testing the API. The corresponding private
// key (use this to generate new samples for this test file) is:
//
//  0x83, 0x67, 0xf4, 0xcd, 0x2a, 0x1f, 0x0e, 0x04, 0x0d, 0x43, 0x13,
//  0x4c, 0x67, 0xc4, 0xf4, 0x28, 0xc9, 0x90, 0x15, 0x02, 0xe2, 0xba,
//  0xfd, 0xbb, 0xfa, 0xbc, 0x92, 0x76, 0x8a, 0x2c, 0x4b, 0xc7, 0x75,
//  0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2, 0x9a,
//  0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f, 0x64,
//  0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0
//
//  This private key can also be found in tools/origin_trials/eftest.key in
//  binary form. Please update that if changing the key.
//
//  To use this with a real browser, use --origin-trial-public-key with the
//  public key, base-64-encoded:
//  --origin-trial-public-key=dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=
const OriginTrialPublicKey kTestPublicKey = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

// This is a valid, but incorrect, public key for testing signatures against.
// The corresponding private key is:
//
//  0x21, 0xee, 0xfa, 0x81, 0x6a, 0xff, 0xdf, 0xb8, 0xc1, 0xdd, 0x75,
//  0x05, 0x04, 0x29, 0x68, 0x67, 0x60, 0x85, 0x91, 0xd0, 0x50, 0x16,
//  0x0a, 0xcf, 0xa2, 0x37, 0xa3, 0x2e, 0x11, 0x7a, 0x17, 0x96, 0x50,
//  0x07, 0x4d, 0x76, 0x55, 0x56, 0x42, 0x17, 0x2d, 0x8a, 0x9c, 0x47,
//  0x96, 0x25, 0xda, 0x70, 0xaa, 0xb9, 0xfd, 0x53, 0x5d, 0x51, 0x3e,
//  0x16, 0xab, 0xb4, 0x86, 0xea, 0xf3, 0x35, 0xc6, 0xca
const OriginTrialPublicKey kTestPublicKey2 = {
    0x50, 0x07, 0x4d, 0x76, 0x55, 0x56, 0x42, 0x17, 0x2d, 0x8a, 0x9c,
    0x47, 0x96, 0x25, 0xda, 0x70, 0xaa, 0xb9, 0xfd, 0x53, 0x5d, 0x51,
    0x3e, 0x16, 0xab, 0xb4, 0x86, 0xea, 0xf3, 0x35, 0xc6, 0xca,
};

// This is a good trial token, signed with the above test private key.
// Generate this token with the command (in tools/origin_trials):
// generate_token.py valid.example.com 2 Frobulate --expire-timestamp=1458766277
const char kSampleTokenV2[] =
    "Ap+Q/Qm0ELadZql+dlEGSwnAVsFZKgCEtUZg8idQC3uekkIeSZIY1tftoYdrwhqj"
    "7FO5L22sNvkZZnacLvmfNwsAAABZeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMTQ1ODc2NjI3N30=";
const uint8_t kSampleTokenV2Signature[] = {
    0x9f, 0x90, 0xfd, 0x09, 0xb4, 0x10, 0xb6, 0x9d, 0x66, 0xa9, 0x7e,
    0x76, 0x51, 0x06, 0x4b, 0x09, 0xc0, 0x56, 0xc1, 0x59, 0x2a, 0x00,
    0x84, 0xb5, 0x46, 0x60, 0xf2, 0x27, 0x50, 0x0b, 0x7b, 0x9e, 0x92,
    0x42, 0x1e, 0x49, 0x92, 0x18, 0xd6, 0xd7, 0xed, 0xa1, 0x87, 0x6b,
    0xc2, 0x1a, 0xa3, 0xec, 0x53, 0xb9, 0x2f, 0x6d, 0xac, 0x36, 0xf9,
    0x19, 0x66, 0x76, 0x9c, 0x2e, 0xf9, 0x9f, 0x37, 0x0b};

// This is a good trial token, signed with the above test private key.
// Generate this token with the command (in tools/origin_trials):
// generate_token.py valid.example.com 3 Frobulate --expire-timestamp=1458766277
const char kSampleTokenV3[] =
    "A79AvyC9SLsjuRTUsjIeGmEfw8Ow0pZSoFtHs8qtrAhUKSNbluCYo86D4M3F6bco"
    "F2BOyjyI7mEWztV+HQvxUAsAAABZeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMTQ1ODc2NjI3N30=";
const uint8_t kSampleTokenV3Signature[] = {
    0xbf, 0x40, 0xbf, 0x20, 0xbd, 0x48, 0xbb, 0x23, 0xb9, 0x14, 0xd4,
    0xb2, 0x32, 0x1e, 0x1a, 0x61, 0x1f, 0xc3, 0xc3, 0xb0, 0xd2, 0x96,
    0x52, 0xa0, 0x5b, 0x47, 0xb3, 0xca, 0xad, 0xac, 0x08, 0x54, 0x29,
    0x23, 0x5b, 0x96, 0xe0, 0x98, 0xa3, 0xce, 0x83, 0xe0, 0xcd, 0xc5,
    0xe9, 0xb7, 0x28, 0x17, 0x60, 0x4e, 0xca, 0x3c, 0x88, 0xee, 0x61,
    0x16, 0xce, 0xd5, 0x7e, 0x1d, 0x0b, 0xf1, 0x50, 0x0b};

// This is a good subdomain trial token, signed with the above test private key.
// Generate this token with the command (in tools/origin_trials):
// generate_token.py 2 example.com Frobulate --is-subdomain
//   --expire-timestamp=1458766277
const char kSampleSubdomainToken[] =
    "Auu+j9nXAQoy5+t00MiWakZwFExcdNC8ENkRdK1gL4OMFHS0AbZCscslDTcP1fjN"
    "FjpbmQG+VCPk1NrldVXZng4AAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxl"
    "LmNvbTo0NDMiLCAiaXNTdWJkb21haW4iOiB0cnVlLCAiZmVhdHVyZSI6ICJGcm9i"
    "dWxhdGUiLCAiZXhwaXJ5IjogMTQ1ODc2NjI3N30=";
const uint8_t kSampleSubdomainTokenSignature[] = {
    0xeb, 0xbe, 0x8f, 0xd9, 0xd7, 0x01, 0x0a, 0x32, 0xe7, 0xeb, 0x74,
    0xd0, 0xc8, 0x96, 0x6a, 0x46, 0x70, 0x14, 0x4c, 0x5c, 0x74, 0xd0,
    0xbc, 0x10, 0xd9, 0x11, 0x74, 0xad, 0x60, 0x2f, 0x83, 0x8c, 0x14,
    0x74, 0xb4, 0x01, 0xb6, 0x42, 0xb1, 0xcb, 0x25, 0x0d, 0x37, 0x0f,
    0xd5, 0xf8, 0xcd, 0x16, 0x3a, 0x5b, 0x99, 0x01, 0xbe, 0x54, 0x23,
    0xe4, 0xd4, 0xda, 0xe5, 0x75, 0x55, 0xd9, 0x9e, 0x0e};

// This is a good trial token, explicitly not a subdomain, signed with the above
// test private key. Generate this token with the command:
// generate_token.py 2 valid.example.com Frobulate --no-subdomain
//   --expire-timestamp=1458766277
const char kSampleNonSubdomainToken[] =
    "AreD979D7tO0luSZTr1+/+J6E0SSj/GEUyLK41o1hXFzXw1R7Z1hCDHs0gXWVSu1"
    "lvH52Winvy39tHbsU2gJJQYAAABveyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiaXNTdWJkb21haW4iOiBmYWxzZSwgImZlYXR1cmUi"
    "OiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6IDE0NTg3NjYyNzd9";
const uint8_t kSampleNonSubdomainTokenSignature[] = {
    0xb7, 0x83, 0xf7, 0xbf, 0x43, 0xee, 0xd3, 0xb4, 0x96, 0xe4, 0x99,
    0x4e, 0xbd, 0x7e, 0xff, 0xe2, 0x7a, 0x13, 0x44, 0x92, 0x8f, 0xf1,
    0x84, 0x53, 0x22, 0xca, 0xe3, 0x5a, 0x35, 0x85, 0x71, 0x73, 0x5f,
    0x0d, 0x51, 0xed, 0x9d, 0x61, 0x08, 0x31, 0xec, 0xd2, 0x05, 0xd6,
    0x55, 0x2b, 0xb5, 0x96, 0xf1, 0xf9, 0xd9, 0x68, 0xa7, 0xbf, 0x2d,
    0xfd, 0xb4, 0x76, 0xec, 0x53, 0x68, 0x09, 0x25, 0x06};

// This is a good third party trial token, signed with the above test private
// key. Generate this token with the command (in tools/origin_trials):
// generate_token.py 3 example.com Frobulate --is-third-party
//   --expire-timestamp=1458766277
const char kSampleThirdPartyToken[] =
    "A9+2NjaYsaFkwtULzbWjcsSJiXD0LuoOgma9fET8hq1uEqVcNyqjGH4ExpF7mYUk"
    "ireYovWqOwsZEyiX6eodfw4AAABveyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZSwgImZlYXR1cmUi"
    "OiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6IDE0NTg3NjYyNzd9";
const uint8_t kSampleThirdPartyTokenSignature[] = {
    0xdf, 0xb6, 0x36, 0x36, 0x98, 0xb1, 0xa1, 0x64, 0xc2, 0xd5, 0x0b,
    0xcd, 0xb5, 0xa3, 0x72, 0xc4, 0x89, 0x89, 0x70, 0xf4, 0x2e, 0xea,
    0x0e, 0x82, 0x66, 0xbd, 0x7c, 0x44, 0xfc, 0x86, 0xad, 0x6e, 0x12,
    0xa5, 0x5c, 0x37, 0x2a, 0xa3, 0x18, 0x7e, 0x04, 0xc6, 0x91, 0x7b,
    0x99, 0x85, 0x24, 0x8a, 0xb7, 0x98, 0xa2, 0xf5, 0xaa, 0x3b, 0x0b,
    0x19, 0x13, 0x28, 0x97, 0xe9, 0xea, 0x1d, 0x7f, 0x0e};

// This is a good trial token, explicitly not a third party, signed with the
// above test private key. Generate this token with the command:
// generate_token.py 3 valid.example.com Frobulate --no-third-party
//   --expire-timestamp=1458766277
const char kSampleNonThirdPartyToken[] =
    "Ay0uBIEXlhMfvS43Z+m8bgeqnnZq27xV4OG13d+bkyGuCKx6Wa+hSkLkk6OStg+D"
    "l8pRdqUG19BhWnizn5TbKAMAAABweyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiaXNUaGlyZFBhcnR5IjogZmFsc2UsICJmZWF0dXJl"
    "IjogIkZyb2J1bGF0ZSIsICJleHBpcnkiOiAxNDU4NzY2Mjc3fQ==";
const uint8_t kSampleNonThirdPartyTokenSignature[] = {
    0x2d, 0x2e, 0x04, 0x81, 0x17, 0x96, 0x13, 0x1f, 0xbd, 0x2e, 0x37,
    0x67, 0xe9, 0xbc, 0x6e, 0x07, 0xaa, 0x9e, 0x76, 0x6a, 0xdb, 0xbc,
    0x55, 0xe0, 0xe1, 0xb5, 0xdd, 0xdf, 0x9b, 0x93, 0x21, 0xae, 0x08,
    0xac, 0x7a, 0x59, 0xaf, 0xa1, 0x4a, 0x42, 0xe4, 0x93, 0xa3, 0x92,
    0xb6, 0x0f, 0x83, 0x97, 0xca, 0x51, 0x76, 0xa5, 0x06, 0xd7, 0xd0,
    0x61, 0x5a, 0x78, 0xb3, 0x9f, 0x94, 0xdb, 0x28, 0x03};

// This is a good third party trial token with usage restriction set to subset,
// signed with the above test private key. Generate this token with the
// command:
// generate_token.py valid.example.com Frobulate --version 3 --is-third-party
//   --expire-timestamp=1458766277 --usage-restriction subset
const char kSampleThirdPartyUsageSubsetToken[] =
    "A27Ee1Bm6HYjEu2Zz1DbGNUaPuM8x0Tnk15Gyx8TRKZg72+JUXgCccMxlLIjVh4l"
    "enOES58tfJxrRCorBAKmBwcAAACCeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZSwgInVzYWdlIjog"
    "InN1YnNldCIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZSIsICJleHBpcnkiOiAxNDU4"
    "NzY2Mjc3fQ==";
const uint8_t kSampleThirdPartyUsageSubsetTokenSignature[] = {
    0x6e, 0xc4, 0x7b, 0x50, 0x66, 0xe8, 0x76, 0x23, 0x12, 0xed, 0x99,
    0xcf, 0x50, 0xdb, 0x18, 0xd5, 0x1a, 0x3e, 0xe3, 0x3c, 0xc7, 0x44,
    0xe7, 0x93, 0x5e, 0x46, 0xcb, 0x1f, 0x13, 0x44, 0xa6, 0x60, 0xef,
    0x6f, 0x89, 0x51, 0x78, 0x02, 0x71, 0xc3, 0x31, 0x94, 0xb2, 0x23,
    0x56, 0x1e, 0x25, 0x7a, 0x73, 0x84, 0x4b, 0x9f, 0x2d, 0x7c, 0x9c,
    0x6b, 0x44, 0x2a, 0x2b, 0x04, 0x02, 0xa6, 0x07, 0x07};

// This is a good third party trial token with usage restriction set to none,
// signed with the above test private key. Generate this token with the
// command:
// generate_token.py valid.example.com Frobulate --version 3 --is-third-party
//   --expire-timestamp=1458766277 --usage-restriction ""
const char kSampleThirdPartyUsageEmptyToken[] =
    "A+gXf6yZgfN8NADWvnEhQ/GKycwCg34USmDlQ9UXTP6jDGJLBV+jI1npSUI0W/YW"
    "hNyNYbzBaE2iCJSGCD56pwwAAAB8eyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZSwgInVzYWdlIjog"
    "IiIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZSIsICJleHBpcnkiOiAxNDU4NzY2Mjc3"
    "fQ==";
const uint8_t kSampleThirdPartyUsageEmptyTokenSignature[] = {
    0xe8, 0x17, 0x7f, 0xac, 0x99, 0x81, 0xf3, 0x7c, 0x34, 0x00, 0xd6,
    0xbe, 0x71, 0x21, 0x43, 0xf1, 0x8a, 0xc9, 0xcc, 0x02, 0x83, 0x7e,
    0x14, 0x4a, 0x60, 0xe5, 0x43, 0xd5, 0x17, 0x4c, 0xfe, 0xa3, 0x0c,
    0x62, 0x4b, 0x05, 0x5f, 0xa3, 0x23, 0x59, 0xe9, 0x49, 0x42, 0x34,
    0x5b, 0xf6, 0x16, 0x84, 0xdc, 0x8d, 0x61, 0xbc, 0xc1, 0x68, 0x4d,
    0xa2, 0x08, 0x94, 0x86, 0x08, 0x3e, 0x7a, 0xa7, 0x0c};

const char kExpectedFeatureName[] = "Frobulate";
// This is an excessively long feature name (100 characters). This is valid, as
// there is no explicit limit on feature name length. Excessive refers to the
// fact that is very unlikely that a developer would choose such a long name.
const char kExpectedLongFeatureName[] =
    "ThisTrialNameIs100CharactersLongIncludingPaddingAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAA";
const char kExpectedOrigin[] = "https://valid.example.com";
const char kExpectedSubdomainOrigin[] = "https://example.com";
const char kExpectedMultipleSubdomainOrigin[] =
    "https://part1.part2.part3.example.com";
const uint64_t kExpectedExpiry = 1458766277;

// The token should not be valid for this origin, or for this feature.
const char kInvalidOrigin[] = "https://invalid.example.com";
const char kInsecureOrigin[] = "http://valid.example.com";
const char kIncorrectPortOrigin[] = "https://valid.example.com:444";
const char kIncorrectDomainOrigin[] = "https://valid.example2.com";
const char kInvalidTLDOrigin[] = "https://com";
const char kInvalidFeatureName[] = "Grokalyze";

// The token should be valid if the current time is kValidTimestamp or earlier.
double kValidTimestamp = 1458766276.0;

// The token should be invalid if the current time is kInvalidTimestamp or
// later.
double kInvalidTimestamp = 1458766278.0;

// Well-formed trial token with an invalid signature.
const char kInvalidSignatureToken[] =
    "Ap+Q/Qm0ELadZql+dlEGSwnAVsFZKgCEtUZg8idQC3uekkIeSZIY1tftoYdrwhqj"
    "7FO5L22sNvkZZnacLvmfNwsAAABaeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGV4IiwgImV4cGly"
    "eSI6IDE0NTg3NjYyNzd9";

// Trial token truncated in the middle of the length field; too short to
// possibly be valid.
const char kTruncatedToken[] =
    "Ap+Q/Qm0ELadZql+dlEGSwnAVsFZKgCEtUZg8idQC3uekkIeSZIY1tftoYdrwhqj"
    "7FO5L22sNvkZZnacLvmfNwsA";

// Trial token with an incorrectly-declared length, but with a valid signature.
const char kIncorrectLengthToken[] =
    "Ao06eNl/CZuM88qurWKX4RfoVEpHcVHWxdOTrEXZkaC1GUHyb/8L4sthADiVWdc9"
    "kXFyF1BW5bbraqp6MBVr3wEAAABaeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMTQ1ODc2NjI3N30=";

// Trial token with a misidentified version (42).
const char kIncorrectVersionToken[] =
    "KlH8wVLT5o59uDvlJESorMDjzgWnvG1hmIn/GiT9Ng3f45ratVeiXCNTeaJheOaG"
    "A6kX4ir4Amv8aHVC+OJHZQkAAABZeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMTQ1ODc2NjI3N30=";

const char kSampleTokenJSON[] =
    "{\"origin\": \"https://valid.example.com:443\", \"feature\": "
    "\"Frobulate\", \"expiry\": 1458766277}";

const char kSampleNonSubdomainTokenJSON[] =
    "{\"origin\": \"https://valid.example.com:443\", \"isSubdomain\": false, "
    "\"feature\": \"Frobulate\", \"expiry\": 1458766277}";

const char kSampleSubdomainTokenJSON[] =
    "{\"origin\": \"https://example.com:443\", \"isSubdomain\": true, "
    "\"feature\": \"Frobulate\", \"expiry\": 1458766277}";

const char kUsageEmptyTokenJSON[] =
    "{\"origin\": \"https://valid.example.com:443\", \"usage\": \"\", "
    "\"feature\": \"Frobulate\", \"expiry\": 1458766277}";

const char kUsageSubsetTokenJSON[] =
    "{\"origin\": \"https://valid.example.com:443\", \"usage\": \"subset\", "
    "\"feature\": \"Frobulate\", \"expiry\": 1458766277}";

const char kSampleNonThirdPartyTokenJSON[] =
    "{\"origin\": \"https://valid.example.com:443\", \"isThirdParty\": false, "
    "\"feature\": \"Frobulate\", \"expiry\": 1458766277}";

const char kSampleThirdPartyTokenJSON[] =
    "{\"origin\": \"https://valid.example.com:443\", \"isThirdParty\": true, "
    "\"feature\": \"Frobulate\", \"expiry\": 1458766277}";

const char kSampleThirdPartyTokenUsageSubsetJSON[] =
    "{\"origin\": \"https://valid.example.com:443\", \"isThirdParty\": true, "
    "\"usage\": \"subset\", \"feature\": \"Frobulate\", \"expiry\": "
    "1458766277}";

const char kSampleThirdPartyTokenUsageEmptyJSON[] =
    "{\"origin\": \"https://valid.example.com:443\", \"isThirdParty\": true, "
    "\"usage\": \"\", \"feature\": \"Frobulate\", \"expiry\": 1458766277}";

// Various ill-formed trial tokens. These should all fail to parse.
const char* kInvalidTokens[] = {
    // Empty String
    "",
    // Invalid - Not JSON at all
    "abcde",
    // Invalid JSON
    "{",
    // Not an object
    "\"abcde\"",
    "123.4",
    "[0, 1, 2]",
    // Missing keys
    "{}",
    "{\"something\": 1}",
    "{\"origin\": \"https://a.a\"}",
    "{\"origin\": \"https://a.a\", \"feature\": \"a\"}",
    "{\"origin\": \"https://a.a\", \"expiry\": 1458766277}",
    "{\"feature\": \"FeatureName\", \"expiry\": 1458766277}",
    // Incorrect types
    "{\"origin\": 1, \"feature\": \"a\", \"expiry\": 1458766277}",
    "{\"origin\": \"https://a.a\", \"feature\": 1, \"expiry\": 1458766277}",
    "{\"origin\": \"https://a.a\", \"feature\": \"a\", \"expiry\": \"1\"}",
    "{\"origin\": \"https://a.a\", \"isSubdomain\": \"true\", \"feature\": "
    "\"a\", \"expiry\": 1458766277}",
    "{\"origin\": \"https://a.a\", \"isSubdomain\": 1, \"feature\": \"a\", "
    "\"expiry\": 1458766277}",
    // Negative expiry timestamp
    "{\"origin\": \"https://a.a\", \"feature\": \"a\", \"expiry\": -1}",
    // Origin not a proper origin URL
    "{\"origin\": \"abcdef\", \"feature\": \"a\", \"expiry\": 1458766277}",
    "{\"origin\": \"data:text/plain,abcdef\", \"feature\": \"a\", \"expiry\": "
    "1458766277}",
    "{\"origin\": \"javascript:alert(1)\", \"feature\": \"a\", \"expiry\": "
    "1458766277}",
};

const char* kInvalidTokensVersion3[] = {
    // Incorrect types
    "{\"origin\": \"https://a.a\", \"isThirdParty\": \"true\", \"feature\": "
    "\"a\", \"expiry\": 1458766277}",
    "{\"origin\": \"https://a.a\", \"isThirdParty\": 1, \"feature\": \"a\", "
    "\"expiry\": 1458766277}",
    // Invalid value in usage field
    "{\"origin\": \"https://a.a\", \"isThirdParty\": true, \"usage\": "
    "\"cycle\", \"feature\": \"a\", "
    "\"expiry\": 1458766277}",
};

// Valid token JSON. The feature name matches matches kExpectedLongFeatureName
// (100 characters), and the origin is 2048 chars.
const char kLargeTokenJSON[] =
    "{\"origin\": "
    "\"https://"
    "www."
    "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.com:9999\","
    // clang-format off
    "\"isSubdomain\": true,"
    "\"feature\": \"ThisTrialNameIs100CharactersLongIncludingPaddingAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\","
    "\"expiry\": 1458766277}";
// clang-format on

const char kExpectedLongTokenOrigin[] =
    "https://"
    "www."
    "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.com:9999";

// Valid token JSON, over 4KB in size. The feature name matches
// kExpectedLongFeatureName (100 characters), and the origin is 3929 chars.
const char kTooLargeTokenJSON[] =
    "{\"origin\": "
    "\"https://"
    "www."
    "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.com:9999\","
    // clang-format off
    "\"isSubdomain\": true,"
    "\"feature\": \"ThisTrialNameIs100CharactersLongIncludingPaddingAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\","
    "\"expiry\": 1458766277}";
// clang-format on

// Large valid token, size = 3052 chars. The feature name matches
// kExpectedLongFeatureName (100 characters), and the origin is 2048 chars.
// Generate this token with the command:
// generate_token.py --is-subdomain --expire-timestamp=1458766277 \
//   2 https://www.<2027 random chars>.com:9999 \
//   ThisTrialNameIs100CharactersLongIncludingPaddingAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
const char kLargeValidToken[] =
    "Aq1wGS1wFPBT/"
    "S0tRbXIhO6fntc3GuDacAPAcfTBMxkdXpgXJMERVcVEVNfZAu1laKHhMUjTp0pOIBi/"
    "KyWXrAQAAAireyJvcmlnaW4iOiAiaHR0cHM6Ly93d3cuYWFhcHB0eXFpbm0wcGFsMWJpZnlpdH"
    "Vkb2h4dThzbWh2Zmx4aTVnbmdsYTVzbWtwbmw2MnFpOWdscWlmanBpZ3V4bDlweHVkYXN6YnVz"
    "Y3Ric3htaTd2MXRyYnhsdWd1YTdpeGp2dXZxM2ZoejVocnVmZmhmN2RocGVxeWN6Zjdzd3Zmcn"
    "hscDBseWhpaHZ0bno3Mnl3a2FucWlqeGZjZ2l4M3d6Z25za3lhODlma2plaGgzZ3h6Y3dmY3Fn"
    "aGI4NnZvNjFxeGhtNG5vc211ZXYyeGN0ajlkNG5hZHR4c3k1bHVld3RjZW5tM2pibWxsd3J1dW"
    "hscGx2ejBmbXdpcm9leGdxc3IxbzJsaTRwYmF5eTJvamt2b3dhaGZ5Z3h1eTJwOWVlYW5zeXNp"
    "YXQ5dzNrdzI1NmVuajRoeHZjZ3A1andleGJnN2liZHk1a2Y1bWlsY2hlZXp4OHhlcnN6Znc2ZW"
    "RweHlzOG15bGl2bzZqcm9ja2JncmhhcWVzZmFod2JycnJoemZzamV4cm13bDF3ZmU1amFub3Rr"
    "cmFidXR2a29tdXJhYXAwMXh0eXJjZThjdW9mZXRtdmFyaGZsYng5dnBhdTB3ZXZvbmV3MG95dz"
    "gya3RlamtwNG9kc3NyM2h2cmh2NGx6ZmZodnBnc2hmbWxoaHl3NWdwejExdzB1ZmhjdzF5N2Fy"
    "MGIwemVnYjZtZ3djYnJ1eGZhNDJmcXJscW50dWdocm9yZzdpNWV0cGhhZ3k0MXNydnUwbmJrdz"
    "k5ZmxybXl4M2JyZTB6a3EyZmtwbGNibzVrZHg3YmQyZWNtd21veXp6c2R1ZnB1dXlucHhlMW13"
    "MGd3NnVoeGljODdsb3VqZXd3Z3RtcXFyOXJlYXkxdjlhZ2dka2dhems0aGJka29yaXVsODhvaH"
    "l1d2h1N2JqbW95aHB5c3Zsb2gxZWZvbm9ybXA1NDFuanRzMXhyZWpvNWo2d21pN2J3bW5obHd5"
    "djJsaWdzdGFkY2k0YXR4MGJpdHJkYmVqYXJ0YnExZGRlNGlmb21wcjBrdHJsdHA5aWNjdm9mYW"
    "ZrYzBjY3RjZWM4NmtmaG5meHNjcjZjcWlqcnV2aWtrdWJva2Zsc2hpcWV1emZ4aTJvbHNjYWUz"
    "a2M5MDJpbjZmOW5qaTNrZGl3bzB2dGN3MzJrbm5qaXhuZm54MjNtOXN2emd6cnFmYmNpYjk2bj"
    "JvZzdzdXhzZTZ2Y2U5a3Q1ZW1qeTVvYXFmcnpjZ3lvaXB3MGdmZWp1b2pvbXlqb3hzb2Nod3l3"
    "eHg1Z2UyMmJndnVrenl2eThqbHk2NmwwaWdwODFvZXI2d2t1bjh5NnljdGR5bnRqZGtsZnF6NH"
    "dqeG14eWduY3RnaGtneDF2eXJmeHlqcTAwNXprdnVkaDBxZnZqc2JyYmo0bng1bTd2Zm1rdnV6"
    "b2k5a2Znb2djMnFhZ294eGFzb3JydXJtZzN2dG5pd3FxbTI4emZoOGp2bTR1MnZhMGhsb2huaW"
    "81dGl0empxenp3dmxiZHpmN3pseXdsZ2JpbHB5eG5tbWlpcnk4Zzdpc3o0dm0zc3ZuanR5YXlj"
    "bm9wOWhzd25ta3Njc2E1NmwwZTlqcjN5eXk5dzd0d2pycTdmcnFkempraHNyMGl1NmFucmFtbW"
    "F5d2dscmdwb29ucTg0OWtvaG8zNWx0aDBzeml1ZzRrb2w4aTdicWtncWNmeG1rNnBoMGZuMjdn"
    "Ymdsdml4dG9ucW14eGhoYzI5bGNodmhraWY4eXZkdmVkYXVybmt4dXB5bHN6N2NucGl2YjR3Ym"
    "dya3JheWhpdzIwaGZiMWtjNGdudGtxaHBpbnR6c21pNDQ3ZnkyOWx5andsamJuc3hhaG1kYWVp"
    "amVnYmM0aGZwbGZhOHdkMHE0cWpieHR1ZnNjaWJiamY2YXNnYjV2dWV6Y2FwenZ4bXhpdGVhc3"
    "R4aXVpenVlaDV4cncxbHk3aHBzYnliaGltbmp6cDB4cDZwb2thc2hlYmhpcTMxZnZnbWVvdG9n"
    "NHY0M2huc3Uzenp0bGRveGxobG9teGttNmZndHh3OHhtOHQ0dTJtemNmMWFwZXNxdXF3ejV5d3"
    "ludXUwb2JyZWkycWlwcW5sdWVzaG01c3AxMndscTZoOW4wdTZ3cmF1cXRkN2hrZnJ1bmFleWVs"
    "NWphNnptZWp6cGJkeXEyNnRncGo5eXY4bThtZXducmZxd29zZ2Y3aG5oNHI3aXZ5OGN4NXgxcG"
    "lpd3NvZHh2ODVjeWltczU3MTE2Zm56YWcya3h0dG16cTc1dXJ6MWEydDBhazR5empxejRkNjZi"
    "ZGJ5dmU0bmJoc2VsN2NwN2h3bnQ4NWIya3RiOGlwZG95Mmc1dmx0b29ydXB5NDN5aWhxdWd0cG"
    "dhcGo2eXIzOWd5eGNlaW9vZ284cGM2c2lqZ2ZoYml1NmZ1aGIwcW9jeGprcHJvOHlycXphaXR3"
    "d3l3b2xnZ2Fzbnl0bnhlazV3aHhwd3ZnMHA1aWRrbHBnNWZjaHlpbXJmYWx1Yzl2N3U0dWJvbW"
    "hkaWlyajF1YjBnaTRlc2dxazhjdHJiajByOXBpNGM3eGoyMTR2bWQ2Mzhub3JxcDNtaGpob2Yw"
    "NHVxbnJjaDRmbnN5emhyZmJlczZhY2VodW9qc2hucjd0OTFvam8xdno1ZHF2am83NzY5amJ5bn"
    "p6dW02amkuY29tOjk5OTkiLCAiaXNTdWJkb21haW4iOiB0cnVlLCAiZmVhdHVyZSI6ICJUaGlz"
    "VHJpYWxOYW1lSXMxMDBDaGFyYWN0ZXJzTG9uZ0luY2x1ZGluZ1BhZGRpbmdBQUFBQUFBQUFBQU"
    "FBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUEiLCAiZXhwaXJ5IjogMTQ1"
    "ODc2NjI3N30=";
const uint8_t kLargeValidTokenSignature[] = {
    0xad, 0x70, 0x19, 0x2d, 0x70, 0x14, 0xf0, 0x53, 0xfd, 0x2d, 0x2d,
    0x45, 0xb5, 0xc8, 0x84, 0xee, 0x9f, 0x9e, 0xd7, 0x37, 0x1a, 0xe0,
    0xda, 0x70, 0x03, 0xc0, 0x71, 0xf4, 0xc1, 0x33, 0x19, 0x1d, 0x5e,
    0x98, 0x17, 0x24, 0xc1, 0x11, 0x55, 0xc5, 0x44, 0x54, 0xd7, 0xd9,
    0x02, 0xed, 0x65, 0x68, 0xa1, 0xe1, 0x31, 0x48, 0xd3, 0xa7, 0x4a,
    0x4e, 0x20, 0x18, 0xbf, 0x2b, 0x25, 0x97, 0xac, 0x04};

// Valid token that is too large, size = 4100 chars. The feature name matches
// kExpectedLongFeatureName (100 characters), and the origin is 2833 chars.
// Generate this token with the command:
// generate_token.py --is-subdomain --expire-timestamp=1458766277 \
//   2 https://www.<4348 random chars>.com:9999 \
//   ThisTrialNameIs100CharactersLongIncludingPaddingAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
const char kTooLargeValidToken[] =
    "AswTUJEo9qF5QaVITkRzMQ+muYHK13+"
    "IFGmg6ZGTiAlHtdIzOS0Kbngkgk3OM43Z8sRVCVq6bl7lCGrjmG3l/"
    "gUAABG8eyJvcmlnaW4iOiAiaHR0cHM6Ly93d3cuYWFwMDdobW9yendvY254bGZnZ3N1ZmplbWY"
    "zamViaDh2MzQxMnBxb3ZzbGh0Y2IyM3ppejVhaG1nZTh3aG03eGZzb2ExdmFuNXhqb2poZ3Jje"
    "nphdng1Mm5raW1zd3ZrdmpvMXFzZmc0dHAxc2NtY3dvNjU3Z3diNG0zdWl6cTJtNHhianpoemp"
    "vMjA1aTltdHNrN2ZrMmdpYXNycmwwNGt0a2xnbnc0aHJkcThib3NnamdvcDA2dWt3ZXY5dTJhe"
    "HZ3a2VuaDY5bjhyNjEzemhtaG5nY3Qxcm5lbGNoa2ZyMXBmcndoN3NnZTlnbGRudHZ5Y296NnM"
    "3YWg1b2dyZWdtOHoxNzduY3p3Y2FsM3k5dnFhaWhrdjN3cTJ0MnZqaWZ1MXZiY244aGd4b3Y2N"
    "mVhNTB2eWdsdXNiazBwcXRqaWFibGdmaWxweHNmeDh3cWJ0ajllaTl4cjJsaXRydXBjbWloeWV"
    "leGl6aHBtNzltcG9zeHp0eDB0eWd0b3hwMWN0Y2JkdHpwbGVydmN5dTdlbnZia2R4OGNweGNjM"
    "jJzYjZhdm93YnZ1ZWhiaXJkdnBrbXp6anV1b2NnY3p5ZW5iNmdnbGU2NjJnemU1c29jN2NwaDN"
    "5ZDlpajIwYXdlbGYwd3pseWpjMnN6eDQwZ2NteWJlMnhxeGF2dXo4YWxiMWRscnp1eXNpeDVxN"
    "mt0NXV1ZHIweHRvbmpwb3Ftem5jaGJtaG0wb3AyNnc5bGQ2amlxY3FwdWo1ZWl2Y3d6amM5OHN"
    "nNGhlMXl4cWhqeHdyZ2hnczlqaDFzaHJ6aXU5ZmllcTl5aHI4NHlhdjRxdWxrdGdmdW5hczF5Z"
    "WF1bGw0MHJ4a2lmOGdmenptaHZwdmo1enhlYXF6bXN0cDZmZmtseDV0bzByamdtajlhb3BueTF"
    "6enhsbWpqZW5uMW9ranIwM2Uxdm54bG9tYTl4dDNwd2FpdHp6bjFtbmJucmVhaGxyd3oyZXFvc"
    "HBhMHhna3Vlb3RrMmdqbnN6eXl1d2Vla3lic245OXF4eDU0d3FheHNwNzR2Z2IxMm05bnllbGx"
    "0dHdhbWlvOWpnMHd6bTFwbWo2YTNkaHVxMHlscmVzNXhnbHZsanlrbWZvMml6dGZ3amViZG1md"
    "Hl4OHh0ZWxnN3Q5d3phYW11eXV1dzR6NXprMmhwYmxpd3Z2b2NwODg2YThkYm9zdjVlZmszamd"
    "renpxZ3ZvMHphcDJ4ZTlzOWp2YW81bjVreHpjemp6OGl6MW95NTZkYnh2cTluYzlmN2ZwdXhsO"
    "GI1N2o3a3ZlNW5qenBwa2FhcGJ2cHZ6NWU3enFlcmw3ZjJ3cGZ4eDNjbXZ6YmdqZXQxcHpjOGl"
    "2cXhmbWpob3ZsbzVzcXk4cXM1djh0Y3p3YnlmZXFrejBrNmVha2Z5Ym10ZmJvZnp4ODF6ZXNvO"
    "G1oeHNzMXdhem11ZnR4cXhiY3d4c2ZwemUzems5Zzk2ZHZlaG51ajdncGpscnN6Z3c0NnhmN2w"
    "xdHVuYWhmdnZhMXBzenhldmFiOTlzbWx5bDZvNDBhbjFoZ3c0Z2txa2Job2pxdHljZ2NzaGRrd"
    "HZta3J2cm9pYm9sYWs3OGJvbWdvZXFqMmN5bXJ6dmlkbHUzYnp4bm95ZHdnZzd0enFhd216cWh"
    "ldnh6c2Y4eWE0eHpsamJxa3lnbDlsYmEyMXBucnNwOG9lcmh4dm1leHV5eHZhNGlmc3FvN2s2d"
    "zY0cm1hdmdmdjc1eXZ5NWFhbXlzeG96eWU4bnF6aTYwdzR2MXhnYXdjOXY2eW1hazJvczdxNWw"
    "2enJibDJpYmZnemZhendhNHI1N2xndmY5NWRwajN4d2NjYW03bzFzYXRicXp3NTNnbXZ0cGJsb"
    "GM2OGhueHBnem1menhhbHJ3dG5vMXJjbXF4ZTJ5azVoaDN4ZmE0d3o5cG5xamtmY290ZXhiZzF"
    "4Z3N3anB6YnhvNGdxMmc4bTd2aGtkMHpuMWpmZWlrd3l2N21ia2t0anh3NHo3c21scnAzMzM4d"
    "3JvN2s1cWtoYmtvYTIwd2Rma2g4MGFjemY5dWhldzlhdmMzOGl1dGI5c29mamlmYmJmdTBiNWh"
    "kaDl3a2NuaDd1NWJmajVlMTZwZnpqamlsZ2FybzV0M2F6dHd6MnRuZmk3cDhsc2xvZGhuOGp6c"
    "WJyOXhuMjN4Zmw1dWhucmxzczFkam5pbWljc2FiemRlYWxid2s2Ymc3am1sZm0yeXZ6cDFyY2x"
    "5OXZ2cjZ1Y2Z2Zmxpa3hrcW1senpkNXNpZjR2Z2JpczQxY3Nwc2N6bnR0dnd5b2Juc2RuZndnc"
    "2dka2gwbWUzNGpsaTh0b3FxaHh5c211dzJlM29wZ20waGluY2JtZ3hxanhvdGJvdXlyNHJ5dG5"
    "uemlwYnRxcWlzZXQwcWxkdWFwcnFtdWdvMTFub29rc2dyenpnZTd2a3p3N3F2eWF1b2V6eHMyc"
    "XlrNHM5cm9pdmlseGpiazVtZmp0Y2JrdXZ1c2FtYzd0aGFkYXVmcHZheHRnZXJjZ3Bjcnh3eWd"
    "4djZ3dnE1d3kyd3ZmZDNqd3Nza3NvY2xod2ZneGpiYWhydWhyeWd1NnZmZ3ZuN3ZueWN2Mmd3b"
    "mdoaXBieDFubnRuc3M4cTZwMzl2bnVua3UyZWtudWJwbmpwYXd6bjJwZmRzcnA2NnM3YWZhYWd"
    "pam10eXNlZ2U0eXV0eGpnODR3cms4dTZnOHdkbWtjaG9zeWJvNnJibnNtY3RsdzMzZWF5Z3duZ"
    "DZvZ2ppeG9ocG5uNmFoZzIycm5tbHd2NXBnaXpleXRvN3dhMWVnaXB1dHVxdGxpYTVwdmRzdXd"
    "pamtqeWUxbjhoangxdGpsa3lrZTJ3bWMyYWhkcmh5Nml6ajJsY2VremJ4MWhxYmFzYzV6emJ0c"
    "zN3bGIwbWJ0ZHFwYXYyeHd6bWdnMXo1cnNwaGYwbHZpbGdwb2FnZWl1cXJzNWpuZ3hzeW9hcXg"
    "yY3ZodHZva3g0bWc2bmh2bm5mYnhuMG0zcmU3N2FqeHNsd3IwdmxqMDN0bG11dnJpYmxoNGRld"
    "zJxamRxbW5jbmU0dWZ0Y2dvdHNidXdjd3p2ejNld291emtyZXdrNWx3c29waXVlN3k0amYzbnR"
    "1eHV0b3hiYTV4b29lNGpocnV4cmdrZmI4bmxubXFrNmp2bHFmMG9jM2Nzb3J5cnZzbTNxc2l5e"
    "mZwYmlkYXpxdnlraTl2YXFldHI5ODdlbmpmNzB1aWx4eHJhc3Jha3NjcjZxNmprdmJtaXBrY2R"
    "lbHlkZ3B5OGJqbWN2dW15d21qaGNyZ3h1enB4ZThhZXpiZWdlaDhidnlzOGJudml5b29lcTA1Y"
    "jAyejdpY2ljbTJ3ZnY1cmNwcDdjd3QzdXVzMWNzdG5yNnJtb2cxZWZqaGlwbjM0M2I1Z2hpYXZ"
    "oeGc1cWM0ODVvbDM5ZWJ0bWZxb296bmJicnZienlqc2poZ2tmcTdzMXJ3c2hieHpsYmVoenhxd"
    "G41d21zb2h2YWh0dW9jbHo1cDJsb3JldXl5MG9yYzR6c3E5bWNlZ3dibnltYnJ6Y2d5Z20xcnV"
    "qeXJiOGN1bWY1YnRtcDY3Ynl6NmE3MDAyc2kwM3hua2Jyb3J2c2Vvams5enNpeG5kOWp1YmV0O"
    "XpyeXBoY2Z2cWVyOWp1cm1nbWJoazBwOG9xdDVudGh6a2pkY2FtOWpvbDJzYTlubWRrdXc0ZzR"
    "2ZnZ5Y3k2ZmJ4Ynd1MGphcGtjYzFzZmJqazQxM2licGpqeGljZXlweGp3cmxjZHd0ZXBjZXNkY"
    "3FocHo4dGNwbDYzczRrOW5ubmNldm5hajA2OGhsY3N6NWRxb3pneHN0NTVyZ21tMWcxaXd4eHg"
    "3YWh0a3psdG9xMTJpZm1temp1d3ozcHlvd2RmaDdta29jYzFxdXR4c254eTN1NHZpcDZiYzNtd"
    "280aXd5aWVtNnowbXZuanpqY3l1MnZhbnpqOHR4YTZ3enlqY253ZmZtMHlqdmt1MnV4dDdrYTN"
    "0bXZ6eXB6M2puMnN5eXM2ZmFwa3J1eHpudWFueGJocHNyc2FiMmR6MmE2N3JubDVnMW9mZndtb"
    "G5ybmV5MWs0eWtzcmVqd2wyN2FqdGp1MWh3OGZ5OGhsZHlkdmdxbnZxa3pvMzF3bml1eGYzem5"
    "3em9mOXQ3bGJxcnAxemVld2s0dzZrbGdvZWczdnEya2ZkaWR6NGtyZjZ2bWtwYzRnZjVzZml1Z"
    "nFraHhhb2dpZmhtdGt1dWZhcWJsaHR6YWc2d2h0MWF6dzBjdnZ6bms4bTh0dDhqZzM5anR6OGV"
    "0aHp3bnFvN3lxczhwdzA3YWlndXR1cmtocm5qMTdoc3E4dnhoZzlqaDhjd214Y3RpNjN2NnVmN"
    "2R1eG1yeGhtOGhuZTZveHZ4ZHZzaDIybjl0NWQ2ZjQzdWtrdDAwYjRmbG1yYmRtaGM0aHVraDJ"
    "sbGJ3OHg1cHBnMGgxa2Z3NndxZjVhZXZrbHBlbGdodmFwM3pxcG5qeHhtcmFveGl5emNtYndzc"
    "2x1czR2NXBxdnVxYmhuYW14em8wb21qcGFzb3RwbHVwcW91dmVuZmxjdGVvNmZqOTdjYW5tejN"
    "kYzJ6ZmtjeHdzaWpqa2V1Mm9vMDNxdWhpcGhrYXN1djdybnlqYmJoMWEwdDNueXdjeWJxaXk0c"
    "XFhaGZnMXlmdGtjcDV4b3dzamlmMWUxaHJiaWtjamwwY3oya2Mwc3JoMXVxYThtcmZjMnpzY2h"
    "2ZW16ejZsbDJuZXh0MDFyOWFreHl2YWo1dWU2eW5rYmZmdGlnY3Jjc2d6MG1ncm1zazl1MnN5N"
    "GZ6c2V2em4zeWRvY2pvNTU1cHV2enl3YXNzbnI2YWE0bmx3ZHkwcXBxdnpicWdreXV3cmptMnR"
    "nenpqb211N3ZmanhtbGJoN29uY3V1MXU2d3R4czM3b2xxd3F3bnJpdnJ2bG53Z3FqdGJqb3R2N"
    "2JqeGRqaWFkcjBndW0zdWp5OGZteHc3aWUzcHdpYmg4dWF1aDVjZW9ja3ZtOGw3YWthYXFhcmV"
    "0bmZycnR6bml3eWVkeWh1NGt4d2tyODJkaWt6b3Y5ZnZrajNwaDh2ZW5wb296dDlscG5pdGlra"
    "zFndW5vbHRrbnBqZHJzMXRudm42anJ2eGoybWY1aGJsd2x3enZ4d2Znc3ZxdGkxNXcyMHIxZWp"
    "nM3BhY3V0NjY2MXFjYnVoeGhueG51aG80ajN2eHNscWZ2Z3htYTNoYjZ4bTJ3bmlidmZiNXFvN"
    "nJ4cXV0Y2hmbGcxZmdzenF0MnlsaTV1dnFxcnFjZXBqaXBvZjF1ajlvNHFpeXJteGhxNnBjemV"
    "hMjRrejV4bzJnaGh5b29sNmZ4cTE2YnVxeGtpa2Vnd2JsbHhyb245NWhoYW85MnlmdXBmN2lsa"
    "mhod2NsemdsZjJxeTNmcGdveXhqaWJubnR4ZjJnZnlteWU5bW9rdnB4NXlkdnFubG5veHlrdGN"
    "oNDB2c3Z5ZnQ3dmJndHA4ZTh0dmg3dTFpamxlcXJzdXR0eWRteHZvejhrd2MyYjdsZmt0dndic"
    "2h1cXZsc2VkZXB4dGd5Y20waGVnZTdpcWV5MG1zZnduemZhejBob2w2ZHBwdjI1dHVlYTlodmx"
    "mb3d2NWl0bGtiajNpcW03eGhzZzl6eG94bWVzdzI2N2xtdHB1Y28xa2hvem10NHl0MGk1cHMyb"
    "zh2em9keHdmeTBrb2VpYjJwbGZ0eGc0d280cnRncHV6Zmh4NGlzejhuMGdwZW5ndC5jb206OTk"
    "5OSIsICJpc1N1YmRvbWFpbiI6IHRydWUsICJmZWF0dXJlIjogIlRoaXNUcmlhbE5hbWVJczEwM"
    "ENoYXJhY3RlcnNMb25nSW5jbHVkaW5nUGFkZGluZ0FBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUF"
    "BQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQSIsICJleHBpcnkiOiAxNDU4NzY2Mjc3fQ==";

}  // namespace

class TrialTokenTest : public testing::Test {
 public:
  TrialTokenTest()
      : expected_origin_(url::Origin::Create(GURL(kExpectedOrigin))),
        expected_subdomain_origin_(
            url::Origin::Create(GURL(kExpectedSubdomainOrigin))),
        expected_multiple_subdomain_origin_(
            url::Origin::Create(GURL(kExpectedMultipleSubdomainOrigin))),
        invalid_origin_(url::Origin::Create(GURL(kInvalidOrigin))),
        insecure_origin_(url::Origin::Create(GURL(kInsecureOrigin))),
        incorrect_port_origin_(url::Origin::Create(GURL(kIncorrectPortOrigin))),
        incorrect_domain_origin_(
            url::Origin::Create(GURL(kIncorrectDomainOrigin))),
        invalid_tld_origin_(url::Origin::Create(GURL(kInvalidTLDOrigin))),
        expected_expiry_(
            base::Time::FromSecondsSinceUnixEpoch(kExpectedExpiry)),
        valid_timestamp_(
            base::Time::FromSecondsSinceUnixEpoch(kValidTimestamp)),
        invalid_timestamp_(
            base::Time::FromSecondsSinceUnixEpoch(kInvalidTimestamp)),
        expected_v2_signature_(
            std::string(reinterpret_cast<const char*>(kSampleTokenV2Signature),
                        std::size(kSampleTokenV2Signature))),
        expected_v3_signature_(
            std::string(reinterpret_cast<const char*>(kSampleTokenV3Signature),
                        std::size(kSampleTokenV3Signature))),
        expected_subdomain_signature_(std::string(
            reinterpret_cast<const char*>(kSampleSubdomainTokenSignature),
            std::size(kSampleSubdomainTokenSignature))),
        expected_nonsubdomain_signature_(std::string(
            reinterpret_cast<const char*>(kSampleNonSubdomainTokenSignature),
            std::size(kSampleNonSubdomainTokenSignature))),
        expected_third_party_signature_(std::string(
            reinterpret_cast<const char*>(kSampleThirdPartyTokenSignature),
            std::size(kSampleThirdPartyTokenSignature))),
        expected_non_third_party_signature_(std::string(
            reinterpret_cast<const char*>(kSampleNonThirdPartyTokenSignature),
            std::size(kSampleNonThirdPartyTokenSignature))),
        expected_third_party_usage_empty_signature_(
            std::string(reinterpret_cast<const char*>(
                            kSampleThirdPartyUsageEmptyTokenSignature),
                        std::size(kSampleThirdPartyUsageEmptyTokenSignature))),
        expected_third_party_usage_subset_signature_(
            std::string(reinterpret_cast<const char*>(
                            kSampleThirdPartyUsageSubsetTokenSignature),
                        std::size(kSampleThirdPartyUsageSubsetTokenSignature))),
        correct_public_key_(kTestPublicKey),
        incorrect_public_key_(kTestPublicKey2) {}

 protected:
  OriginTrialTokenStatus Extract(const std::string& token_text,
                                 const OriginTrialPublicKey& public_key,
                                 std::string* token_payload,
                                 std::string* token_signature,
                                 uint8_t* token_version) {
    return TrialToken::Extract(token_text, public_key, token_payload,
                               token_signature, token_version);
  }

  OriginTrialTokenStatus ExtractStatusOnly(
      const std::string& token_text,
      const OriginTrialPublicKey& public_key) {
    std::string token_payload;
    std::string token_signature;
    uint8_t token_version;
    return Extract(token_text, public_key, &token_payload, &token_signature,
                   &token_version);
  }

  std::unique_ptr<TrialToken> Parse(const std::string& token_payload,
                                    const uint8_t token_version) {
    return TrialToken::Parse(token_payload, token_version);
  }

  bool ValidateOrigin(TrialToken* token, const url::Origin origin) {
    return token->ValidateOrigin(origin);
  }

  bool ValidateFeatureName(TrialToken* token, const char* feature_name) {
    return token->ValidateFeatureName(feature_name);
  }

  bool ValidateDate(TrialToken* token, const base::Time& now) {
    return token->ValidateDate(now);
  }

  const OriginTrialPublicKey& correct_public_key() {
    return correct_public_key_;
  }
  const OriginTrialPublicKey& incorrect_public_key() {
    return incorrect_public_key_;
  }

  const url::Origin expected_origin_;
  const url::Origin expected_subdomain_origin_;
  const url::Origin expected_multiple_subdomain_origin_;
  const url::Origin invalid_origin_;
  const url::Origin insecure_origin_;
  const url::Origin incorrect_port_origin_;
  const url::Origin incorrect_domain_origin_;
  const url::Origin invalid_tld_origin_;

  const base::Time expected_expiry_;
  const base::Time valid_timestamp_;
  const base::Time invalid_timestamp_;

  std::string expected_v2_signature_;
  std::string expected_v3_signature_;
  std::string expected_subdomain_signature_;
  std::string expected_nonsubdomain_signature_;
  std::string expected_third_party_signature_;
  std::string expected_non_third_party_signature_;
  std::string expected_third_party_usage_empty_signature_;
  std::string expected_third_party_usage_subset_signature_;

 private:
  OriginTrialPublicKey correct_public_key_;
  OriginTrialPublicKey incorrect_public_key_;
};

// Test the extraction of the signed payload from token strings. This includes
// checking the included version identifier, payload length, and cryptographic
// signature.

TEST_F(TrialTokenTest, ExtractValidSignatureVersion2) {
  std::string token_payload;
  std::string token_signature;
  uint8_t token_version;
  OriginTrialTokenStatus status =
      Extract(kSampleTokenV2, correct_public_key(), &token_payload,
              &token_signature, &token_version);
  ASSERT_EQ(OriginTrialTokenStatus::kSuccess, status);
  EXPECT_EQ(kVersion2, token_version);
  EXPECT_STREQ(kSampleTokenJSON, token_payload.c_str());
  EXPECT_EQ(expected_v2_signature_, token_signature);
}

TEST_F(TrialTokenTest, ExtractValidSignatureVersion3) {
  std::string token_payload;
  std::string token_signature;
  uint8_t token_version;
  OriginTrialTokenStatus status =
      Extract(kSampleTokenV3, correct_public_key(), &token_payload,
              &token_signature, &token_version);
  ASSERT_EQ(OriginTrialTokenStatus::kSuccess, status);
  EXPECT_EQ(kVersion3, token_version);
  EXPECT_STREQ(kSampleTokenJSON, token_payload.c_str());
  EXPECT_EQ(expected_v3_signature_, token_signature);
}

TEST_F(TrialTokenTest, ExtractSubdomainValidSignature) {
  std::string token_payload;
  std::string token_signature;
  uint8_t token_version;
  OriginTrialTokenStatus status =
      Extract(kSampleSubdomainToken, correct_public_key(), &token_payload,
              &token_signature, &token_version);
  ASSERT_EQ(OriginTrialTokenStatus::kSuccess, status);
  EXPECT_EQ(kVersion2, token_version);
  EXPECT_STREQ(kSampleSubdomainTokenJSON, token_payload.c_str());
  EXPECT_EQ(expected_subdomain_signature_, token_signature);
}

TEST_F(TrialTokenTest, ExtractNonSubdomainValidSignature) {
  std::string token_payload;
  std::string token_signature;
  uint8_t token_version;
  OriginTrialTokenStatus status =
      Extract(kSampleNonSubdomainToken, correct_public_key(), &token_payload,
              &token_signature, &token_version);
  ASSERT_EQ(OriginTrialTokenStatus::kSuccess, status);
  EXPECT_EQ(kVersion2, token_version);
  EXPECT_STREQ(kSampleNonSubdomainTokenJSON, token_payload.c_str());
  EXPECT_EQ(expected_nonsubdomain_signature_, token_signature);
}

TEST_F(TrialTokenTest, ExtractThirdPartyValidSignature) {
  std::string token_payload;
  std::string token_signature;
  uint8_t token_version;
  OriginTrialTokenStatus status =
      Extract(kSampleThirdPartyToken, correct_public_key(), &token_payload,
              &token_signature, &token_version);
  ASSERT_EQ(OriginTrialTokenStatus::kSuccess, status);
  EXPECT_EQ(kVersion3, token_version);
  EXPECT_STREQ(kSampleThirdPartyTokenJSON, token_payload.c_str());
  EXPECT_EQ(expected_third_party_signature_, token_signature);
}

TEST_F(TrialTokenTest, ExtractNonThirdPartyValidSignature) {
  std::string token_payload;
  std::string token_signature;
  uint8_t token_version;
  OriginTrialTokenStatus status =
      Extract(kSampleNonThirdPartyToken, correct_public_key(), &token_payload,
              &token_signature, &token_version);
  ASSERT_EQ(OriginTrialTokenStatus::kSuccess, status);
  EXPECT_EQ(kVersion3, token_version);
  EXPECT_STREQ(kSampleNonThirdPartyTokenJSON, token_payload.c_str());
  EXPECT_EQ(expected_non_third_party_signature_, token_signature);
}

TEST_F(TrialTokenTest, ExtractThirdPartyUsageEmptyValidSignature) {
  std::string token_payload;
  std::string token_signature;
  uint8_t token_version;
  OriginTrialTokenStatus status =
      Extract(kSampleThirdPartyUsageEmptyToken, correct_public_key(),
              &token_payload, &token_signature, &token_version);
  ASSERT_EQ(OriginTrialTokenStatus::kSuccess, status);
  EXPECT_EQ(kVersion3, token_version);
  EXPECT_STREQ(kSampleThirdPartyTokenUsageEmptyJSON, token_payload.c_str());
  EXPECT_EQ(expected_third_party_usage_empty_signature_, token_signature);
}

TEST_F(TrialTokenTest, ExtractThirdPartyUsageSubsetValidSignature) {
  std::string token_payload;
  std::string token_signature;
  uint8_t token_version;
  OriginTrialTokenStatus status =
      Extract(kSampleThirdPartyUsageSubsetToken, correct_public_key(),
              &token_payload, &token_signature, &token_version);
  ASSERT_EQ(OriginTrialTokenStatus::kSuccess, status);
  EXPECT_EQ(kVersion3, token_version);
  EXPECT_STREQ(kSampleThirdPartyTokenUsageSubsetJSON, token_payload.c_str());
  EXPECT_EQ(expected_third_party_usage_subset_signature_, token_signature);
}

TEST_F(TrialTokenTest, ExtractInvalidSignature) {
  OriginTrialTokenStatus status =
      ExtractStatusOnly(kInvalidSignatureToken, correct_public_key());
  EXPECT_EQ(OriginTrialTokenStatus::kInvalidSignature, status);
}

TEST_F(TrialTokenTest, ExtractSignatureWithIncorrectKey) {
  OriginTrialTokenStatus status =
      ExtractStatusOnly(kSampleTokenV2, incorrect_public_key());
  EXPECT_EQ(OriginTrialTokenStatus::kInvalidSignature, status);
}

TEST_F(TrialTokenTest, ExtractEmptyToken) {
  OriginTrialTokenStatus status = ExtractStatusOnly("", correct_public_key());
  EXPECT_EQ(OriginTrialTokenStatus::kMalformed, status);
}

TEST_F(TrialTokenTest, ExtractShortToken) {
  OriginTrialTokenStatus status =
      ExtractStatusOnly(kTruncatedToken, correct_public_key());
  EXPECT_EQ(OriginTrialTokenStatus::kMalformed, status);
}

TEST_F(TrialTokenTest, ExtractUnsupportedVersion) {
  OriginTrialTokenStatus status =
      ExtractStatusOnly(kIncorrectVersionToken, correct_public_key());
  EXPECT_EQ(OriginTrialTokenStatus::kWrongVersion, status);
}

TEST_F(TrialTokenTest, ExtractSignatureWithIncorrectLength) {
  OriginTrialTokenStatus status =
      ExtractStatusOnly(kIncorrectLengthToken, correct_public_key());
  EXPECT_EQ(OriginTrialTokenStatus::kMalformed, status);
}

TEST_F(TrialTokenTest, ExtractLargeToken) {
  std::string token_payload;
  std::string token_signature;
  uint8_t token_version;
  OriginTrialTokenStatus status =
      Extract(kLargeValidToken, correct_public_key(), &token_payload,
              &token_signature, &token_version);
  ASSERT_EQ(OriginTrialTokenStatus::kSuccess, status);
  EXPECT_EQ(kVersion2, token_version);
  std::string expected_signature(
      std::string(reinterpret_cast<const char*>(kLargeValidTokenSignature),
                  std::size(kLargeValidTokenSignature)));
  EXPECT_EQ(expected_signature, token_signature);
}

TEST_F(TrialTokenTest, ExtractTooLargeToken) {
  OriginTrialTokenStatus status =
      ExtractStatusOnly(kTooLargeValidToken, correct_public_key());
  EXPECT_EQ(OriginTrialTokenStatus::kMalformed, status);
}

// Test parsing of fields from JSON token.
class TrialTokenParseInvalidTest
    : public TrialTokenTest,
      public testing::WithParamInterface<std::tuple<const char*, uint8_t>> {};

TEST_P(TrialTokenParseInvalidTest, ParseInvalidString) {
  std::tuple<const char*, uint8_t> param = GetParam();
  std::unique_ptr<TrialToken> empty_token =
      Parse(std::get<0>(param), std::get<1>(param));
  EXPECT_FALSE(empty_token) << "Invalid trial token should not parse.";
}

INSTANTIATE_TEST_SUITE_P(TrialTokenTest,
                         TrialTokenParseInvalidTest,
                         testing::Combine(testing::ValuesIn(kInvalidTokens),
                                          testing::Values(kVersion2,
                                                          kVersion3)));

class TrialTokenParseInvalidVersion3Test
    : public TrialTokenTest,
      public testing::WithParamInterface<const char*> {};

TEST_P(TrialTokenParseInvalidVersion3Test, ParseInvalidString) {
  std::unique_ptr<TrialToken> empty_token = Parse(GetParam(), kVersion3);
  EXPECT_FALSE(empty_token) << "Invalid trial token should not parse.";
}

INSTANTIATE_TEST_SUITE_P(TrialTokenTest,
                         TrialTokenParseInvalidVersion3Test,
                         testing::ValuesIn(kInvalidTokensVersion3));

// Test parsing of fields from JSON token.
class TrialTokenParseTest : public TrialTokenTest,
                            public testing::WithParamInterface<uint8_t> {};

TEST_P(TrialTokenParseTest, ParseValidToken) {
  std::unique_ptr<TrialToken> token = Parse(kSampleTokenJSON, GetParam());
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_FALSE(token->match_subdomains());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
  EXPECT_EQ(TrialToken::UsageRestriction::kNone, token->usage_restriction());
}

TEST_P(TrialTokenParseTest, ParseValidNonSubdomainToken) {
  std::unique_ptr<TrialToken> token =
      Parse(kSampleNonSubdomainTokenJSON, GetParam());
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_FALSE(token->match_subdomains());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_P(TrialTokenParseTest, ParseValidSubdomainToken) {
  std::unique_ptr<TrialToken> token =
      Parse(kSampleSubdomainTokenJSON, GetParam());
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_TRUE(token->match_subdomains());
  EXPECT_EQ(kExpectedSubdomainOrigin, token->origin().Serialize());
  EXPECT_EQ(expected_subdomain_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_P(TrialTokenParseTest, ParseValidLargeToken) {
  std::unique_ptr<TrialToken> token = Parse(kLargeTokenJSON, GetParam());
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedLongFeatureName, token->feature_name());
  EXPECT_TRUE(token->match_subdomains());
  url::Origin expected_long_origin(
      url::Origin::Create(GURL(kExpectedLongTokenOrigin)));
  EXPECT_EQ(expected_long_origin, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_P(TrialTokenParseTest, ParseTooLargeToken) {
  std::unique_ptr<TrialToken> token = Parse(kTooLargeTokenJSON, GetParam());
  ASSERT_FALSE(token);
}

TEST_P(TrialTokenParseTest, ValidateValidToken) {
  std::unique_ptr<TrialToken> token = Parse(kSampleTokenJSON, GetParam());
  ASSERT_TRUE(token);
  EXPECT_TRUE(ValidateOrigin(token.get(), expected_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), invalid_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), insecure_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), incorrect_port_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), incorrect_domain_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), invalid_tld_origin_));
  EXPECT_TRUE(ValidateFeatureName(token.get(), kExpectedFeatureName));
  EXPECT_FALSE(ValidateFeatureName(token.get(), kInvalidFeatureName));
  EXPECT_FALSE(ValidateFeatureName(
      token.get(), base::ToUpperASCII(kExpectedFeatureName).c_str()));
  EXPECT_FALSE(ValidateFeatureName(
      token.get(), base::ToLowerASCII(kExpectedFeatureName).c_str()));
  EXPECT_TRUE(ValidateDate(token.get(), valid_timestamp_));
  EXPECT_FALSE(ValidateDate(token.get(), invalid_timestamp_));
}

TEST_P(TrialTokenParseTest, ValidateValidSubdomainToken) {
  std::unique_ptr<TrialToken> token =
      Parse(kSampleSubdomainTokenJSON, GetParam());
  ASSERT_TRUE(token);
  EXPECT_TRUE(ValidateOrigin(token.get(), expected_origin_));
  EXPECT_TRUE(ValidateOrigin(token.get(), expected_subdomain_origin_));
  EXPECT_TRUE(ValidateOrigin(token.get(), expected_multiple_subdomain_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), insecure_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), incorrect_port_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), incorrect_domain_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), invalid_tld_origin_));
}

TEST_P(TrialTokenParseTest, TokenIsValid) {
  std::unique_ptr<TrialToken> token = Parse(kSampleTokenJSON, GetParam());
  ASSERT_TRUE(token);
  EXPECT_EQ(OriginTrialTokenStatus::kSuccess,
            token->IsValid(expected_origin_, valid_timestamp_));
  EXPECT_EQ(OriginTrialTokenStatus::kWrongOrigin,
            token->IsValid(invalid_origin_, valid_timestamp_));
  EXPECT_EQ(OriginTrialTokenStatus::kWrongOrigin,
            token->IsValid(insecure_origin_, valid_timestamp_));
  EXPECT_EQ(OriginTrialTokenStatus::kWrongOrigin,
            token->IsValid(incorrect_port_origin_, valid_timestamp_));
  EXPECT_EQ(OriginTrialTokenStatus::kExpired,
            token->IsValid(expected_origin_, invalid_timestamp_));
}

TEST_P(TrialTokenParseTest, SubdomainTokenIsValid) {
  std::unique_ptr<TrialToken> token =
      Parse(kSampleSubdomainTokenJSON, GetParam());
  ASSERT_TRUE(token);
  EXPECT_EQ(OriginTrialTokenStatus::kSuccess,
            token->IsValid(expected_origin_, valid_timestamp_));
  EXPECT_EQ(OriginTrialTokenStatus::kSuccess,
            token->IsValid(expected_subdomain_origin_, valid_timestamp_));
  EXPECT_EQ(
      OriginTrialTokenStatus::kSuccess,
      token->IsValid(expected_multiple_subdomain_origin_, valid_timestamp_));
  EXPECT_EQ(OriginTrialTokenStatus::kWrongOrigin,
            token->IsValid(incorrect_domain_origin_, valid_timestamp_));
  EXPECT_EQ(OriginTrialTokenStatus::kWrongOrigin,
            token->IsValid(insecure_origin_, valid_timestamp_));
  EXPECT_EQ(OriginTrialTokenStatus::kWrongOrigin,
            token->IsValid(incorrect_port_origin_, valid_timestamp_));
  EXPECT_EQ(OriginTrialTokenStatus::kExpired,
            token->IsValid(expected_origin_, invalid_timestamp_));
}

INSTANTIATE_TEST_SUITE_P(TrialTokenTest,
                         TrialTokenParseTest,
                         testing::Values(kVersion2, kVersion3));

TEST_F(TrialTokenTest, ParseValidNonThirdPartyToken) {
  std::unique_ptr<TrialToken> token =
      Parse(kSampleNonThirdPartyTokenJSON, kVersion3);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_FALSE(token->is_third_party());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_F(TrialTokenTest, ParseValidThirdPartyToken) {
  std::unique_ptr<TrialToken> token =
      Parse(kSampleThirdPartyTokenJSON, kVersion3);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_TRUE(token->is_third_party());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_F(TrialTokenTest, ParseValidThirdPartyTokenInvalidVersion) {
  std::unique_ptr<TrialToken> token =
      Parse(kSampleThirdPartyTokenJSON, kVersion2);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_FALSE(token->is_third_party());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_F(TrialTokenTest, ParseValidUsageEmptyToken) {
  std::unique_ptr<TrialToken> token = Parse(kUsageEmptyTokenJSON, kVersion3);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_FALSE(token->is_third_party());
  EXPECT_EQ(TrialToken::UsageRestriction::kNone, token->usage_restriction());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_F(TrialTokenTest, ParseValidUsageSubsetToken) {
  std::unique_ptr<TrialToken> token = Parse(kUsageSubsetTokenJSON, kVersion3);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_FALSE(token->is_third_party());
  EXPECT_EQ(TrialToken::UsageRestriction::kSubset, token->usage_restriction());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_F(TrialTokenTest, ParseValidThirdPartyUsageSubsetToken) {
  std::unique_ptr<TrialToken> token =
      Parse(kSampleThirdPartyTokenUsageSubsetJSON, kVersion3);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_TRUE(token->is_third_party());
  EXPECT_EQ(TrialToken::UsageRestriction::kSubset, token->usage_restriction());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_F(TrialTokenTest, ParseValidThirdPartyUsageEmptyToken) {
  std::unique_ptr<TrialToken> token =
      Parse(kSampleThirdPartyTokenUsageEmptyJSON, kVersion3);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_TRUE(token->is_third_party());
  EXPECT_EQ(TrialToken::UsageRestriction::kNone, token->usage_restriction());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

// Test overall extraction and parsing, to ensure output status matches returned
// token, and signature is provided.
// Test Version 2.
TEST_F(TrialTokenTest, FromValidToken) {
  OriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> token =
      TrialToken::From(kSampleTokenV2, correct_public_key(), &status);
  EXPECT_TRUE(token);
  EXPECT_EQ(OriginTrialTokenStatus::kSuccess, status);
  EXPECT_EQ(expected_v2_signature_, token->signature());
}

TEST_F(TrialTokenTest, FromInvalidSignature) {
  OriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> token =
      TrialToken::From(kSampleTokenV2, incorrect_public_key(), &status);
  EXPECT_FALSE(token);
  EXPECT_EQ(OriginTrialTokenStatus::kInvalidSignature, status);
}

// Test Version 3.
TEST_F(TrialTokenTest, FromValidTokenVersion3) {
  OriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> token =
      TrialToken::From(kSampleTokenV3, correct_public_key(), &status);
  EXPECT_TRUE(token);
  EXPECT_EQ(OriginTrialTokenStatus::kSuccess, status);
  EXPECT_EQ(expected_v3_signature_, token->signature());
}

TEST_F(TrialTokenTest, FromInvalidSignatureVersion3) {
  OriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> token =
      TrialToken::From(kSampleTokenV3, incorrect_public_key(), &status);
  EXPECT_FALSE(token);
  EXPECT_EQ(OriginTrialTokenStatus::kInvalidSignature, status);
}

TEST_F(TrialTokenTest, FromMalformedToken) {
  OriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> token =
      TrialToken::From(kIncorrectLengthToken, correct_public_key(), &status);
  EXPECT_FALSE(token);
  EXPECT_EQ(OriginTrialTokenStatus::kMalformed, status);
}

}  // namespace blink

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/enclave/verify/rekor.h"

#include <cstdint>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "device/fido/enclave/verify/test_utils.h"
#include "device/fido/enclave/verify/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device::enclave {

constexpr char kBody[] =
    "eyJhcGlWZXJzaW9uIjoiMC4wLjEiLCJraW5kIjoicmVrb3JkIiwic3BlYyI6eyJkYXRhIjp7"
    "Imhhc2giOnsiYWxnb3JpdGhtIjoic2hhMjU2IiwidmFsdWUiOiI5NjFmNjBhY2I5ZTU3MjNi"
    "N2IzMDE4NjA0ODE1MjliYzNmMTY2MWU1MDg2YzI5Y2Q1NjI0MmUzMjFiYTdmOTU5In19LCJz"
    "aWduYXR1cmUiOnsiY29udGVudCI6Ik1FWUNJUUN5RUhmUVp4VnphOG9TZzZHclBwOW5aM0VU"
    "cytOV0g2blhUN3VIUDJ6L0hnSWhBSkpHTjU2K3BPU1BGNGdtL0o1QXMyMzA2d09RRDJLeEla"
    "bWk5R2p5aDZSOSIsImZvcm1hdCI6Ing1MDkiLCJwdWJsaWNLZXkiOnsiY29udGVudCI6IkxT"
    "MHRMUzFDUlVkSlRpQlFWVUpNU1VNZ1MwVlpMUzB0TFMwS1RVWnJkMFYzV1VoTGIxcEplbW93"
    "UTBGUldVbExiMXBKZW1vd1JFRlJZMFJSWjBGRlluZFNZMUZaTWxsMlZXaHdPRkZ3YWtKRWFr"
    "UlpkR2R5YWtGWFNncGhMMlYzVlV0MU4xVlNTMVpOWW5wcFRqaEpaSHAxTjI1bFMyTjJaakpS"
    "UzFCcldWaFNaWEJzZVRabVQzVm1aRmhhU2l0VFVGWnhXRUpuUFQwS0xTMHRMUzFGVGtRZ1VG"
    "VkNURWxESUV0RldTMHRMUzB0Q2c9PSJ9fX19";

constexpr char kLogID[] =
    "c0d23d6ad406973f9559f3ba2d1ca01f84147d8ffc5b8445c224f98b9591801d";

constexpr char kSignedEntryTimestamp[] =
    "MEYCIQCd0RrIJrMSCBhTAwl+HOMU/9w81hs7xCXZRElft/"
    "jcCAIhAN07e5BrXL4xn8ZeZTAnfsCwyjO9e3NaTNt4zAFj96mV";

constexpr char kApiVersion[] = "0.0.1";

constexpr char kKind[] = "rekord";

constexpr char kGenericSignatureContent[] =
    "MEYCIQCyEHfQZxVza8oSg6GrPp9nZ3ETs+NWH6nXT7uHP2z/HgIhAJJGN56+pOSPF4gm/"
    "J5As2306wOQD2KxIZmi9Gjyh6R9";

constexpr char kGenericSignatureFormat[] = "x509";

constexpr char kGenericSignaturePublicKeyContent[] =
    "LS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS0KTUZrd0V3WUhLb1pJemowQ0FRWUlLb1pJemowRE"
    "FRY0RRZ0FFYndSY1FZMll2VWhwOFFwakJEakRZdGdyakFXSgphL2V3VUt1N1VSS1ZNYnppTjhJ"
    "ZHp1N25lS2N2ZjJRS1BrWVhSZXBseTZmT3VmZFhaSitTUFZxWEJnPT0KLS0tLS1FTkQgUFVCTE"
    "lDIEtFWS0tLS0tCg==";

constexpr char kHashValue[] =
    "961f60acb9e5723b7b301860481529bc3f1661e5086c29cd56242e321ba7f959";

constexpr uint64_t kLogIndex = 74497915;

constexpr base::Time kIntegratedTime = base::Time::FromTimeT(1709113639);

TEST(RekorTest, GetRekorLogEntry) {
  std::string json = GetContentsFromFile("logentry.json");
  base::span<const uint8_t> span = base::make_span(
      static_cast<uint8_t*>((uint8_t*)json.data()), json.size());
  std::optional<LogEntry> log_entry = GetRekorLogEntry(span);
  EXPECT_TRUE(log_entry.has_value());
  EXPECT_EQ(log_entry->body, kBody);
  EXPECT_EQ(log_entry->integrated_time, kIntegratedTime);
  EXPECT_EQ(log_entry->log_id, kLogID);
  EXPECT_EQ(log_entry->log_index, kLogIndex);
  EXPECT_TRUE(log_entry->verification.has_value());
  EXPECT_EQ(log_entry->verification->signed_entry_timestamp,
            kSignedEntryTimestamp);
}

TEST(RekorTest, GetRekorLogEntryNoVerification) {
  std::string json = GetContentsFromFile("logentry_noverification.json");
  base::span<const uint8_t> span = base::make_span(
      static_cast<uint8_t*>((uint8_t*)json.data()), json.size());
  std::optional<LogEntry> log_entry = GetRekorLogEntry(span);
  EXPECT_TRUE(log_entry.has_value());
  EXPECT_EQ(log_entry->body, kBody);
  EXPECT_EQ(log_entry->integrated_time, kIntegratedTime);
  EXPECT_EQ(log_entry->log_id, kLogID);
  EXPECT_EQ(log_entry->log_index, kLogIndex);
  EXPECT_FALSE(log_entry->verification.has_value());
}

TEST(RekorTest, GetRekorLogEntryBody) {
  std::string json = GetContentsFromFile("logentry.json");
  base::span<const uint8_t> span = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  std::optional<Body> log_entry_body = GetRekorLogEntryBody(span);
  EXPECT_TRUE(log_entry_body.has_value());
  EXPECT_EQ(log_entry_body->api_version, kApiVersion);
  EXPECT_EQ(log_entry_body->kind, kKind);
  EXPECT_EQ(log_entry_body->spec.generic_signature.content,
            kGenericSignatureContent);
  EXPECT_EQ(log_entry_body->spec.generic_signature.format,
            kGenericSignatureFormat);
  EXPECT_EQ(log_entry_body->spec.generic_signature.public_key.content,
            kGenericSignaturePublicKeyContent);
  const std::vector<uint8_t>& bytes = log_entry_body->spec.data.hash.bytes;
  std::string hash_value = std::string(bytes.begin(), bytes.end());
  EXPECT_EQ(hash_value, kHashValue);
  EXPECT_EQ(log_entry_body->spec.data.hash.hash_type, HashType::kSHA256);
}

TEST(RekorTest, RekorSignatureBundleFromLogEntryWithVerification) {
  std::string json = GetContentsFromFile("logentry.json");
  base::span<const uint8_t> span = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  std::optional<LogEntry> log_entry = GetRekorLogEntry(span);
  std::optional<RekorSignatureBundle> rekor_signature_bundle =
      GetRekorSignatureBundle(log_entry.value());
  // TODO(livseibert): Once VerifyRekorSignature is implemented, check that this
  // result is valid.
  EXPECT_TRUE(rekor_signature_bundle.has_value());
}

TEST(RekorTest, RekorSignatureBundleFromLogEntryWithoutVerification) {
  std::string json = GetContentsFromFile("logentry_noverification.json");
  base::span<const uint8_t> span = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  std::optional<LogEntry> log_entry = GetRekorLogEntry(span);
  std::optional<RekorSignatureBundle> rekor_signature_bundle =
      GetRekorSignatureBundle(log_entry.value());
  EXPECT_FALSE(rekor_signature_bundle.has_value());
}

TEST(RekorTest, RekorSignatureBundleFromLogEntryWithBackslash) {
  std::string json = GetContentsFromFile("logentry_backslash.json");
  base::span<const uint8_t> span = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  std::optional<LogEntry> log_entry = GetRekorLogEntry(span);
  std::optional<RekorSignatureBundle> rekor_signature_bundle =
      GetRekorSignatureBundle(log_entry.value());
  EXPECT_FALSE(rekor_signature_bundle.has_value());
}

TEST(RekorTest, VerifyRekorSignatureSuccess) {
  auto pub_key = ConvertPemToRaw(GetContentsFromFile("rekor_pub_key.pem"));
  ASSERT_TRUE(pub_key.has_value());
  std::string json = GetContentsFromFile("logentry.json");
  base::span<const uint8_t> log_entry = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  EXPECT_TRUE(VerifyRekorSignature(log_entry, *pub_key));
}

TEST(RekorTest, VerifyRekorSignatureFailure) {
  auto pub_key = ConvertPemToRaw(GetContentsFromFile("rekor_pub_key.pem"));
  ASSERT_TRUE(pub_key.has_value());
  std::string json = GetContentsFromFile("logentry_backslash.json");
  base::span<const uint8_t> log_entry = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  EXPECT_FALSE(VerifyRekorSignature(log_entry, *pub_key));
}

TEST(RekorTest, VerifyRekorBodySucceeds) {
  std::string json = GetContentsFromFile("logentry.json");
  std::string endorsement = GetContentsFromFile("endorsement.json");
  base::span<const uint8_t> span = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  std::optional<Body> log_entry_body = GetRekorLogEntryBody(span);
  EXPECT_TRUE(log_entry_body.has_value());
  base::span<const uint8_t> content_span = base::make_span(
      reinterpret_cast<const uint8_t*>(endorsement.data()), endorsement.size());
  EXPECT_TRUE(VerifyRekorBody(log_entry_body.value(), content_span));
}

TEST(RekorTest, VerifyRekorBodyWrongSignatureFormatFails) {
  std::string json = GetContentsFromFile("logentry.json");
  std::string endorsement = GetContentsFromFile("endorsement.json");
  base::span<const uint8_t> span = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  std::optional<Body> log_entry_body = GetRekorLogEntryBody(span);
  EXPECT_TRUE(log_entry_body.has_value());
  log_entry_body->spec.generic_signature.format = "bad";
  base::span<const uint8_t> content_span = base::make_span(
      reinterpret_cast<const uint8_t*>(endorsement.data()), endorsement.size());
  EXPECT_FALSE(VerifyRekorBody(log_entry_body.value(), content_span));
}

TEST(RekorTest, VerifyRekorBodyWrongContentFails) {
  std::string json = GetContentsFromFile("logentry.json");
  std::string endorsement = "abcd";
  base::span<const uint8_t> span = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  std::optional<Body> log_entry_body = GetRekorLogEntryBody(span);
  EXPECT_TRUE(log_entry_body.has_value());
  base::span<const uint8_t> content_span = base::make_span(
      reinterpret_cast<const uint8_t*>(endorsement.data()), endorsement.size());
  EXPECT_FALSE(VerifyRekorBody(log_entry_body.value(), content_span));
}

TEST(RekorTest, VerifyRekorBodyWrongPublicKeyFails) {
  std::string json = GetContentsFromFile("logentry.json");
  std::string endorsement = GetContentsFromFile("endorsement.json");
  base::span<const uint8_t> span = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  std::optional<Body> log_entry_body = GetRekorLogEntryBody(span);
  EXPECT_TRUE(log_entry_body.has_value());
  log_entry_body->spec.generic_signature.public_key.content = "abcd";
  base::span<const uint8_t> content_span = base::make_span(
      reinterpret_cast<const uint8_t*>(endorsement.data()), endorsement.size());
  EXPECT_FALSE(VerifyRekorBody(log_entry_body.value(), content_span));
}

TEST(RekorTest, VerifyRekorLogEntrySuccess) {
  auto pub_key = ConvertPemToRaw(GetContentsFromFile("rekor_pub_key.pem"));
  ASSERT_TRUE(pub_key.has_value());
  std::string json = GetContentsFromFile("logentry.json");
  std::string endorsement = GetContentsFromFile("endorsement.json");
  base::span<const uint8_t> log_entry = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  base::span<const uint8_t> endorsement_span = base::make_span(
      reinterpret_cast<const uint8_t*>(endorsement.data()), endorsement.size());
  EXPECT_TRUE(VerifyRekorLogEntry(log_entry, *pub_key, endorsement_span));
}

TEST(RekorTest, VerifyRekorLogEntryBadLogEntryFails) {
  auto pub_key = ConvertPemToRaw(GetContentsFromFile("rekor_pub_key.pem"));
  ASSERT_TRUE(pub_key.has_value());
  std::string json = GetContentsFromFile("logentry_backslash.json");
  std::string endorsement = GetContentsFromFile("endorsement.json");
  base::span<const uint8_t> log_entry = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  base::span<const uint8_t> endorsement_span = base::make_span(
      reinterpret_cast<const uint8_t*>(endorsement.data()), endorsement.size());
  EXPECT_FALSE(VerifyRekorLogEntry(log_entry, *pub_key, endorsement_span));
}

TEST(RekorTest, VerifyRekorLogEntryBadEndorsementFails) {
  auto pub_key = ConvertPemToRaw(GetContentsFromFile("rekor_pub_key.pem"));
  ASSERT_TRUE(pub_key.has_value());
  std::string json = GetContentsFromFile("logentry.json");
  std::string endorsement = "abcd";
  base::span<const uint8_t> log_entry = base::make_span(
      reinterpret_cast<const uint8_t*>(json.data()), json.size());
  base::span<const uint8_t> endorsement_span = base::make_span(
      reinterpret_cast<const uint8_t*>(endorsement.data()), endorsement.size());
  EXPECT_FALSE(VerifyRekorLogEntry(log_entry, *pub_key, endorsement_span));
}

}  // namespace device::enclave

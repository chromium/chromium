// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_MOCK_UNEXPORTABLE_KEY_H_
#define CRYPTO_MOCK_UNEXPORTABLE_KEY_H_

#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace crypto {

class MockUnexportableSigningKey : public UnexportableSigningKey,
                                   public StatefulKey {
 public:
  MockUnexportableSigningKey();
  ~MockUnexportableSigningKey() override;

  // UnexportableKey:
  MOCK_METHOD(SignatureVerifier::SignatureAlgorithm,
              Algorithm,
              (),
              (const, override));
  MOCK_METHOD(std::vector<uint8_t>,
              GetSubjectPublicKeyInfo,
              (),
              (const, override));
  MOCK_METHOD(std::vector<uint8_t>, GetWrappedKey, (), (const, override));
  MOCK_METHOD(bool, IsHardwareBacked, (), (const, override));
#if BUILDFLAG(IS_APPLE)
  MOCK_METHOD(SecKeyRef, GetSecKeyRef, (), (const, override));
#endif  // BUILDFLAG(IS_APPLE)
  MOCK_METHOD(const StatefulKey*, AsStatefulKey, (), (const, override));

  // StatefulKey:
  MOCK_METHOD(std::string, GetKeyTag, (), (const, override));
  MOCK_METHOD(base::Time, GetCreationTime, (), (const, override));

  // UnexportableSigningKey:
  MOCK_METHOD(std::optional<std::vector<uint8_t>>,
              SignSlowly,
              (base::span<const uint8_t> data),
              (override));
#if BUILDFLAG(IS_WIN)
  MOCK_METHOD(bool, SupportsTls13, (), (override));
#endif  // BUILDFLAG(IS_WIN)
};

class MockUnexportableAttestationKey : public UnexportableAttestationKey,
                                       public StatefulKey {
 public:
  MockUnexportableAttestationKey();
  ~MockUnexportableAttestationKey() override;

  // UnexportableKey:
  MOCK_METHOD(SignatureVerifier::SignatureAlgorithm,
              Algorithm,
              (),
              (const, override));
  MOCK_METHOD(std::vector<uint8_t>,
              GetSubjectPublicKeyInfo,
              (),
              (const, override));
  MOCK_METHOD(std::vector<uint8_t>, GetWrappedKey, (), (const, override));
  MOCK_METHOD(bool, IsHardwareBacked, (), (const, override));
#if BUILDFLAG(IS_APPLE)
  MOCK_METHOD(SecKeyRef, GetSecKeyRef, (), (const, override));
#endif  // BUILDFLAG(IS_APPLE)
  MOCK_METHOD(const StatefulKey*, AsStatefulKey, (), (const, override));

  // StatefulKey:
  MOCK_METHOD(std::string, GetKeyTag, (), (const, override));
  MOCK_METHOD(base::Time, GetCreationTime, (), (const, override));

  // UnexportableAttestationKey:
  MOCK_METHOD(std::optional<AttestationStatement>,
              CertifySlowly,
              (const UnexportableSigningKey& signing_key,
               base::span<const uint8_t> challenge),
              (override));
};

}  // namespace crypto

#endif  // CRYPTO_MOCK_UNEXPORTABLE_KEY_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BIO_ENROLLMENT_H_
#define DEVICE_FIDO_BIO_ENROLLMENT_H_

#include "base/component_export.h"
#include "base/optional.h"

#include "components/cbor/values.h"

#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"

namespace device {

// This file defines structures and values required to interact with
// an authenticator that supports authenticatorBioEnrollment (0x09,
// or vendor-specific 0x40). This command currently exists in the
// pre-standardization CTAP2.1 specification, section 5.7.
// TODO(martinkr) add link to standard when published

enum class BioEnrollmentRequestKey : uint8_t {
  kModality = 0x01,
  kSubCommand = 0x02,
  kSubCommandParams = 0x03,
  kPinProtocol = 0x04,
  kPinAuth = 0x05,
  kGetModality = 0x06
};

enum class BioEnrollmentModality : uint8_t {
  kFingerprint = 0x01,
  kMin = kFingerprint,
  kMax = kFingerprint
};

enum class BioEnrollmentFingerprintKind : uint8_t {
  kTouch = 0x01,
  kSwipe = 0x02,
  kMin = kTouch,
  kMax = kSwipe
};

enum class BioEnrollmentSubCommand : uint8_t {
  kEnrollBegin = 0x01,
  kEnrollCaptureNextSample = 0x02,
  kCancelCurrentEnrollment = 0x03,
  kEnumerateEnrollments = 0x04,
  kSetFriendlyName = 0x05,
  kRemoveEnrollment = 0x06,
  kGetFingerprintSensorInfo = 0x07,
  kMin = kEnrollBegin,
  kMax = kGetFingerprintSensorInfo
};

enum class BioEnrollmentSubCommandParam : uint8_t {
  kTemplateId = 0x01,
  kTemplateFriendlyName = 0x02,
  kTimeoutMilliseconds = 0x03
};

enum class BioEnrollmentResponseKey : uint8_t {
  kModality = 0x01,
  kFingerprintKind = 0x02,
  kMaxCaptureSamplesRequiredForEnroll = 0x03,
  kTemplateId = 0x04,
  kLastEnrollSampleStatus = 0x05,
  kRemainingSamples = 0x06,
  kTemplateInfos = 0x07
};

enum class BioEnrollmentTemplateInfoParam : uint8_t {
  kTemplateId = 0x01,
  kTemplateFriendlyName = 0x02
};

enum class BioEnrollmentSampleStatus : uint8_t {
  kGood = 0x00,
  kTooHigh = 0x01,
  kTooLow = 0x02,
  kTooLeft = 0x03,
  kTooRight = 0x04,
  kTooFast = 0x05,
  kTooSlow = 0x06,
  kPoorQuality = 0x07,
  kTooSkewed = 0x08,
  kTooShort = 0x09,
  kMergeFailure = 0x0A,
  kExists = 0x0B,
  kDatabaseFull = 0x0C,
  kNoUserActivity = 0x0D,
  kNoUserPresenceTransition = 0x0E,
  kMin = kGood,
  kMax = kNoUserPresenceTransition
};

template <typename T>
static base::Optional<T> ToBioEnrollmentEnum(uint8_t v) {
  // Check if enum-class is in range...
  if (v < static_cast<int>(T::kMin) || v > static_cast<int>(T::kMax)) {
    // ...to avoid possible undefined behavior (casting from int to enum).
    return base::nullopt;
  }
  return static_cast<T>(v);
}

struct BioEnrollmentRequest {
  enum Version {
    kDefault,
    kPreview,
  };

  static BioEnrollmentRequest ForGetModality(Version);
  static BioEnrollmentRequest ForGetSensorInfo(Version);
  static BioEnrollmentRequest ForEnrollBegin(
      Version,
      const pin::TokenResponse& pin_token);
  static BioEnrollmentRequest ForEnrollNextSample(
      Version,
      const pin::TokenResponse& pin_token,
      std::vector<uint8_t> template_id);
  static BioEnrollmentRequest ForCancel(Version);
  static BioEnrollmentRequest ForEnumerate(Version,
                                           const pin::TokenResponse& token);
  static BioEnrollmentRequest ForRename(Version,
                                        const pin::TokenResponse& token,
                                        std::vector<uint8_t> id,
                                        std::string name);
  static BioEnrollmentRequest ForDelete(Version,
                                        const pin::TokenResponse& token,
                                        std::vector<uint8_t> id);

  Version version;
  base::Optional<BioEnrollmentModality> modality;
  base::Optional<BioEnrollmentSubCommand> subcommand;
  base::Optional<cbor::Value::MapValue> params;
  base::Optional<uint8_t> pin_protocol;
  base::Optional<std::vector<uint8_t>> pin_auth;
  base::Optional<bool> get_modality;

  BioEnrollmentRequest(BioEnrollmentRequest&&);
  BioEnrollmentRequest& operator=(BioEnrollmentRequest&&);
  ~BioEnrollmentRequest();

 private:
  BioEnrollmentRequest(Version);
};

struct COMPONENT_EXPORT(DEVICE_FIDO) BioEnrollmentResponse {
  static base::Optional<BioEnrollmentResponse> Parse(
      const base::Optional<cbor::Value>& cbor_response);

  BioEnrollmentResponse();
  BioEnrollmentResponse(BioEnrollmentResponse&&);
  BioEnrollmentResponse& operator=(BioEnrollmentResponse&&) = default;
  ~BioEnrollmentResponse();

  bool operator==(const BioEnrollmentResponse&) const;

  base::Optional<BioEnrollmentModality> modality;
  base::Optional<BioEnrollmentFingerprintKind> fingerprint_kind;
  base::Optional<uint8_t> max_samples_for_enroll;
  base::Optional<std::vector<uint8_t>> template_id;
  base::Optional<BioEnrollmentSampleStatus> last_status;
  base::Optional<uint8_t> remaining_samples;
  base::Optional<std::map<std::vector<uint8_t>, std::string>> template_infos;
};

COMPONENT_EXPORT(DEVICE_FIDO)
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const BioEnrollmentRequest& request);

}  // namespace device

#endif  // DEVICE_FIDO_BIO_ENROLLMENT_H_

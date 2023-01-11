// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "device/fido/fido_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "device/fido/bio/enrollment_handler.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_fido_device_factory.h"

namespace device {
namespace {

constexpr char kPIN[] = "1477";

class BioEnrollmentHandlerTest : public ::testing::Test {
  void SetUp() override {
    virtual_device_factory_.SetSupportedProtocol(ProtocolVersion::kCtap2);
    virtual_device_factory_.mutable_state()->pin = kPIN;
    virtual_device_factory_.mutable_state()->pin_retries =
        device::kMaxPinRetries;
  }

 public:
  void OnEnroll(BioEnrollmentSampleStatus status, uint8_t remaining_samples) {
    if (status != BioEnrollmentSampleStatus::kGood) {
      sample_failures_++;
      return;
    }
    if (!sampling_) {
      sampling_ = true;
      remaining_samples_ = remaining_samples;
      return;
    }
    EXPECT_EQ(remaining_samples, --remaining_samples_);
  }

 protected:
  std::unique_ptr<BioEnrollmentHandler> MakeHandler() {
    return std::make_unique<BioEnrollmentHandler>(
        base::flat_set<FidoTransportProtocol>{
            FidoTransportProtocol::kUsbHumanInterfaceDevice},
        ready_callback_.callback(), error_callback_.callback(),
        base::BindRepeating(&BioEnrollmentHandlerTest::GetPIN,
                            base::Unretained(this)),
        &virtual_device_factory_);
  }

  std::pair<CtapDeviceResponseCode, BioEnrollmentHandler::TemplateId>
  EnrollTemplate(BioEnrollmentHandler* handler) {
    test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                         BioEnrollmentHandler::TemplateId>
        cb;
    handler->EnrollTemplate(MakeSampleCallback(), cb.callback());

    cb.WaitForCallback();
    return {cb.status(), cb.value()};
  }

  void GetPIN(uint32_t min_pin_length,
              int64_t attempts,
              base::OnceCallback<void(std::string)> provide_pin) {
    std::move(provide_pin).Run(kPIN);
  }

  BioEnrollmentHandler::SampleCallback MakeSampleCallback() {
    remaining_samples_ = 0;
    sample_failures_ = 0;
    sampling_ = false;
    return base::BindRepeating(&BioEnrollmentHandlerTest::OnEnroll,
                               base::Unretained(this));
  }

  uint8_t remaining_samples_;
  size_t sample_failures_;
  bool sampling_;
  base::test::TaskEnvironment task_environment_;
  test::ValueCallbackReceiver<BioEnrollmentHandler::SensorInfo> ready_callback_;
  test::ValueCallbackReceiver<BioEnrollmentHandler::Error> error_callback_;
  test::VirtualFidoDeviceFactory virtual_device_factory_;
};

// Tests bio enrollment handler against device without PIN support.
TEST_F(BioEnrollmentHandlerTest, NoPINSupport) {
  VirtualCtap2Device::Config config;
  config.pin_support = false;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  error_callback_.WaitForCallback();

  EXPECT_EQ(error_callback_.value(), BioEnrollmentHandler::Error::kNoPINSet);
}

// Tests bio enrollment handler against device with the forcePINChange flag on.
TEST_F(BioEnrollmentHandlerTest, ForcePINChange) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;
  config.min_pin_length_support = true;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {Ctap2Version::kCtap2_1};
  virtual_device_factory_.mutable_state()->force_pin_change = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  error_callback_.WaitForCallback();

  EXPECT_EQ(error_callback_.value(),
            BioEnrollmentHandler::Error::kForcePINChange);
}

// Tests enrollment handler PIN soft block.
TEST_F(BioEnrollmentHandlerTest, SoftPINBlock) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.mutable_state()->pin = "1234";
  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  error_callback_.WaitForCallback();

  EXPECT_EQ(error_callback_.value(),
            BioEnrollmentHandler::Error::kSoftPINBlock);
}

// Tests bio enrollment commands against an authenticator lacking support.
TEST_F(BioEnrollmentHandlerTest, NoBioEnrollmentSupport) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  error_callback_.WaitForCallback();
  EXPECT_EQ(error_callback_.value(),
            BioEnrollmentHandler::Error::kAuthenticatorMissingBioEnrollment);
}

// Tests fingerprint enrollment lifecycle.
TEST_F(BioEnrollmentHandlerTest, Enroll) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  auto [status, template_id] = EnrollTemplate(handler.get());
  EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
  EXPECT_FALSE(template_id.empty());
}

// Tests enrolling multiple fingerprints.
TEST_F(BioEnrollmentHandlerTest, EnrollMultiple) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Multiple enrollments
  for (auto i = 0; i < 4; i++) {
    auto [status, template_id] = EnrollTemplate(handler.get());
    EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
    EXPECT_FALSE(template_id.empty());
  }

  // Enumerate to check enrollments.
  test::StatusAndValueCallbackReceiver<
      CtapDeviceResponseCode,
      absl::optional<std::map<std::vector<uint8_t>, std::string>>>
      cb;
  handler->EnumerateTemplates(cb.callback());
  cb.WaitForCallback();
  EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kSuccess);
  EXPECT_EQ(cb.value(),
            (std::map<std::vector<uint8_t>, std::string>{{{1}, "Template1"},
                                                         {{2}, "Template2"},
                                                         {{3}, "Template3"},
                                                         {{4}, "Template4"}}));
}

// Tests enrolling beyond maximum capacity.
TEST_F(BioEnrollmentHandlerTest, EnrollMax) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Enroll until full.
  CtapDeviceResponseCode status;
  BioEnrollmentHandler::TemplateId template_id;
  for (;;) {
    std::tie(status, template_id) = EnrollTemplate(handler.get());
    if (status != CtapDeviceResponseCode::kSuccess)
      break;
  }

  EXPECT_EQ(status, CtapDeviceResponseCode::kCtap2ErrFpDatabaseFull);
  EXPECT_TRUE(template_id.empty());
}

// Tests enumerating with no enrollments.
TEST_F(BioEnrollmentHandlerTest, EnumerateNone) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::StatusAndValueCallbackReceiver<
      CtapDeviceResponseCode,
      absl::optional<std::map<std::vector<uint8_t>, std::string>>>
      cb;
  handler->EnumerateTemplates(cb.callback());
  cb.WaitForCallback();
  EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);
  EXPECT_EQ(cb.value(), absl::nullopt);
}

// Tests enumerating with one enrollment.
TEST_F(BioEnrollmentHandlerTest, EnumerateOne) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Enroll - skip response validation
  auto [status, template_id] = EnrollTemplate(handler.get());
  EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
  EXPECT_FALSE(template_id.empty());

  // Enumerate
  test::StatusAndValueCallbackReceiver<
      CtapDeviceResponseCode,
      absl::optional<std::map<std::vector<uint8_t>, std::string>>>
      cb1;
  handler->EnumerateTemplates(cb1.callback());
  cb1.WaitForCallback();
  EXPECT_EQ(cb1.status(), CtapDeviceResponseCode::kSuccess);
  EXPECT_EQ(cb1.value(),
            (std::map<std::vector<uint8_t>, std::string>{{{1}, "Template1"}}));
}

// Tests renaming an enrollment (success and failure).
TEST_F(BioEnrollmentHandlerTest, Rename) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Rename non-existent enrollment.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb0;
  handler->RenameTemplate({1}, "OtherFingerprint1", cb0.callback());
  cb0.WaitForCallback();
  EXPECT_EQ(cb0.value(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);

  // Enroll - skip response validation.
  auto [status, template_id] = EnrollTemplate(handler.get());
  EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
  EXPECT_FALSE(template_id.empty());

  // Rename non-existent enrollment.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb2;
  handler->RenameTemplate(template_id, "OtherFingerprint1", cb2.callback());
  cb2.WaitForCallback();
  EXPECT_EQ(cb2.value(), CtapDeviceResponseCode::kSuccess);

  // Enumerate to validate renaming.
  test::StatusAndValueCallbackReceiver<
      CtapDeviceResponseCode,
      absl::optional<std::map<std::vector<uint8_t>, std::string>>>
      cb3;
  handler->EnumerateTemplates(cb3.callback());
  cb3.WaitForCallback();
  EXPECT_EQ(cb3.status(), CtapDeviceResponseCode::kSuccess);
  EXPECT_EQ(cb3.value(), (std::map<std::vector<uint8_t>, std::string>{
                             {template_id, "OtherFingerprint1"}}));
}

// Tests deleting an enrollment (success and failure).
TEST_F(BioEnrollmentHandlerTest, Delete) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Delete non-existent enrollment.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb0;
  handler->DeleteTemplate({1}, cb0.callback());
  cb0.WaitForCallback();
  EXPECT_EQ(cb0.value(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);

  // Enroll - skip response validation.
  auto [status, template_id] = EnrollTemplate(handler.get());
  EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
  EXPECT_FALSE(template_id.empty());

  // Delete existing enrollment.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb2;
  handler->DeleteTemplate({1}, cb2.callback());
  cb2.WaitForCallback();
  EXPECT_EQ(cb2.value(), CtapDeviceResponseCode::kSuccess);

  // Attempt to delete again to prove enrollment is gone.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb3;
  handler->DeleteTemplate({1}, cb3.callback());
  cb3.WaitForCallback();
  EXPECT_EQ(cb3.value(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);
}

// Test that enrollment succeeds even if one of the samples yields an error
// status. The error status should be propagated into the SampleCallback.
TEST_F(BioEnrollmentHandlerTest, SampleError) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);
  virtual_device_factory_.mutable_state()->bio_enrollment_next_sample_error =
      true;

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  auto [status, template_id] = EnrollTemplate(handler.get());
  EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
  EXPECT_EQ(sample_failures_, 1u);
}

// Test that enrollment succeeds even if one of the samples yields a timeout
// status. The timeout status should not propagate into the SampleCallback.
TEST_F(BioEnrollmentHandlerTest, SampleNoUserActivity) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);
  virtual_device_factory_.mutable_state()->bio_enrollment_next_sample_timeout =
      true;

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  auto [status, template_id] = EnrollTemplate(handler.get());
  EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
  EXPECT_EQ(sample_failures_, 0u);
}

}  // namespace
}  // namespace device

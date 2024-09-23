// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/bio/enrollment_handler.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

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
        ready_future_.GetCallback(), error_future_.GetCallback(),
        base::BindRepeating(&BioEnrollmentHandlerTest::GetPIN,
                            base::Unretained(this)),
        &virtual_device_factory_);
  }

  std::pair<CtapDeviceResponseCode, BioEnrollmentHandler::TemplateId>
  EnrollTemplate(BioEnrollmentHandler* handler) {
    base::test::TestFuture<CtapDeviceResponseCode,
                           BioEnrollmentHandler::TemplateId>
        future;
    handler->EnrollTemplate(MakeSampleCallback(), future.GetCallback());

    EXPECT_TRUE(future.Wait());
    return future.Get();
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
  base::test::TestFuture<BioEnrollmentHandler::SensorInfo> ready_future_;
  base::test::TestFuture<BioEnrollmentHandler::Error> error_future_;
  test::VirtualFidoDeviceFactory virtual_device_factory_;
};

// Tests bio enrollment handler against device without PIN support.
TEST_F(BioEnrollmentHandlerTest, NoPINSupport) {
  VirtualCtap2Device::Config config;
  config.pin_support = false;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  EXPECT_TRUE(error_future_.Wait());

  EXPECT_EQ(error_future_.Get(), BioEnrollmentHandler::Error::kNoPINSet);
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
  EXPECT_TRUE(error_future_.Wait());

  EXPECT_EQ(error_future_.Get(), BioEnrollmentHandler::Error::kForcePINChange);
}

// Tests enrollment handler PIN soft block.
TEST_F(BioEnrollmentHandlerTest, SoftPINBlock) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.mutable_state()->pin = "1234";
  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  EXPECT_TRUE(error_future_.Wait());

  EXPECT_EQ(error_future_.Get(), BioEnrollmentHandler::Error::kSoftPINBlock);
}

// Tests bio enrollment commands against an authenticator lacking support.
TEST_F(BioEnrollmentHandlerTest, NoBioEnrollmentSupport) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  EXPECT_TRUE(error_future_.Wait());
  EXPECT_EQ(error_future_.Get(),
            BioEnrollmentHandler::Error::kAuthenticatorMissingBioEnrollment);
}

// Tests fingerprint enrollment lifecycle.
TEST_F(BioEnrollmentHandlerTest, Enroll) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  EXPECT_TRUE(ready_future_.Wait());

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
  EXPECT_TRUE(ready_future_.Wait());

  // Multiple enrollments
  for (auto i = 0; i < 4; i++) {
    auto [status, template_id] = EnrollTemplate(handler.get());
    EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
    EXPECT_FALSE(template_id.empty());
  }

  // Enumerate to check enrollments.
  base::test::TestFuture<
      CtapDeviceResponseCode,
      std::optional<std::map<std::vector<uint8_t>, std::string>>>
      future;
  handler->EnumerateTemplates(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(std::get<0>(future.Get()), CtapDeviceResponseCode::kSuccess);
  EXPECT_EQ(std::get<1>(future.Get()),
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
  EXPECT_TRUE(ready_future_.Wait());

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
  EXPECT_TRUE(ready_future_.Wait());

  base::test::TestFuture<
      CtapDeviceResponseCode,
      std::optional<std::map<std::vector<uint8_t>, std::string>>>
      future;
  handler->EnumerateTemplates(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(std::get<0>(future.Get()),
            CtapDeviceResponseCode::kCtap2ErrInvalidOption);
  EXPECT_EQ(std::get<1>(future.Get()), std::nullopt);
}

// Tests enumerating with one enrollment.
TEST_F(BioEnrollmentHandlerTest, EnumerateOne) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  EXPECT_TRUE(ready_future_.Wait());

  // Enroll - skip response validation
  auto [status, template_id] = EnrollTemplate(handler.get());
  EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
  EXPECT_FALSE(template_id.empty());

  // Enumerate
  base::test::TestFuture<
      CtapDeviceResponseCode,
      std::optional<std::map<std::vector<uint8_t>, std::string>>>
      future;
  handler->EnumerateTemplates(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(std::get<0>(future.Get()), CtapDeviceResponseCode::kSuccess);
  EXPECT_EQ(std::get<1>(future.Get()),
            (std::map<std::vector<uint8_t>, std::string>{{{1}, "Template1"}}));
}

// Tests renaming an enrollment (success and failure).
TEST_F(BioEnrollmentHandlerTest, Rename) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  EXPECT_TRUE(ready_future_.Wait());

  // Rename non-existent enrollment.
  base::test::TestFuture<CtapDeviceResponseCode> future;
  handler->RenameTemplate({1}, "OtherFingerprint1", future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);

  // Enroll - skip response validation.
  auto [status, template_id] = EnrollTemplate(handler.get());
  EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
  EXPECT_FALSE(template_id.empty());

  // Rename non-existent enrollment.
  base::test::TestFuture<CtapDeviceResponseCode> future2;
  handler->RenameTemplate(template_id, "OtherFingerprint1",
                          future2.GetCallback());
  EXPECT_TRUE(future2.Wait());
  EXPECT_EQ(future2.Get(), CtapDeviceResponseCode::kSuccess);

  // Enumerate to validate renaming.
  base::test::TestFuture<
      CtapDeviceResponseCode,
      std::optional<std::map<std::vector<uint8_t>, std::string>>>
      future3;
  handler->EnumerateTemplates(future3.GetCallback());
  EXPECT_TRUE(future3.Wait());
  EXPECT_EQ(std::get<0>(future3.Get()), CtapDeviceResponseCode::kSuccess);
  EXPECT_EQ(std::get<1>(future3.Get()),
            (std::map<std::vector<uint8_t>, std::string>{
                {template_id, "OtherFingerprint1"}}));
}

// Tests deleting an enrollment (success and failure).
TEST_F(BioEnrollmentHandlerTest, Delete) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  EXPECT_TRUE(ready_future_.Wait());

  // Delete non-existent enrollment.
  base::test::TestFuture<CtapDeviceResponseCode> future0;
  handler->DeleteTemplate({1}, future0.GetCallback());
  EXPECT_TRUE(future0.Wait());
  EXPECT_EQ(future0.Get(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);

  // Enroll - skip response validation.
  auto [status, template_id] = EnrollTemplate(handler.get());
  EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
  EXPECT_FALSE(template_id.empty());

  // Delete existing enrollment.
  base::test::TestFuture<CtapDeviceResponseCode> future2;
  handler->DeleteTemplate({1}, future2.GetCallback());
  EXPECT_TRUE(future2.Wait());
  EXPECT_EQ(future2.Get(), CtapDeviceResponseCode::kSuccess);

  // Attempt to delete again to prove enrollment is gone.
  base::test::TestFuture<CtapDeviceResponseCode> future3;
  handler->DeleteTemplate({1}, future3.GetCallback());
  EXPECT_TRUE(future3.Wait());
  EXPECT_EQ(future3.Get(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);
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
  EXPECT_TRUE(ready_future_.Wait());

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
  EXPECT_TRUE(ready_future_.Wait());

  auto [status, template_id] = EnrollTemplate(handler.get());
  EXPECT_EQ(status, CtapDeviceResponseCode::kSuccess);
  EXPECT_EQ(sample_failures_, 0u);
}

}  // namespace
}  // namespace device

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_provider_mac.h"

#include <dlfcn.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/shape_detection/barcode_detection_impl_mac_vision.h"
#include "services/shape_detection/barcode_detection_provider_mac.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace shape_detection {

namespace {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

static const std::vector<mojom::BarcodeFormat>& CISupportedFormats = {
    mojom::BarcodeFormat::QR_CODE};
static const std::vector<mojom::BarcodeFormat>& VisionSupportedFormats = {
    mojom::BarcodeFormat::AZTEC,       mojom::BarcodeFormat::CODE_128,
    mojom::BarcodeFormat::CODE_39,     mojom::BarcodeFormat::CODE_93,
    mojom::BarcodeFormat::DATA_MATRIX, mojom::BarcodeFormat::EAN_13,
    mojom::BarcodeFormat::EAN_8,       mojom::BarcodeFormat::ITF,
    mojom::BarcodeFormat::PDF417,      mojom::BarcodeFormat::QR_CODE,
    mojom::BarcodeFormat::UPC_E};
static const std::vector<mojom::BarcodeFormat>& MockVisionSupportedFormats = {
    mojom::BarcodeFormat::AZTEC, mojom::BarcodeFormat::DATA_MATRIX,
    mojom::BarcodeFormat::QR_CODE};

static NSArray* MockVisionSupportedSymbologyStrings = @[
  @"VNBarcodeSymbologyAztec", @"VNBarcodeSymbologyDataMatrix",
  @"VNBarcodeSymbologyQR"
];

class MockVisionAPI : public VisionAPIInterface {
 public:
  MOCK_CONST_METHOD0(GetSupportedSymbologies, NSArray*(void));
};

std::unique_ptr<mojom::BarcodeDetectionProvider> CreateBarcodeProviderMac(
    std::unique_ptr<VisionAPIInterface> vision_api) {
  return std::make_unique<BarcodeDetectionProviderMac>(std::move(vision_api));
}

std::unique_ptr<VisionAPIInterface> CreateNullVisionAPI() {
  return nullptr;
}

std::unique_ptr<VisionAPIInterface> CreateVisionAPI() {
  return VisionAPIInterface::Create();
}

std::unique_ptr<VisionAPIInterface> CreateMockVisionAPI(
    NSArray* returned_symbologies) {
  std::unique_ptr<NiceMock<MockVisionAPI>> mock_vision_api =
      std::make_unique<NiceMock<MockVisionAPI>>();
  ON_CALL(*mock_vision_api, GetSupportedSymbologies())
      .WillByDefault(Return(returned_symbologies));
  return mock_vision_api;
}

using VisionAPIInterfaceFactory =
    base::RepeatingCallback<std::unique_ptr<VisionAPIInterface>()>;

struct TestParams {
  const std::vector<mojom::BarcodeFormat> formats;
  bool test_vision_api;
  VisionAPIInterfaceFactory vision_api;
} kTestParams[] = {
    {CISupportedFormats, false, base::BindRepeating(&CreateNullVisionAPI)},
    {VisionSupportedFormats, true, base::BindRepeating(&CreateVisionAPI)},
    {MockVisionSupportedFormats, true,
     base::BindRepeating(&CreateMockVisionAPI,
                         MockVisionSupportedSymbologyStrings)},
};
}

class BarcodeDetectionProviderMacTest
    : public TestWithParam<struct TestParams> {
 public:
  ~BarcodeDetectionProviderMacTest() override = default;

  void SetUp() override {
    if (@available(macOS 10.13, *))
      is_vision_available_ = true;
    ASSERT_EQ([MockVisionSupportedSymbologyStrings count],
              MockVisionSupportedFormats.size());
  }

  void TearDown() override {
    if (vision_framework_)
      dlclose(vision_framework_);
  }

  void EnumerateSupportedFormatsCallback(
      const std::vector<mojom::BarcodeFormat>& expected,
      const std::vector<mojom::BarcodeFormat>& results) {
    EXPECT_THAT(results,
                testing::ElementsAreArray(expected.begin(), expected.end()));

    OnEnumerateSupportedFormats();
  }
  MOCK_METHOD0(OnEnumerateSupportedFormats, void(void));

  std::unique_ptr<mojom::BarcodeDetectionProvider> provider_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  void* vision_framework_ = nullptr;
  bool is_vision_available_ = false;
};

TEST_P(BarcodeDetectionProviderMacTest, EnumerateSupportedBarcodes) {
  if (is_vision_available_ != GetParam().test_vision_api) {
    LOG(WARNING) << "Barcode Detection for this (library, OS version) pair is "
                    "not supported, skipping test.";
    return;
  }

  // Only load Vision if we're testing it.
  if (GetParam().test_vision_api) {
    vision_framework_ =
        dlopen("/System/Library/Frameworks/Vision.framework/Vision", RTLD_LAZY);
  }

  provider_ = CreateBarcodeProviderMac(GetParam().vision_api.Run());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, OnEnumerateSupportedFormats())
      .WillOnce(RunClosure(quit_closure));
  provider_->EnumerateSupportedFormats(base::BindOnce(
      &BarcodeDetectionProviderMacTest::EnumerateSupportedFormatsCallback,
      base::Unretained(this), GetParam().formats));
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(,
                         BarcodeDetectionProviderMacTest,
                         ValuesIn(kTestParams));

TEST_F(BarcodeDetectionProviderMacTest, EnumerateSupportedBarcodesCached) {
  if (!is_vision_available_) {
    LOG(WARNING) << "Barcode Detection with Vision not supported before 10.13, "
                 << "skipping test.";
    return;
  }

  auto mock_vision_api = std::make_unique<MockVisionAPI>();
  ON_CALL(*mock_vision_api, GetSupportedSymbologies())
      .WillByDefault(Return(MockVisionSupportedSymbologyStrings));
  EXPECT_CALL(*mock_vision_api, GetSupportedSymbologies());

  provider_ = CreateBarcodeProviderMac(std::move(mock_vision_api));
  provider_->EnumerateSupportedFormats(base::BindOnce(
      &BarcodeDetectionProviderMacTest::EnumerateSupportedFormatsCallback,
      base::Unretained(this), MockVisionSupportedFormats));
  provider_->EnumerateSupportedFormats(base::BindOnce(
      &BarcodeDetectionProviderMacTest::EnumerateSupportedFormatsCallback,
      base::Unretained(this), MockVisionSupportedFormats));
  provider_->EnumerateSupportedFormats(base::BindOnce(
      &BarcodeDetectionProviderMacTest::EnumerateSupportedFormatsCallback,
      base::Unretained(this), MockVisionSupportedFormats));
}

TEST_F(BarcodeDetectionProviderMacTest, EnumerateSupportedBarcodesUnknown) {
  if (!is_vision_available_) {
    LOG(WARNING) << "Barcode Detection with Vision not supported before 10.13, "
                 << "skipping test.";
    return;
  }

  NSMutableArray* mock_supported_symbologies =
      [NSMutableArray arrayWithArray:MockVisionSupportedSymbologyStrings];
  [mock_supported_symbologies addObject:@"FooSymbology"];
  std::unique_ptr<VisionAPIInterface> mock_vision_api =
      CreateMockVisionAPI(mock_supported_symbologies);

  provider_ = CreateBarcodeProviderMac(std::move(mock_vision_api));
  provider_->EnumerateSupportedFormats(base::BindOnce(
      &BarcodeDetectionProviderMacTest::EnumerateSupportedFormatsCallback,
      base::Unretained(this), MockVisionSupportedFormats));
}

TEST_F(BarcodeDetectionProviderMacTest, EnumerateSupportedBarcodesErrored) {
  if (!is_vision_available_) {
    LOG(WARNING) << "Barcode Detection with Vision not supported before 10.13, "
                 << "skipping test.";
    return;
  }

  std::unique_ptr<VisionAPIInterface> mock_vision_api =
      CreateMockVisionAPI([NSArray array]);

  provider_ = CreateBarcodeProviderMac(std::move(mock_vision_api));
  provider_->EnumerateSupportedFormats(base::BindOnce(
      &BarcodeDetectionProviderMacTest::EnumerateSupportedFormatsCallback,
      base::Unretained(this), std::vector<mojom::BarcodeFormat>()));
  provider_->EnumerateSupportedFormats(base::BindOnce(
      &BarcodeDetectionProviderMacTest::EnumerateSupportedFormatsCallback,
      base::Unretained(this), std::vector<mojom::BarcodeFormat>()));
  provider_->EnumerateSupportedFormats(base::BindOnce(
      &BarcodeDetectionProviderMacTest::EnumerateSupportedFormatsCallback,
      base::Unretained(this), std::vector<mojom::BarcodeFormat>()));
}

TEST_F(BarcodeDetectionProviderMacTest, HintFormats) {
  if (!is_vision_available_) {
    LOG(WARNING) << "Barcode Detection with Vision not supported before 10.13, "
                 << "skipping test.";
    return;
  }

  vision_framework_ =
      dlopen("/System/Library/Frameworks/Vision.framework/Vision", RTLD_LAZY);

  mojo::Remote<mojom::BarcodeDetectionProvider> provider_remote;
  mojo::MakeSelfOwnedReceiver(CreateBarcodeProviderMac(CreateVisionAPI()),
                              provider_remote.BindNewPipeAndPassReceiver());

  auto options = mojom::BarcodeDetectorOptions::New();
  options->formats = {mojom::BarcodeFormat::UNKNOWN};

  mojo::test::BadMessageObserver observer;
  mojo::Remote<mojom::BarcodeDetection> impl;
  provider_remote->CreateBarcodeDetection(impl.BindNewPipeAndPassReceiver(),
                                          std::move(options));

  EXPECT_EQ("Formats hint contains UNKNOWN BarcodeFormat.",
            observer.WaitForBadMessage());
}

}  // shape_detection namespace

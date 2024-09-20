// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/qr_generator/qr_generator_util.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class QRGeneratorUtilTest : public PlatformTest {
 public:
  QRGeneratorUtilTest() {}

 protected:
  void TearDown() override { [mainScreenPartialMock_ stopMocking]; }

  void RunValidSizeTest(CGFloat fakeScale) {
    // Mock mainScreen scale.
    mainScreenPartialMock_ = OCMPartialMock([UIScreen mainScreen]);
    OCMStub([(UIScreen*)mainScreenPartialMock_ scale]).andReturn(fakeScale);

    NSData* qrData = [sampleUrl_ dataUsingEncoding:NSUTF8StringEncoding];

    UIImage* qrImage = GenerateQRCode(qrData, imageSize_);

    EXPECT_EQ(imageSize_, qrImage.size.width);
    EXPECT_EQ(imageSize_, qrImage.size.height);
  }

  id mainScreenPartialMock_;
  NSString* sampleUrl_ = @"https://google.com/";
  CGFloat imageSize_ = 200.0;
};

// Tests that the GenerateQRCode utility function creates a valid QR code image.
// TODO(crbug.com/40769543): reenable this test.
TEST_F(QRGeneratorUtilTest, DISABLED_GenerateQRCode_ValidData) {
  NSData* qrData = [sampleUrl_ dataUsingEncoding:NSUTF8StringEncoding];

  UIImage* qrImage = GenerateQRCode(qrData, imageSize_);

  CIDetector* detector = [CIDetector detectorOfType:@"CIDetectorTypeQRCode"
                                            context:nil
                                            options:nil];

  NSArray<CIFeature*>* features = [detector featuresInImage:qrImage.CIImage];
  ASSERT_EQ(1U, [features count]);

  CIQRCodeFeature* qrCodeFeature = (CIQRCodeFeature*)features[0];
  EXPECT_NSEQ(sampleUrl_, [qrCodeFeature messageString]);
}

// Tests that GenerateQRCode utility function creates a QR code image of the
// requested size when the screen has 1x scale.
TEST_F(QRGeneratorUtilTest, GenerateQRCode_ValidSize_1xScale) {
  RunValidSizeTest(1.0);
}

// Tests that GenerateQRCode utility function creates a QR code image of the
// requested size when the screen has 2x scale.
TEST_F(QRGeneratorUtilTest, GenerateQRCode_ValidSize_2xScale) {
  RunValidSizeTest(2.0);
}

// Tests that GenerateQRCode utility function creates a QR code image of the
// requested size when the screen has 3x scale.
TEST_F(QRGeneratorUtilTest, GenerateQRCode_ValidSize_3xScale) {
  RunValidSizeTest(3.0);
}

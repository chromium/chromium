// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/ocmock_complex_type_helper.h"

#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// A complex type to test with..
struct SampleComplexType {
  int number;
  float blob;
};

typedef int ScalarType;

@protocol TestedProtocol
- (void)passObject:(id)foo;
- (void)passComplexType:(const SampleComplexType&)foo;
- (void)passScalar:(ScalarType)foo;
@end

@interface MockClass : OCMockComplexTypeHelper
@end

@implementation MockClass

typedef void (^complexTypeBlock)(const SampleComplexType&);

- (void)passComplexType:(const SampleComplexType&)foo {
  return static_cast<complexTypeBlock>([self blockForSelector:_cmd])(foo);
}

typedef void (^ScalarBlock)(const ScalarType&);

- (void)passScalar:(ScalarType)foo {
  return static_cast<ScalarBlock>([self blockForSelector:_cmd])(foo);
}

@end

namespace {

class OCMockComplexTypeHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    OCMockObject* protocol_mock =
        [OCMockObject mockForProtocol:@protocol(TestedProtocol)];
    helped_mock_ = [[MockClass alloc] initWithRepresentedObject:protocol_mock];
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(helped_mock_);
    PlatformTest::TearDown();
  }

  id helped_mock_;
};

TEST_F(OCMockComplexTypeHelperTest, nilObjectStillWorks) {
  [[helped_mock_ expect] passObject:nil];
  [helped_mock_ passObject:nil];
}

TEST_F(OCMockComplexTypeHelperTest, anyObjectStillWorks) {
  id someObject = [[NSObject alloc] init];
  [[helped_mock_ expect] passObject:OCMOCK_ANY];
  [helped_mock_ passObject:someObject];
}

TEST_F(OCMockComplexTypeHelperTest, complexType) {
  const SampleComplexType expected_value = {1, 1.0};

  complexTypeBlock block = ^(const SampleComplexType& value) {
    EXPECT_EQ(expected_value.number, value.number);
    EXPECT_EQ(expected_value.blob, value.blob);
  };
  [helped_mock_ onSelector:@selector(passComplexType:)
      callBlockExpectation:(id)block];

  [helped_mock_ passComplexType:expected_value];
}

TEST_F(OCMockComplexTypeHelperTest, scalarType) {
  const ScalarType expected_value = 42;

  ScalarBlock block = ^(const ScalarType& value) {
    EXPECT_EQ(expected_value, value);
  };
  [helped_mock_ onSelector:@selector(passScalar:)
      callBlockExpectation:(id)block];

  [helped_mock_ passScalar:expected_value];
}

}  // namespace

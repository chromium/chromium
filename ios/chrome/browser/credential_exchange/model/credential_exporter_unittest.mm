// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_exporter.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/credential_exchange/model/credential_export_manager_swift.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class CredentialExporterTest : public PlatformTest {
 protected:
  void SetUp() override {
    mock_delegate_ = OCMProtocolMock(@protocol(CredentialExporterDelegate));
    window_ = [[UIWindow alloc] init];
    exporter_ = [[CredentialExporter alloc] initWithWindow:window_
                                                  delegate:mock_delegate_];
  }

  base::test::TaskEnvironment task_environment_;
  id mock_delegate_;
  UIWindow* window_;
  CredentialExporter* exporter_;
};

TEST_F(CredentialExporterTest, PropagatesExportError) {
  [[mock_delegate_ expect] onExportError];

  [(id<CredentialExportManagerDelegate>)exporter_ onExportError];

  [mock_delegate_ verify];
}

}  // namespace

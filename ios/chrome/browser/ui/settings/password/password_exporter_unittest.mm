// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_exporter_for_testing.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/password_test_util.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakePasswordSerialzerBridge : NSObject <PasswordSerializerBridge>

// Allows for on demand execution of the block that handles the serialized
// passwords.
- (void)executeHandler;

@end

@implementation FakePasswordSerialzerBridge {
  // Handler processing the serialized passwords.
  void (^_serializedPasswordsHandler)(std::string);
}

- (void)serializePasswords:
            (std::vector<std::unique_ptr<autofill::PasswordForm>>)passwords
                   handler:(void (^)(std::string))serializedPasswordsHandler {
  _serializedPasswordsHandler = serializedPasswordsHandler;
}

- (void)executeHandler {
  _serializedPasswordsHandler("test serialized passwords string");
}

@end

@interface FakePasswordFileWriter : NSObject <FileWriterProtocol>

// Allows for on demand execution of the block that should be executed after
// the file has finished writing.
- (void)executeHandler;

// Indicates if the writing of the file was finished successfully or with an
// error.
@property(nonatomic, assign) WriteToURLStatus writingStatus;

// Whether a write operation was attempted.
@property(nonatomic, assign) BOOL writeAttempted;

@end

@implementation FakePasswordFileWriter {
  // Handler executed after the file write operation finishes.
  void (^_writeStatusHandler)(WriteToURLStatus);
}

@synthesize writingStatus = _writingStatus;
@synthesize writeAttempted = _writeAttempted;

- (instancetype)init {
  self = [super init];
  if (self) {
    _writeAttempted = NO;
  }
  return self;
}

- (void)writeData:(NSData*)data
            toURL:(NSURL*)fileURL
          handler:(void (^)(WriteToURLStatus))handler {
  _writeAttempted = YES;
  _writeStatusHandler = handler;
}

- (void)executeHandler {
  _writeStatusHandler(self.writingStatus);
}

@end

namespace {
class PasswordExporterTest : public PlatformTest {
 public:
  PasswordExporterTest() = default;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    mock_reauthentication_module_ = [[MockReauthenticationModule alloc] init];
    password_exporter_delegate_ =
        OCMProtocolMock(@protocol(PasswordExporterDelegate));
    password_exporter_ = [[PasswordExporter alloc]
        initWithReauthenticationModule:mock_reauthentication_module_
                              delegate:password_exporter_delegate_];
  }

  std::vector<std::unique_ptr<autofill::PasswordForm>> CreatePasswordList() {
    auto password_form = std::make_unique<autofill::PasswordForm>();
    password_form->origin = GURL("http://accounts.google.com/a/LoginAuth");
    password_form->username_value = base::ASCIIToUTF16("test@testmail.com");
    password_form->password_value = base::ASCIIToUTF16("test1");

    std::vector<std::unique_ptr<autofill::PasswordForm>> password_forms;
    password_forms.push_back(std::move(password_form));
    return password_forms;
  }

  id password_exporter_delegate_;
  PasswordExporter* password_exporter_;
  MockReauthenticationModule* mock_reauthentication_module_;
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

// Tests that when reauthentication is successful, writing the passwords file
// is attempted and a call to show the activity view is made.
TEST_F(PasswordExporterTest, PasswordFileWriteReauthSucceeded) {
  mock_reauthentication_module_.shouldSucceed = YES;
  FakePasswordFileWriter* fake_password_file_writer =
      [[FakePasswordFileWriter alloc] init];
  fake_password_file_writer.writingStatus = WriteToURLStatus::SUCCESS;
  [password_exporter_ setPasswordFileWriter:fake_password_file_writer];

  OCMExpect([password_exporter_delegate_
      showActivityViewWithActivityItems:[OCMArg any]
                      completionHandler:[OCMArg any]]);

  [password_exporter_ startExportFlow:CreatePasswordList()];

  // Wait for all asynchronous tasks to complete.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(fake_password_file_writer.writeAttempted);

  // Execute file write completion handler to continue the flow.
  [fake_password_file_writer executeHandler];

  EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
}

// Tests that if the file writing fails because of not enough disk space
// the appropriate error is displayed and the export operation
// is interrupted.
TEST_F(PasswordExporterTest, WritingFailedOutOfDiskSpace) {
  mock_reauthentication_module_.shouldSucceed = YES;
  FakePasswordFileWriter* fake_password_file_writer =
      [[FakePasswordFileWriter alloc] init];
  fake_password_file_writer.writingStatus =
      WriteToURLStatus::OUT_OF_DISK_SPACE_ERROR;
  [password_exporter_ setPasswordFileWriter:fake_password_file_writer];

  OCMExpect([password_exporter_delegate_
      showExportErrorAlertWithLocalizedReason:
          l10n_util::GetNSString(
              IDS_IOS_EXPORT_PASSWORDS_OUT_OF_SPACE_ALERT_MESSAGE)]);
  [[password_exporter_delegate_ reject]
      showActivityViewWithActivityItems:[OCMArg any]
                      completionHandler:[OCMArg any]];
  [password_exporter_ startExportFlow:CreatePasswordList()];

  // Wait for all asynchronous tasks to complete.
  task_environment_.RunUntilIdle();

  // Use @try/@catch as -reject raises an exception.
  @try {
    [fake_password_file_writer executeHandler];
    EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
  } @catch (NSException* exception) {
    // The exception is raised when
    // - showActivityViewWithActivityItems:completionHandler:
    // is invoked. As this should not happen, mark the test as failed.
    GTEST_FAIL();
  }

  // Failure to write the passwords file ends the export operation.
  EXPECT_EQ(ExportState::IDLE, password_exporter_.exportState);
}

// Tests that if a file write fails with an error other than not having
// enough disk space, the appropriate error is displayed and the export
// operation is interrupted.
TEST_F(PasswordExporterTest, WritingFailedUnknownError) {
  mock_reauthentication_module_.shouldSucceed = YES;
  FakePasswordFileWriter* fake_password_file_writer =
      [[FakePasswordFileWriter alloc] init];
  fake_password_file_writer.writingStatus = WriteToURLStatus::UNKNOWN_ERROR;
  [password_exporter_ setPasswordFileWriter:fake_password_file_writer];

  OCMExpect([password_exporter_delegate_
      showExportErrorAlertWithLocalizedReason:
          l10n_util::GetNSString(
              IDS_IOS_EXPORT_PASSWORDS_UNKNOWN_ERROR_ALERT_MESSAGE)]);
  [[password_exporter_delegate_ reject]
      showActivityViewWithActivityItems:[OCMArg any]
                      completionHandler:[OCMArg any]];
  [password_exporter_ startExportFlow:CreatePasswordList()];

  // Wait for all asynchronous tasks to complete.
  task_environment_.RunUntilIdle();

  // Use @try/@catch as -reject raises an exception.
  @try {
    [fake_password_file_writer executeHandler];
    EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
  } @catch (NSException* exception) {
    // The exception is raised when
    // - showActivityViewWithActivityItems:completionHandler:
    // is invoked. As this should not happen, mark the test as failed.
    GTEST_FAIL();
  }

  // Failure to write the passwords file ends the export operation.
  EXPECT_EQ(ExportState::IDLE, password_exporter_.exportState);
}

// Tests that when reauthentication fails the export flow is interrupted.
TEST_F(PasswordExporterTest, ExportInterruptedWhenReauthFails) {
  mock_reauthentication_module_.shouldSucceed = NO;
  FakePasswordSerialzerBridge* fake_password_serializer_bridge =
      [[FakePasswordSerialzerBridge alloc] init];
  [password_exporter_
      setPasswordSerializerBridge:fake_password_serializer_bridge];

  FakePasswordFileWriter* fake_password_file_writer =
      [[FakePasswordFileWriter alloc] init];
  [password_exporter_ setPasswordFileWriter:fake_password_file_writer];

  [[password_exporter_delegate_ reject]
      showActivityViewWithActivityItems:[OCMArg any]
                      completionHandler:[OCMArg any]];

  // Use @try/@catch as -reject raises an exception.
  @try {
    [password_exporter_ startExportFlow:CreatePasswordList()];

    // Wait for all asynchronous tasks to complete.
    task_environment_.RunUntilIdle();
    EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
  } @catch (NSException* exception) {
    // The exception is raised when
    // - showActivityViewWithActivityItems:completionHandler:
    // is invoked. As this should not happen, mark the test as failed.
    GTEST_FAIL();
  }
  // Serializing passwords hasn't finished.
  EXPECT_EQ(ExportState::ONGOING, password_exporter_.exportState);

  [fake_password_serializer_bridge executeHandler];

  // Make sure this test doesn't pass only because file writing hasn't finished
  // yet.
  task_environment_.RunUntilIdle();

  // Serializing passwords has finished, but reauthentication was not
  // successful, so writing the file was not attempted.
  EXPECT_FALSE(fake_password_file_writer.writeAttempted);
  EXPECT_EQ(ExportState::IDLE, password_exporter_.exportState);
}

// Tests that cancelling the export while serialization is still ongoing
// waits for it to finish before cleaning up.
TEST_F(PasswordExporterTest, CancelWaitsForSerializationFinished) {
  mock_reauthentication_module_.shouldSucceed = YES;
  FakePasswordSerialzerBridge* fake_password_serializer_bridge =
      [[FakePasswordSerialzerBridge alloc] init];
  [password_exporter_
      setPasswordSerializerBridge:fake_password_serializer_bridge];

  FakePasswordFileWriter* fake_password_file_writer =
      [[FakePasswordFileWriter alloc] init];
  [password_exporter_ setPasswordFileWriter:fake_password_file_writer];

  [[password_exporter_delegate_ reject]
      showActivityViewWithActivityItems:[OCMArg any]
                      completionHandler:[OCMArg any]];

  [password_exporter_ startExportFlow:CreatePasswordList()];
  [password_exporter_ cancelExport];
  EXPECT_EQ(ExportState::CANCELLING, password_exporter_.exportState);

  // Use @try/@catch as -reject raises an exception.
  @try {
    [fake_password_serializer_bridge executeHandler];
    // Wait for all asynchronous tasks to complete.
    task_environment_.RunUntilIdle();
    EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
  } @catch (NSException* exception) {
    // The exception is raised when
    // - showActivityViewWithActivityItems:completionHandler:
    // is invoked. As this should not happen, mark the test as failed.
    GTEST_FAIL();
  }
  EXPECT_FALSE(fake_password_file_writer.writeAttempted);
  EXPECT_EQ(ExportState::IDLE, password_exporter_.exportState);
}

// Tests that if the export is cancelled before writing to file finishes
// successfully the request to show the activity controller isn't made.
TEST_F(PasswordExporterTest, CancelledBeforeWriteToFileFinishesSuccessfully) {
  mock_reauthentication_module_.shouldSucceed = YES;
  FakePasswordFileWriter* fake_password_file_writer =
      [[FakePasswordFileWriter alloc] init];
  fake_password_file_writer.writingStatus = WriteToURLStatus::SUCCESS;
  [password_exporter_ setPasswordFileWriter:fake_password_file_writer];

  [[password_exporter_delegate_ reject]
      showActivityViewWithActivityItems:[OCMArg any]
                      completionHandler:[OCMArg any]];

  [password_exporter_ startExportFlow:CreatePasswordList()];
  // Wait for all asynchronous tasks to complete.
  task_environment_.RunUntilIdle();
  [password_exporter_ cancelExport];
  EXPECT_EQ(ExportState::CANCELLING, password_exporter_.exportState);

  // Use @try/@catch as -reject raises an exception.
  @try {
    [fake_password_file_writer executeHandler];
    EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
  } @catch (NSException* exception) {
    // The exception is raised when
    // - showActivityViewWithActivityItems:completionHandler:
    // is invoked. As this should not happen, mark the test as failed.
    GTEST_FAIL();
  }
  EXPECT_EQ(ExportState::IDLE, password_exporter_.exportState);
}

// Tests that if the export is cancelled before writing to file fails
// with an error, the request to show the error alert isn't made.
TEST_F(PasswordExporterTest, CancelledBeforeWriteToFileFails) {
  mock_reauthentication_module_.shouldSucceed = YES;
  FakePasswordFileWriter* fake_password_file_writer =
      [[FakePasswordFileWriter alloc] init];
  fake_password_file_writer.writingStatus = WriteToURLStatus::UNKNOWN_ERROR;
  [password_exporter_ setPasswordFileWriter:fake_password_file_writer];

  [[password_exporter_delegate_ reject]
      showExportErrorAlertWithLocalizedReason:[OCMArg any]];

  [password_exporter_ startExportFlow:CreatePasswordList()];
  // Wait for all asynchronous tasks to complete.
  task_environment_.RunUntilIdle();
  [password_exporter_ cancelExport];
  EXPECT_EQ(ExportState::CANCELLING, password_exporter_.exportState);

  // Use @try/@catch as -reject raises an exception.
  @try {
    [fake_password_file_writer executeHandler];
    EXPECT_OCMOCK_VERIFY(password_exporter_delegate_);
  } @catch (NSException* exception) {
    // The exception is raised when
    // - showExportErrorAlertWithLocalizedReason:
    // is invoked. As this should not happen, mark the test as failed.
    GTEST_FAIL();
  }
  EXPECT_EQ(ExportState::IDLE, password_exporter_.exportState);
}

}  // namespace

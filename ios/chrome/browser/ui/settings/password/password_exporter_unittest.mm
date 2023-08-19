// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_exporter_for_testing.h"

#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

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
            (const std::vector<password_manager::CredentialUIEntry>&)passwords
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

  std::vector<password_manager::CredentialUIEntry> CreatePasswordList() {
    password_manager::PasswordForm password_form;
    password_form.url = GURL("http://accounts.google.com/a/LoginAuth");
    password_form.username_value = u"test@testmail.com";
    password_form.password_value = u"test1";

    return {password_manager::CredentialUIEntry(password_form)};
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
  mock_reauthentication_module_.expectedResult =
      ReauthenticationResult::kSuccess;
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
  mock_reauthentication_module_.expectedResult =
      ReauthenticationResult::kSuccess;
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
  mock_reauthentication_module_.expectedResult =
      ReauthenticationResult::kSuccess;
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
  mock_reauthentication_module_.expectedResult =
      ReauthenticationResult::kFailure;
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
  mock_reauthentication_module_.expectedResult =
      ReauthenticationResult::kSuccess;
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
  mock_reauthentication_module_.expectedResult =
      ReauthenticationResult::kSuccess;
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
  mock_reauthentication_module_.expectedResult =
      ReauthenticationResult::kSuccess;
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

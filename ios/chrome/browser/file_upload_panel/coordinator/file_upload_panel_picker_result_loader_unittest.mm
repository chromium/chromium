// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_picker_result_loader.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/mock_callback.h"
#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_media_item.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class FileUploadPanelPickerResultLoaderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    web_state_ = std::make_unique<web::FakeWebState>();
  }

  web::WebTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that the loader can be created and destroyed.
TEST_F(FileUploadPanelPickerResultLoaderTest, CreateAndDestroy) {
  FileUploadPanelPickerResultLoader loader(@[],
                                           web_state_ -> GetUniqueIdentifier());
}

// Tests a successful load of one item.
TEST_F(FileUploadPanelPickerResultLoaderTest, LoadSuccess) {
  id item_provider = OCMClassMock([NSItemProvider class]);
  OCMStub([item_provider registeredTypeIdentifiers]).andReturn(@[
    UTTypeImage.identifier
  ]);
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
  NSURL* file_url =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(file_path.AsUTF8Unsafe())];
  OCMStub([item_provider
      loadFileRepresentationForTypeIdentifier:UTTypeImage.identifier
                            completionHandler:([OCMArg
                                                  invokeBlockWithArgs:file_url,
                                                                      [NSNull
                                                                          null],
                                                                      nil])]);

  id result = OCMClassMock([PHPickerResult class]);
  OCMStub([result itemProvider]).andReturn(item_provider);

  FileUploadPanelPickerResultLoader loader(@[ result ],
                                           web_state_ -> GetUniqueIdentifier());

  testing::StrictMock<
      base::MockCallback<FileUploadPanelPickerResultLoader::LoadResultCallback>>
      callback;
  base::RepeatingClosure quit_run_loop = task_environment_.QuitClosure();
  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce([&](NSArray<FileUploadPanelMediaItem*>* items) {
        ASSERT_EQ(1u, items.count);
        EXPECT_FALSE(items[0].isVideo);
        quit_run_loop.Run();
      });

  loader.Load(callback.Get());
  task_environment_.RunUntilQuit();
  histogram_tester_.ExpectUniqueSample(
      "IOS.FileUploadPanel.PhotoPicker.ResultLoader.Result", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "IOS.FileUploadPanel.PhotoPicker.ResultLoader.FileCount", 1, 1);
}

// Tests a failed load.
TEST_F(FileUploadPanelPickerResultLoaderTest, LoadFailure) {
  id item_provider = OCMClassMock([NSItemProvider class]);
  OCMStub([item_provider registeredTypeIdentifiers]).andReturn(@[
    UTTypeImage.identifier
  ]);
  NSError* error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMStub([item_provider
      loadFileRepresentationForTypeIdentifier:UTTypeImage.identifier
                            completionHandler:([OCMArg
                                                  invokeBlockWithArgs:[NSNull
                                                                          null],
                                                                      error,
                                                                      nil])]);

  id result = OCMClassMock([PHPickerResult class]);
  OCMStub([result itemProvider]).andReturn(item_provider);

  FileUploadPanelPickerResultLoader loader(@[ result ],
                                           web_state_ -> GetUniqueIdentifier());

  testing::StrictMock<
      base::MockCallback<FileUploadPanelPickerResultLoader::LoadResultCallback>>
      callback;
  base::RepeatingClosure quit_run_loop = task_environment_.QuitClosure();
  EXPECT_CALL(callback, Run(nil)).WillOnce([&]() { quit_run_loop.Run(); });

  loader.Load(callback.Get());
  task_environment_.RunUntilQuit();
  histogram_tester_.ExpectUniqueSample(
      "IOS.FileUploadPanel.PhotoPicker.ResultLoader.Result", false, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.FileUploadPanel.PhotoPicker.ResultLoader.FileCount", 0);
}

// Tests a successful load of multiple items.
TEST_F(FileUploadPanelPickerResultLoaderTest, LoadMultipleSuccess) {
  id item_provider1 = OCMClassMock([NSItemProvider class]);
  OCMStub([item_provider1 registeredTypeIdentifiers]).andReturn(@[
    UTTypeImage.identifier
  ]);
  base::FilePath file_path1;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path1));
  NSURL* file_url1 = [NSURL
      fileURLWithPath:base::SysUTF8ToNSString(file_path1.AsUTF8Unsafe())];
  OCMStub([item_provider1
      loadFileRepresentationForTypeIdentifier:UTTypeImage.identifier
                            completionHandler:([OCMArg
                                                  invokeBlockWithArgs:file_url1,
                                                                      [NSNull
                                                                          null],
                                                                      nil])]);

  id result1 = OCMClassMock([PHPickerResult class]);
  OCMStub([result1 itemProvider]).andReturn(item_provider1);

  id item_provider2 = OCMClassMock([NSItemProvider class]);
  OCMStub([item_provider2 registeredTypeIdentifiers]).andReturn(@[
    UTTypeVideo.identifier
  ]);
  base::FilePath file_path2;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path2));
  NSURL* file_url2 = [NSURL
      fileURLWithPath:base::SysUTF8ToNSString(file_path2.AsUTF8Unsafe())];
  OCMStub([item_provider2
      loadFileRepresentationForTypeIdentifier:UTTypeVideo.identifier
                            completionHandler:([OCMArg
                                                  invokeBlockWithArgs:file_url2,
                                                                      [NSNull
                                                                          null],
                                                                      nil])]);

  id result2 = OCMClassMock([PHPickerResult class]);
  OCMStub([result2 itemProvider]).andReturn(item_provider2);

  FileUploadPanelPickerResultLoader loader(@[ result1, result2 ],
                                           web_state_ -> GetUniqueIdentifier());

  testing::StrictMock<
      base::MockCallback<FileUploadPanelPickerResultLoader::LoadResultCallback>>
      callback;
  base::RepeatingClosure quit_run_loop = task_environment_.QuitClosure();
  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce([&](NSArray<FileUploadPanelMediaItem*>* items) {
        ASSERT_EQ(2u, items.count);
        EXPECT_FALSE(items[0].isVideo);
        EXPECT_TRUE(items[1].isVideo);
        quit_run_loop.Run();
      });

  loader.Load(callback.Get());
  task_environment_.RunUntilQuit();
  histogram_tester_.ExpectUniqueSample(
      "IOS.FileUploadPanel.PhotoPicker.ResultLoader.Result", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "IOS.FileUploadPanel.PhotoPicker.ResultLoader.FileCount", 2, 1);
}

// Tests a partial failure load of multiple items.
TEST_F(FileUploadPanelPickerResultLoaderTest, LoadMultiplePartialFailure) {
  id item_provider1 = OCMClassMock([NSItemProvider class]);
  OCMStub([item_provider1 registeredTypeIdentifiers]).andReturn(@[
    UTTypeImage.identifier
  ]);
  base::FilePath file_path1;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path1));
  NSURL* file_url1 = [NSURL
      fileURLWithPath:base::SysUTF8ToNSString(file_path1.AsUTF8Unsafe())];
  OCMStub([item_provider1
      loadFileRepresentationForTypeIdentifier:UTTypeImage.identifier
                            completionHandler:([OCMArg
                                                  invokeBlockWithArgs:file_url1,
                                                                      [NSNull
                                                                          null],
                                                                      nil])]);

  id result1 = OCMClassMock([PHPickerResult class]);
  OCMStub([result1 itemProvider]).andReturn(item_provider1);

  id item_provider2 = OCMClassMock([NSItemProvider class]);
  OCMStub([item_provider2 registeredTypeIdentifiers]).andReturn(@[
    UTTypeVideo.identifier
  ]);
  NSError* error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMStub([item_provider2
      loadFileRepresentationForTypeIdentifier:UTTypeVideo.identifier
                            completionHandler:([OCMArg
                                                  invokeBlockWithArgs:[NSNull
                                                                          null],
                                                                      error,
                                                                      nil])]);

  id result2 = OCMClassMock([PHPickerResult class]);
  OCMStub([result2 itemProvider]).andReturn(item_provider2);

  FileUploadPanelPickerResultLoader loader(@[ result1, result2 ],
                                           web_state_ -> GetUniqueIdentifier());

  testing::StrictMock<
      base::MockCallback<FileUploadPanelPickerResultLoader::LoadResultCallback>>
      callback;
  base::RepeatingClosure quit_run_loop = task_environment_.QuitClosure();
  EXPECT_CALL(callback, Run(nil)).WillOnce([&]() { quit_run_loop.Run(); });

  loader.Load(callback.Get());
  task_environment_.RunUntilQuit();
  histogram_tester_.ExpectUniqueSample(
      "IOS.FileUploadPanel.PhotoPicker.ResultLoader.Result", false, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.FileUploadPanel.PhotoPicker.ResultLoader.FileCount", 0);
}

// Tests a total failure load of multiple items.
TEST_F(FileUploadPanelPickerResultLoaderTest, LoadMultipleTotalFailure) {
  id item_provider1 = OCMClassMock([NSItemProvider class]);
  OCMStub([item_provider1 registeredTypeIdentifiers]).andReturn(@[
    UTTypeImage.identifier
  ]);
  NSError* error1 = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMStub([item_provider1
      loadFileRepresentationForTypeIdentifier:UTTypeImage.identifier
                            completionHandler:([OCMArg
                                                  invokeBlockWithArgs:[NSNull
                                                                          null],
                                                                      error1,
                                                                      nil])]);

  id result1 = OCMClassMock([PHPickerResult class]);
  OCMStub([result1 itemProvider]).andReturn(item_provider1);

  id item_provider2 = OCMClassMock([NSItemProvider class]);
  OCMStub([item_provider2 registeredTypeIdentifiers]).andReturn(@[
    UTTypeVideo.identifier
  ]);
  NSError* error2 = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  OCMStub([item_provider2
      loadFileRepresentationForTypeIdentifier:UTTypeVideo.identifier
                            completionHandler:([OCMArg
                                                  invokeBlockWithArgs:[NSNull
                                                                          null],
                                                                      error2,
                                                                      nil])]);

  id result2 = OCMClassMock([PHPickerResult class]);
  OCMStub([result2 itemProvider]).andReturn(item_provider2);

  FileUploadPanelPickerResultLoader loader(@[ result1, result2 ],
                                           web_state_ -> GetUniqueIdentifier());

  testing::StrictMock<
      base::MockCallback<FileUploadPanelPickerResultLoader::LoadResultCallback>>
      callback;
  base::RepeatingClosure quit_run_loop = task_environment_.QuitClosure();
  EXPECT_CALL(callback, Run(nil)).WillOnce([&]() { quit_run_loop.Run(); });

  loader.Load(callback.Get());
  task_environment_.RunUntilQuit();
  histogram_tester_.ExpectUniqueSample(
      "IOS.FileUploadPanel.PhotoPicker.ResultLoader.Result", false, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.FileUploadPanel.PhotoPicker.ResultLoader.FileCount", 0);
}

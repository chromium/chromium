// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <memory>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/path_service.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/paths/paths.h"
#import "ios/chrome/browser/sessions/session_features.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_ios_factory.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Fixture Class. Takes care of deleting the directory used to store test data.
class SessionServiceTest : public PlatformTest {
 public:
  SessionServiceTest() = default;

  SessionServiceTest(const SessionServiceTest&) = delete;
  SessionServiceTest& operator=(const SessionServiceTest&) = delete;

  ~SessionServiceTest() override = default;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
    directory_ = scoped_temp_directory_.GetPath();

    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::SingleThreadTaskRunner::GetCurrentDefault();
    session_service_ =
        [[SessionServiceIOS alloc] initWithTaskRunner:task_runner];
  }

  void TearDown() override {
    session_service_ = nil;
    PlatformTest::TearDown();
  }

  // Returns a WebStateList with `tabs_count` WebStates and activates the first
  // WebState.
  std::unique_ptr<WebStateList> CreateWebStateList(int tabs_count) {
    std::unique_ptr<WebStateList> web_state_list =
        std::make_unique<WebStateList>(&web_state_list_delegate_);
    for (int i = 0; i < tabs_count; ++i) {
      auto web_state = std::make_unique<web::FakeWebState>([@(i) stringValue]);
      web_state->SetNavigationItemCount(1);
      web_state_list->InsertWebState(i, std::move(web_state),
                                     WebStateList::INSERT_FORCE_INDEX,
                                     WebStateOpener());
    }
    if (tabs_count > 0)
      web_state_list->ActivateWebStateAt(0);
    return web_state_list;
  }

  // Returns the path to serialized SessionWindowIOS from a testdata file named
  // `filename` or nil if the file cannot be found.
  NSString* SessionPathForTestData(const base::FilePath::CharType* filename) {
    base::FilePath session_path;
    if (!base::PathService::Get(ios::DIR_TEST_DATA, &session_path))
      return nil;

    session_path = session_path.Append(FILE_PATH_LITERAL("sessions"));
    session_path = session_path.Append(filename);
    if (!base::PathExists(session_path))
      return nil;

    return base::SysUTF8ToNSString(session_path.AsUTF8Unsafe());
  }

  SessionServiceIOS* session_service() { return session_service_; }

  const base::FilePath& directory() const { return directory_; }

  NSString* directory_as_nsstring() const {
    return base::SysUTF8ToNSString(directory().AsUTF8Unsafe());
  }

 private:
  base::ScopedTempDir scoped_temp_directory_;
  base::test::TaskEnvironment task_environment_;
  SessionServiceIOS* session_service_ = nil;
  FakeWebStateListDelegate web_state_list_delegate_;
  base::FilePath directory_;
};

TEST_F(SessionServiceTest, SessionPathForDirectory) {
  const base::FilePath root(FILE_PATH_LITERAL("root"));
  EXPECT_NSEQ(@"root/Sessions/session-id/session.plist",
              [SessionServiceIOS sessionPathForSessionID:@"session-id"
                                               directory:root]);
}

TEST_F(SessionServiceTest, SaveSessionWindowToPath) {
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(0);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];

  NSString* session_id = [[NSUUID UUID] UUIDString];
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:YES];

  // Even if `immediately` is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();

  NSFileManager* file_manager = [NSFileManager defaultManager];
  EXPECT_TRUE([file_manager removeItemAtPath:directory_as_nsstring()
                                       error:nullptr]);
}

TEST_F(SessionServiceTest, SaveSessionWindowToPathDirectoryExists) {
  ASSERT_TRUE([[NSFileManager defaultManager]
            createDirectoryAtPath:directory_as_nsstring()
      withIntermediateDirectories:YES
                       attributes:nil
                            error:nullptr]);
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(0);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];

  NSString* session_id = [[NSUUID UUID] UUIDString];
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:YES];

  // Even if `immediately` is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();

  NSFileManager* file_manager = [NSFileManager defaultManager];
  EXPECT_TRUE([file_manager removeItemAtPath:directory_as_nsstring()
                                       error:nullptr]);
}

// Tests that sessions are saved in separated files if feature is enabled.
TEST_F(SessionServiceTest, SeparateFiles_SaveSession) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(sessions::kSaveSessionTabsToSeparateFiles);

  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];
  NSString* session_id = [[NSUUID UUID] UUIDString];
  NSString* session_path =
      [SessionServiceIOS sessionPathForSessionID:session_id
                                       directory:directory()];
  NSFileManager* file_manager = [NSFileManager defaultManager];

  // Dirty WebState should be saved
  for (int i = 0; i < 2; i++) {
    [factory markWebStateDirty:web_state_list->GetWebStateAt(i)];
  }
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:YES];
  // Even if `immediately` is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < 2; i++) {
    NSString* session_tab_path =
        [SessionServiceIOS filePathForTabID:[@(i) stringValue]
                                sessionPath:session_path];
    EXPECT_TRUE([file_manager fileExistsAtPath:session_tab_path]);
    NSData* file_contents = [NSData dataWithContentsOfFile:session_tab_path];
    CRWSessionStorage* storage =
        web_state_list->GetWebStateAt(i)->BuildSessionStorage();
    NSError* error = nil;
    NSData* data = [NSKeyedArchiver archivedDataWithRootObject:storage
                                         requiringSecureCoding:NO
                                                         error:&error];
    EXPECT_NSEQ(file_contents, data);
  }
}

// Tests that only dirty webStates are saved. Not dirty files are not touched.
TEST_F(SessionServiceTest, SeparateFiles_OnlyDirtySaved) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(sessions::kSaveSessionTabsToSeparateFiles);

  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];
  NSString* session_id = [[NSUUID UUID] UUIDString];
  NSString* session_path =
      [SessionServiceIOS sessionPathForSessionID:session_id
                                       directory:directory()];

  ASSERT_TRUE([[NSFileManager defaultManager]
            createDirectoryAtPath:[session_path
                                      stringByDeletingLastPathComponent]
      withIntermediateDirectories:YES
                       attributes:nil
                            error:nullptr]);

  // Write sentinel files
  for (int i = 0; i < 2; i++) {
    NSString* session_tab_path =
        [SessionServiceIOS filePathForTabID:[@(i) stringValue]
                                sessionPath:session_path];
    [[session_tab_path dataUsingEncoding:NSUTF8StringEncoding]
        writeToFile:session_tab_path
         atomically:NO];
  }
  // Only mark webstate 1 as dirty
  [factory markWebStateDirty:web_state_list->GetWebStateAt(1)];
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:YES];
  // Even if `immediately` is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();

  // Check file 0 is unchanged
  NSString* session_tab_0_path =
      [SessionServiceIOS filePathForTabID:@"0" sessionPath:session_path];
  NSData* file_contents_0 = [NSData dataWithContentsOfFile:session_tab_0_path];
  EXPECT_NSEQ(file_contents_0,
              [session_tab_0_path dataUsingEncoding:NSUTF8StringEncoding]);

  // Check file 1 is saved
  NSString* session_tab_1_path =
      [SessionServiceIOS filePathForTabID:@"1" sessionPath:session_path];
  NSData* file_contents_1 = [NSData dataWithContentsOfFile:session_tab_1_path];
  CRWSessionStorage* storage =
      web_state_list->GetWebStateAt(1)->BuildSessionStorage();
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:storage
                                       requiringSecureCoding:NO
                                                       error:&error];
  EXPECT_NSEQ(file_contents_1, data);
}

// Tests that obsolete files are deleted.
TEST_F(SessionServiceTest, SeparateFiles_CleanFiles) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(sessions::kSaveSessionTabsToSeparateFiles);

  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];
  NSString* session_id = [[NSUUID UUID] UUIDString];
  NSString* session_path =
      [SessionServiceIOS sessionPathForSessionID:session_id
                                       directory:directory()];

  NSFileManager* file_manager = [NSFileManager defaultManager];
  ASSERT_TRUE([file_manager
            createDirectoryAtPath:[session_path
                                      stringByDeletingLastPathComponent]
      withIntermediateDirectories:YES
                       attributes:nil
                            error:nullptr]);

  // Write 3 sentinel files, last one representing a tab that no longer exists.
  for (int i = 0; i < 3; i++) {
    NSString* session_tab_path =
        [SessionServiceIOS filePathForTabID:[@(i) stringValue]
                                sessionPath:session_path];
    [[session_tab_path dataUsingEncoding:NSUTF8StringEncoding]
        writeToFile:session_tab_path
         atomically:NO];
  }
  // Create another file in the directory
  NSString* unrelated_path = [[session_path stringByDeletingLastPathComponent]
      stringByAppendingPathComponent:@"unrelated"];
  [[unrelated_path dataUsingEncoding:NSUTF8StringEncoding]
      writeToFile:unrelated_path
       atomically:NO];

  // Close a webState. The file should disappear on next saving.
  web_state_list->CloseWebStateAt(1, WebStateList::CLOSE_USER_ACTION);

  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:YES];

  // Even if `immediately` is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();

  // WebState 0 should have been saved normally, content unchanged as it is not
  // dirty
  NSString* session_tab_0_path =
      [SessionServiceIOS filePathForTabID:@"0" sessionPath:session_path];
  NSData* file_contents_0 = [NSData dataWithContentsOfFile:session_tab_0_path];
  EXPECT_NSEQ(file_contents_0,
              [session_tab_0_path dataUsingEncoding:NSUTF8StringEncoding]);

  // File 1 (WebState closed) should have been deleted
  NSString* session_tab_1_path =
      [SessionServiceIOS filePathForTabID:@"1" sessionPath:session_path];
  EXPECT_FALSE([file_manager fileExistsAtPath:session_tab_1_path]);

  // File 2 (obsolete) should have been deleted
  NSString* session_tab_2_path =
      [SessionServiceIOS filePathForTabID:@"2" sessionPath:session_path];
  EXPECT_FALSE([file_manager fileExistsAtPath:session_tab_2_path]);

  // Unrelated file should still exist
  EXPECT_TRUE([file_manager fileExistsAtPath:unrelated_path]);
  NSData* file_contents_unrelated =
      [NSData dataWithContentsOfFile:unrelated_path];
  EXPECT_NSEQ(file_contents_unrelated,
              [unrelated_path dataUsingEncoding:NSUTF8StringEncoding]);
}

TEST_F(SessionServiceTest, LoadSessionFromDirectoryNoFile) {
  NSString* session_id = [[NSUUID UUID] UUIDString];
  SessionIOS* session =
      [session_service() loadSessionWithSessionID:session_id
                                        directory:directory()];
  EXPECT_TRUE(session == nil);
}

// Tests that the session service doesn't retain the SessionIOSFactory, and that
// SaveSession will be no-op if the factory is destroyed earlier.
TEST_F(SessionServiceTest, SaveExpiredSession) {
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];

  NSString* session_id = [[NSUUID UUID] UUIDString];
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:NO];
  [factory disconnect];
  factory = nil;
  // Make sure that the delay for saving a session has passed (at least 2.5
  // seconds)
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(2.5));
  base::RunLoop().RunUntilIdle();

  SessionIOS* session =
      [session_service() loadSessionWithSessionID:session_id
                                        directory:directory()];
  EXPECT_FALSE(session);
}

TEST_F(SessionServiceTest, LoadSessionFromDirectory) {
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];

  NSString* session_id = [[NSUUID UUID] UUIDString];
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:YES];

  // Even if `immediately` is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();

  SessionIOS* session =
      [session_service() loadSessionWithSessionID:session_id
                                        directory:directory()];
  EXPECT_EQ(1u, session.sessionWindows.count);
  EXPECT_EQ(2u, session.sessionWindows[0].sessions.count);
  EXPECT_EQ(0u, session.sessionWindows[0].selectedIndex);
}

TEST_F(SessionServiceTest, LoadSessionFromPath) {
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];

  NSString* session_id = [[NSUUID UUID] UUIDString];
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:YES];

  // Even if `immediately` is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();

  NSString* session_path =
      [SessionServiceIOS sessionPathForSessionID:session_id
                                       directory:directory()];
  NSString* renamed_path = [session_path stringByAppendingPathExtension:@"bak"];
  ASSERT_NSNE(session_path, renamed_path);

  // Rename the file.
  ASSERT_TRUE([[NSFileManager defaultManager] moveItemAtPath:session_path
                                                      toPath:renamed_path
                                                       error:nil]);

  SessionIOS* session = [session_service() loadSessionFromPath:renamed_path];
  EXPECT_EQ(1u, session.sessionWindows.count);
  EXPECT_EQ(2u, session.sessionWindows[0].sessions.count);
  EXPECT_EQ(0u, session.sessionWindows[0].selectedIndex);
}

TEST_F(SessionServiceTest, LoadCorruptedSession) {
  NSString* session_path =
      SessionPathForTestData(FILE_PATH_LITERAL("corrupted.plist"));
  ASSERT_NSNE(nil, session_path);
  SessionIOS* session = [session_service() loadSessionFromPath:session_path];
  EXPECT_TRUE(session == nil);
}

// TODO(crbug.com/661633): remove this once M67 has shipped (i.e. once more
// than a year has passed since the introduction of the compatibility code).
TEST_F(SessionServiceTest, LoadM57Session) {
  NSString* session_path =
      SessionPathForTestData(FILE_PATH_LITERAL("session_m57.plist"));
  ASSERT_NSNE(nil, session_path);
  SessionIOS* session = [session_service() loadSessionFromPath:session_path];
  EXPECT_EQ(1u, session.sessionWindows.count);
}

// TODO(crbug.com/661633): remove this once M68 has shipped (i.e. once more
// than a year has passed since the introduction of the compatibility code).
TEST_F(SessionServiceTest, LoadM58Session) {
  NSString* session_path =
      SessionPathForTestData(FILE_PATH_LITERAL("session_m58.plist"));
  ASSERT_NSNE(nil, session_path);
  SessionIOS* session = [session_service() loadSessionFromPath:session_path];
  EXPECT_EQ(1u, session.sessionWindows.count);
}

}  // anonymous namespace

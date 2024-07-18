// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/path_service.h"
#import "base/run_loop.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/sessions/model/session_service_ios.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"
#import "ios/chrome/browser/sessions/model/session_window_ios_factory.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// The delay before saving.
constexpr base::TimeDelta kSaveDelay = base::Seconds(2.5);

// Fixture Class. Takes care of deleting the directory used to store test data.
class SessionServiceTest : public PlatformTest {
 public:
  SessionServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  }

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
        [[SessionServiceIOS alloc] initWithSaveDelay:kSaveDelay
                                          taskRunner:task_runner];
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
      auto web_state = std::make_unique<web::FakeWebState>();
      web_state->SetNavigationItemCount(1);
      web_state_list->InsertWebState(std::move(web_state),
                                     WebStateList::InsertionParams::AtIndex(i));
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

    return base::apple::FilePathToNSString(session_path);
  }

  SessionServiceIOS* session_service() { return session_service_; }

  const base::FilePath& directory() const { return directory_; }

  NSString* directory_as_nsstring() const {
    return base::apple::FilePathToNSString(directory());
  }

  void WaitForBackgroundTaskComplete() {
    base::RunLoop run_loop;
    [session_service_ shutdownWithClosure:run_loop.QuitClosure()];
    run_loop.Run();
  }

  void WaitForSessionSaveComplete() {
    task_environment_.FastForwardBy(kSaveDelay);
    WaitForBackgroundTaskComplete();
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
  SessionWindowIOSFactory* factory = [[SessionWindowIOSFactory alloc]
      initWithWebStateList:web_state_list.get()];

  NSString* session_id = [[NSUUID UUID] UUIDString];
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:YES];

  // Even if `immediately` is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  WaitForBackgroundTaskComplete();

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
  SessionWindowIOSFactory* factory = [[SessionWindowIOSFactory alloc]
      initWithWebStateList:web_state_list.get()];

  NSString* session_id = [[NSUUID UUID] UUIDString];
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:YES];

  // Even if `immediately` is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  WaitForBackgroundTaskComplete();

  NSFileManager* file_manager = [NSFileManager defaultManager];
  EXPECT_TRUE([file_manager removeItemAtPath:directory_as_nsstring()
                                       error:nullptr]);
}

TEST_F(SessionServiceTest, LoadSessionFromDirectoryNoFile) {
  NSString* session_id = [[NSUUID UUID] UUIDString];
  SessionWindowIOS* session =
      [session_service() loadSessionWithSessionID:session_id
                                        directory:directory()];
  EXPECT_TRUE(session == nil);
}

// Tests that the session service doesn't retain the SessionWindowIOSFactory,
// and that SaveSession will be no-op if the factory is destroyed earlier.
TEST_F(SessionServiceTest, SaveExpiredSession) {
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionWindowIOSFactory* factory = [[SessionWindowIOSFactory alloc]
      initWithWebStateList:web_state_list.get()];

  NSString* session_id = [[NSUUID UUID] UUIDString];
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:NO];
  [factory disconnect];
  factory = nil;

  // Make sure that the delay for saving a session has passed.
  WaitForSessionSaveComplete();

  SessionWindowIOS* session =
      [session_service() loadSessionWithSessionID:session_id
                                        directory:directory()];
  EXPECT_FALSE(session);
}

TEST_F(SessionServiceTest, LoadSessionFromDirectory) {
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionWindowIOSFactory* factory = [[SessionWindowIOSFactory alloc]
      initWithWebStateList:web_state_list.get()];

  NSString* session_id = [[NSUUID UUID] UUIDString];
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:YES];

  // Even if `immediately` is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  WaitForBackgroundTaskComplete();

  SessionWindowIOS* session_window =
      [session_service() loadSessionWithSessionID:session_id
                                        directory:directory()];
  EXPECT_EQ(2u, session_window.sessions.count);
  EXPECT_EQ(0u, session_window.selectedIndex);
}

TEST_F(SessionServiceTest, LoadSessionFromPath) {
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionWindowIOSFactory* factory = [[SessionWindowIOSFactory alloc]
      initWithWebStateList:web_state_list.get()];

  NSString* session_id = [[NSUUID UUID] UUIDString];
  [session_service() saveSession:factory
                       sessionID:session_id
                       directory:directory()
                     immediately:YES];

  // Even if `immediately` is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  WaitForBackgroundTaskComplete();

  NSString* session_path =
      [SessionServiceIOS sessionPathForSessionID:session_id
                                       directory:directory()];
  NSString* renamed_path = [session_path stringByAppendingPathExtension:@"bak"];
  ASSERT_NSNE(session_path, renamed_path);

  // Rename the file.
  ASSERT_TRUE([[NSFileManager defaultManager] moveItemAtPath:session_path
                                                      toPath:renamed_path
                                                       error:nil]);

  SessionWindowIOS* session_window =
      [session_service() loadSessionFromPath:renamed_path];
  EXPECT_EQ(2u, session_window.sessions.count);
  EXPECT_EQ(0u, session_window.selectedIndex);
}

TEST_F(SessionServiceTest, LoadCorruptedSession) {
  NSString* session_path =
      SessionPathForTestData(FILE_PATH_LITERAL("corrupted.plist"));
  ASSERT_NSNE(nil, session_path);
  SessionWindowIOS* session =
      [session_service() loadSessionFromPath:session_path];
  EXPECT_TRUE(session == nil);
}

// TODO(crbug.com/41284267): remove this once M67 has shipped (i.e. once more
// than a year has passed since the introduction of the compatibility code).
TEST_F(SessionServiceTest, LoadM57Session) {
  NSString* session_path =
      SessionPathForTestData(FILE_PATH_LITERAL("session_m57.plist"));
  ASSERT_NSNE(nil, session_path);
  SessionWindowIOS* session_window =
      [session_service() loadSessionFromPath:session_path];
  EXPECT_NSNE(nil, session_window);
}

// TODO(crbug.com/41284267): remove this once M68 has shipped (i.e. once more
// than a year has passed since the introduction of the compatibility code).
TEST_F(SessionServiceTest, LoadM58Session) {
  NSString* session_path =
      SessionPathForTestData(FILE_PATH_LITERAL("session_m58.plist"));
  ASSERT_NSNE(nil, session_path);
  SessionWindowIOS* session_window =
      [session_service() loadSessionFromPath:session_path];
  EXPECT_NSNE(nil, session_window);
}

}  // anonymous namespace

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ios/chrome/browser/chrome_paths.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_ios_factory.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Fixture Class. Takes care of deleting the directory used to store test data.
class SessionServiceTest : public PlatformTest {
 public:
  SessionServiceTest() = default;
  ~SessionServiceTest() override = default;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
    base::FilePath directory_name = scoped_temp_directory_.GetPath();
    directory_name = directory_name.Append(FILE_PATH_LITERAL("sessions"));
    directory_ = base::SysUTF8ToNSString(directory_name.AsUTF8Unsafe());

    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadTaskRunnerHandle::Get();
    session_service_ =
        [[SessionServiceIOS alloc] initWithTaskRunner:task_runner];
  }

  void TearDown() override {
    session_service_ = nil;
    PlatformTest::TearDown();
  }

  // Returns a WebStateList with |tabs_count| WebStates and activates the first
  // WebState.
  std::unique_ptr<WebStateList> CreateWebStateList(int tabs_count) {
    std::unique_ptr<WebStateList> web_state_list =
        std::make_unique<WebStateList>(&web_state_list_delegate_);
    for (int i = 0; i < tabs_count; ++i) {
      web_state_list->InsertWebState(i, std::make_unique<web::FakeWebState>(),
                                     WebStateList::INSERT_FORCE_INDEX,
                                     WebStateOpener());
    }
    if (tabs_count > 0)
      web_state_list->ActivateWebStateAt(0);
    return web_state_list;
  }

  // Returns the path to serialized SessionWindowIOS from a testdata file named
  // |filename| or nil if the file cannot be found.
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

  NSString* directory() { return directory_; }

 private:
  base::ScopedTempDir scoped_temp_directory_;
  base::test::TaskEnvironment task_environment_;
  SessionServiceIOS* session_service_;
  NSString* directory_;
  FakeWebStateListDelegate web_state_list_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SessionServiceTest);
};

TEST_F(SessionServiceTest, SessionPathForDirectory) {
  EXPECT_NSEQ(@"session.plist",
              [SessionServiceIOS sessionPathForDirectory:@""]);
}

TEST_F(SessionServiceTest, SaveSessionWindowToPath) {
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(0);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];
  [session_service() saveSession:factory directory:directory() immediately:YES];

  // Even if |immediately| is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();

  NSFileManager* file_manager = [NSFileManager defaultManager];
  EXPECT_TRUE([file_manager removeItemAtPath:directory() error:nullptr]);
}

TEST_F(SessionServiceTest, SaveSessionWindowToPathDirectoryExists) {
  ASSERT_TRUE([[NSFileManager defaultManager] createDirectoryAtPath:directory()
                                        withIntermediateDirectories:YES
                                                         attributes:nil
                                                              error:nullptr]);
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(0);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];

  [session_service() saveSession:factory directory:directory() immediately:YES];

  // Even if |immediately| is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();

  NSFileManager* file_manager = [NSFileManager defaultManager];
  EXPECT_TRUE([file_manager removeItemAtPath:directory() error:nullptr]);
}

TEST_F(SessionServiceTest, LoadSessionFromDirectoryNoFile) {
  SessionIOS* session =
      [session_service() loadSessionFromDirectory:directory()];
  EXPECT_TRUE(session == nil);
}

// Tests that the session service doesn't retain the SessionIOSFactory, and that
// savesession will be no-op if the factory is destroyed earlier.
TEST_F(SessionServiceTest, SaveExpiredSession) {
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];

  [session_service() saveSession:factory directory:directory() immediately:NO];
  [factory disconnect];
  factory = nil;
  // Make sure that the delay for saving a session has passed (at least 2.5
  // seconds)
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(2.5));
  base::RunLoop().RunUntilIdle();

  SessionIOS* session =
      [session_service() loadSessionFromDirectory:directory()];
  EXPECT_FALSE(session);
}

TEST_F(SessionServiceTest, LoadSessionFromDirectory) {
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];

  [session_service() saveSession:factory directory:directory() immediately:YES];

  // Even if |immediately| is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();

  SessionIOS* session =
      [session_service() loadSessionFromDirectory:directory()];
  EXPECT_EQ(1u, session.sessionWindows.count);
  EXPECT_EQ(2u, session.sessionWindows[0].sessions.count);
  EXPECT_EQ(0u, session.sessionWindows[0].selectedIndex);
}

TEST_F(SessionServiceTest, LoadSessionFromPath) {
  std::unique_ptr<WebStateList> web_state_list = CreateWebStateList(2);
  SessionIOSFactory* factory =
      [[SessionIOSFactory alloc] initWithWebStateList:web_state_list.get()];

  [session_service() saveSession:factory directory:directory() immediately:YES];

  // Even if |immediately| is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();

  NSString* session_path =
      [SessionServiceIOS sessionPathForDirectory:directory()];
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

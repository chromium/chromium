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
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ios/chrome/browser/chrome_paths.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/web/public/session/crw_session_storage.h"
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

  // Create a SessionIOSFactory creating a SessionIOS with |window_count|
  // windows each with |tab_count| tabs.
  SessionIOSFactory CreateSessionFactory(NSUInteger window_count,
                                         NSUInteger tab_count) {
    return ^{
      NSMutableArray<SessionWindowIOS*>* windows = [NSMutableArray array];
      while (windows.count < window_count) {
        NSMutableArray<CRWSessionStorage*>* tabs = [NSMutableArray array];
        while (tabs.count < tab_count) {
          [tabs addObject:[[CRWSessionStorage alloc] init]];
        }
        [windows addObject:[[SessionWindowIOS alloc]
                               initWithSessions:[tabs copy]
                                  selectedIndex:(tabs.count ? tabs.count - 1
                                                            : NSNotFound)]];
      }
      return [[SessionIOS alloc] initWithWindows:[windows copy]];
    };
  }

  SessionServiceIOS* session_service() { return session_service_; }

  NSString* directory() { return directory_; }

 private:
  base::ScopedTempDir scoped_temp_directory_;
  base::test::TaskEnvironment task_environment_;
  SessionServiceIOS* session_service_;
  NSString* directory_;

  DISALLOW_COPY_AND_ASSIGN(SessionServiceTest);
};

TEST_F(SessionServiceTest, SessionPathForDirectory) {
  EXPECT_NSEQ(@"session.plist",
              [SessionServiceIOS sessionPathForDirectory:@""]);
}

TEST_F(SessionServiceTest, SaveSessionWindowToPath) {
  [session_service() saveSession:CreateSessionFactory(0u, 0u)
                       directory:directory()
                     immediately:YES];

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

  [session_service() saveSession:CreateSessionFactory(0u, 0u)
                       directory:directory()
                     immediately:YES];

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

TEST_F(SessionServiceTest, LoadSessionFromDirectory) {
  [session_service() saveSession:CreateSessionFactory(2u, 1u)
                       directory:directory()
                     immediately:YES];

  // Even if |immediately| is YES, the file is created by a task on the task
  // runner passed to SessionServiceIOS initializer (which is the current
  // thread task runner during test). Wait for the task to complete.
  base::RunLoop().RunUntilIdle();

  SessionIOS* session =
      [session_service() loadSessionFromDirectory:directory()];
  EXPECT_EQ(2u, session.sessionWindows.count);
  for (SessionWindowIOS* sessionWindow in session.sessionWindows) {
    EXPECT_EQ(1u, sessionWindow.sessions.count);
    EXPECT_EQ(0u, sessionWindow.selectedIndex);
  }
}

TEST_F(SessionServiceTest, LoadSessionFromPath) {
  [session_service() saveSession:CreateSessionFactory(2u, 1u)
                       directory:directory()
                     immediately:YES];

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
  EXPECT_EQ(2u, session.sessionWindows.count);
  for (SessionWindowIOS* sessionWindow in session.sessionWindows) {
    EXPECT_EQ(1u, sessionWindow.sessions.count);
    EXPECT_EQ(0u, sessionWindow.selectedIndex);
  }
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

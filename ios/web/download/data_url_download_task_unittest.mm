// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/data_url_download_task.h"

#import "base/files/file_path.h"
#import "base/notreached.h"
#import "base/run_loop.h"
#import "base/scoped_observation.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

const char kContentDisposition[] = "attachment; filename=file.test";
const char kMimeType[] = "application/pdf";

// Fake DownloadTaskImpl::Delegate used for tests.
class DataUrlDownloadTaskTestDelegate : public DownloadTaskImpl::Delegate {
 public:
  DataUrlDownloadTaskTestDelegate() {}

  void OnTaskDestroyed(DownloadTaskImpl* task) override {}

  // Returns mock, which can be accessed via session() method.
  NSURLSession* CreateSession(NSString* identifier,
                              NSArray<NSHTTPCookie*>* cookies,
                              id<NSURLSessionDataDelegate> delegate,
                              NSOperationQueue* delegate_queue) override {
    NOTREACHED();
    return nil;
  }
};

// Fake DownloadTaskObserver used to wait for task completion.
class DataUrlDownloadTaskTestObserver : public DownloadTaskObserver {
 public:
  DataUrlDownloadTaskTestObserver(DownloadTask* task,
                                  base::OnceClosure done_closure)
      : done_closure_(std::move(done_closure)) {
    scoped_observation_.Observe(task);
  }

  void OnDownloadUpdated(DownloadTask* task) override {
    DCHECK(scoped_observation_.IsObservingSource(task));
    if (task->IsDone()) {
      scoped_observation_.Reset();
      std::move(done_closure_).Run();
    }
  }

  void OnDownloadDestroyed(DownloadTask* task) override {
    DCHECK(scoped_observation_.IsObservingSource(task));
    scoped_observation_.Reset();
    std::move(done_closure_).Run();
  }

 private:
  base::ScopedObservation<DownloadTask, DownloadTaskObserver>
      scoped_observation_{this};
  base::OnceClosure done_closure_;
};

// Starts the task and waits until `task` is in done state.
void StartTaskAndWaitUntilDone(DownloadTaskImpl* task) {
  base::RunLoop run_loop;
  DataUrlDownloadTaskTestObserver observer(task, run_loop.QuitClosure());

  task->Start(base::FilePath(), web::DownloadTask::Destination::kToMemory);
  run_loop.Run();
}

}  //  namespace

// Test fixture for testing DownloadTaskImplTest class.
class DataUrlDownloadTaskTest : public PlatformTest {
 protected:
  DataUrlDownloadTaskTest() {
    browser_state_.SetOffTheRecord(true);
    web_state_.SetBrowserState(&browser_state_);
  }

  web::WebTaskEnvironment task_environment_;
  FakeBrowserState browser_state_;
  FakeWebState web_state_;
  DataUrlDownloadTaskTestDelegate task_delegate_;
};

// Tests valid data:// url downloads.
TEST_F(DataUrlDownloadTaskTest, ValidDataUrl) {
  // Create data:// url download task.
  char kDataUrl[] = "data:text/plain;base64,Q2hyb21pdW0=";
  DataUrlDownloadTask task(&web_state_, GURL(kDataUrl), @"GET",
                           kContentDisposition,
                           /*total_bytes=*/-1, kMimeType,
                           [[NSUUID UUID] UUIDString], &task_delegate_);

  StartTaskAndWaitUntilDone(&task);

  // Verify the state of downloaded task.
  const char kTestData[] = "Chromium";
  EXPECT_EQ(DownloadTask::State::kComplete, task.GetState());
  EXPECT_EQ(0, task.GetErrorCode());
  EXPECT_EQ(strlen(kTestData), static_cast<size_t>(task.GetTotalBytes()));
  EXPECT_EQ(strlen(kTestData), static_cast<size_t>(task.GetReceivedBytes()));
  EXPECT_EQ(100, task.GetPercentComplete());
  EXPECT_EQ("text/plain", task.GetMimeType());
  EXPECT_NSEQ(@(kTestData),
              [[NSString alloc] initWithData:task.GetResponseData()
                                    encoding:NSUTF8StringEncoding]);
}

// Tests empty data:// url downloads.
TEST_F(DataUrlDownloadTaskTest, EmptyDataUrl) {
  // Create data:// url download task.
  char kDataUrl[] = "data://";
  DataUrlDownloadTask task(&web_state_, GURL(kDataUrl), @"GET",
                           kContentDisposition,
                           /*total_bytes=*/-1, kMimeType,
                           [[NSUUID UUID] UUIDString], &task_delegate_);

  StartTaskAndWaitUntilDone(&task);

  // Verify the state of downloaded task.
  EXPECT_EQ(DownloadTask::State::kFailed, task.GetState());
  EXPECT_EQ(net::ERR_INVALID_URL, task.GetErrorCode());
  EXPECT_EQ(-1, task.GetTotalBytes());
  EXPECT_EQ(0, task.GetReceivedBytes());
  EXPECT_EQ(0, task.GetPercentComplete());
  EXPECT_EQ("", task.GetMimeType());
  EXPECT_NSEQ(@"", [[NSString alloc] initWithData:task.GetResponseData()
                                         encoding:NSUTF8StringEncoding]);
}

}  // namespace web

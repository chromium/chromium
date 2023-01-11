// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/simple_thread.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/ppb_message_loop_proxy.h"
#include "ppapi/shared_impl/callback_tracker.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/test_globals.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

// Note, this file tests TrackedCallback which lives in ppapi/shared_impl.
// Unfortunately, we need the test to live in ppapi/proxy so that it can use
// the thread support there.
namespace ppapi {
namespace proxy {

namespace {

class CallbackThread : public base::SimpleThread {
 public:
  explicit CallbackThread(PP_Instance instance)
      : SimpleThread("CallbackThread"), instance_(instance) {}
  ~CallbackThread() override {}

  // base::SimpleThread overrides.
  void BeforeStart() override {
    ProxyAutoLock acquire;
    // Create the message loop here, after PpapiGlobals has been created.
    message_loop_ = new MessageLoopResource(instance_);
  }
  void BeforeJoin() override {
    ProxyAutoLock acquire;
    message_loop()->PostQuit(PP_TRUE);
    message_loop_ = nullptr;
  }
  void Run() override {
    ProxyAutoLock acquire;
    // Make a local copy of message_loop_ for this thread so we can interact
    // with it even after the main thread releases it.
    scoped_refptr<MessageLoopResource> message_loop(message_loop_);
    message_loop->AttachToCurrentThread();
    // Note, run releases the lock to run events.
    base::RunLoop().Run();
    message_loop->DetachFromThread();
  }

  MessageLoopResource* message_loop() { return message_loop_.get(); }

 private:
  PP_Instance instance_;
  scoped_refptr<MessageLoopResource> message_loop_;
};

class TrackedCallbackTest : public PluginProxyTest {
 public:
  TrackedCallbackTest() : thread_(pp_instance()) {}
  CallbackThread& thread() { return thread_; }

 private:
  // PluginProxyTest overrides.
  void SetUp() override {
    PluginProxyTest::SetUp();
    thread_.Start();
  }
  void TearDown() override {
    thread_.Join();
    PluginProxyTest::TearDown();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  CallbackThread thread_;
};

// All valid results (PP_OK, PP_ERROR_...) are nonpositive.
const int32_t kInitializedResultValue = 1;
const int32_t kOverrideResultValue = 2;

struct CallbackRunInfo {
  explicit CallbackRunInfo(base::ThreadChecker* thread_checker)
      : run_count_(0),
        result_(kInitializedResultValue),
        completion_task_run_count_(0),
        completion_task_result_(kInitializedResultValue),
        thread_checker_(thread_checker),
        callback_did_run_event_(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  void CallbackDidRun(int32_t result) {
    CHECK(thread_checker_->CalledOnValidThread());
    if (!run_count_)
      result_ = result;
    ++run_count_;
    callback_did_run_event_.Signal();
  }
  void CompletionTaskDidRun(int32_t result) {
    CHECK(thread_checker_->CalledOnValidThread());
    if (!completion_task_run_count_)
      completion_task_result_ = result;
    ++completion_task_run_count_;
  }
  void WaitUntilCompleted() { callback_did_run_event_.Wait(); }
  unsigned run_count() { return run_count_; }
  int32_t result() { return result_; }
  unsigned completion_task_run_count() { return completion_task_run_count_; }
  int32_t completion_task_result() { return completion_task_result_; }
 private:
  unsigned run_count_;
  int32_t result_;
  unsigned completion_task_run_count_;
  int32_t completion_task_result_;
  // Weak; owned by the creator of CallbackRunInfo.
  base::ThreadChecker* thread_checker_;

  base::WaitableEvent callback_did_run_event_;
};

void TestCallback(void* user_data, int32_t result) {
  CallbackRunInfo* info = static_cast<CallbackRunInfo*>(user_data);
  info->CallbackDidRun(result);
}

// CallbackShutdownTest --------------------------------------------------------

class CallbackShutdownTest : public TrackedCallbackTest {
 public:
  CallbackShutdownTest() : info_did_run_(&thread_checker_),
                           info_did_abort_(&thread_checker_),
                           info_didnt_run_(&thread_checker_) {}

  // Cases:
  // (1) A callback which is run (so shouldn't be aborted on shutdown).
  // (2) A callback which is aborted (so shouldn't be aborted on shutdown).
  // (3) A callback which isn't run (so should be aborted on shutdown).
  CallbackRunInfo& info_did_run() { return info_did_run_; }      // (1)
  CallbackRunInfo& info_did_abort() { return info_did_abort_; }  // (2)
  CallbackRunInfo& info_didnt_run() { return info_didnt_run_; }  // (3)

 private:
  base::ThreadChecker thread_checker_;
  CallbackRunInfo info_did_run_;
  CallbackRunInfo info_did_abort_;
  CallbackRunInfo info_didnt_run_;
};

}  // namespace

// Tests that callbacks are properly aborted on module shutdown.
TEST_F(CallbackShutdownTest, DISABLED_AbortOnShutdown) {
  ProxyAutoLock lock;
  scoped_refptr<Resource> resource(
      new Resource(OBJECT_IS_PROXY, pp_instance()));

  // Set up case (1) (see above).
  EXPECT_EQ(0U, info_did_run().run_count());
  // TODO(dmichael): Test this on a background thread?
  scoped_refptr<TrackedCallback> callback_did_run = new TrackedCallback(
      resource.get(),
      PP_MakeCompletionCallback(&TestCallback, &info_did_run()));
  EXPECT_EQ(0U, info_did_run().run_count());
  callback_did_run->Run(PP_OK);
  EXPECT_EQ(1U, info_did_run().run_count());
  EXPECT_EQ(PP_OK, info_did_run().result());

  // Set up case (2).
  EXPECT_EQ(0U, info_did_abort().run_count());
  scoped_refptr<TrackedCallback> callback_did_abort = new TrackedCallback(
      resource.get(),
      PP_MakeCompletionCallback(&TestCallback, &info_did_abort()));
  EXPECT_EQ(0U, info_did_abort().run_count());
  callback_did_abort->Abort();
  EXPECT_EQ(1U, info_did_abort().run_count());
  EXPECT_EQ(PP_ERROR_ABORTED, info_did_abort().result());

  // Set up case (3).
  EXPECT_EQ(0U, info_didnt_run().run_count());
  scoped_refptr<TrackedCallback> callback_didnt_run = new TrackedCallback(
      resource.get(),
      PP_MakeCompletionCallback(&TestCallback, &info_didnt_run()));
  EXPECT_EQ(0U, info_didnt_run().run_count());

  GetGlobals()->GetCallbackTrackerForInstance(pp_instance())->AbortAll();

  // Check case (1).
  EXPECT_EQ(1U, info_did_run().run_count());

  // Check case (2).
  EXPECT_EQ(1U, info_did_abort().run_count());

  // Check case (3).
  EXPECT_EQ(1U, info_didnt_run().run_count());
  EXPECT_EQ(PP_ERROR_ABORTED, info_didnt_run().result());
}

// CallbackResourceTest --------------------------------------------------------

namespace {

class CallbackResourceTest : public TrackedCallbackTest {
 public:
  CallbackResourceTest() {}
};

class CallbackMockResource : public Resource {
 public:
  static scoped_refptr<CallbackMockResource> Create(PP_Instance instance) {
    ProxyAutoLock acquire;
    return scoped_refptr<CallbackMockResource>(
        new CallbackMockResource(instance));
  }
  ~CallbackMockResource() {}

  // Take a reference to this resource, which will add it to the tracker.
  void TakeRef() {
    ProxyAutoLock acquire;
    ScopedPPResource temp_resource(ScopedPPResource::PassRef(), GetReference());
    EXPECT_NE(0, temp_resource.get());
    reference_holder_ = temp_resource;
  }
  // Release it, removing it from the tracker.
  void ReleaseRef() {
    ProxyAutoLock acquire;
    reference_holder_ = 0;
  }

  // Create the test callbacks on a background thread, so that we can verify
  // they are run on the same thread where they were created.
  void CreateCallbacksOnLoop(MessageLoopResource* loop_resource) {
    ProxyAutoLock acquire;
    // |thread_checker_| will bind to the background thread.
    thread_checker_.DetachFromThread();
    loop_resource->task_runner()->PostTask(
        FROM_HERE, RunWhileLocked(base::BindOnce(
                       &CallbackMockResource::CreateCallbacks, this)));
  }

  int32_t CompletionTask(CallbackRunInfo* info, int32_t result) {
    // The completion task must run on the thread where the callback was
    // created, and must hold the proxy lock.
    CHECK(thread_checker_.CalledOnValidThread());
    ProxyLock::AssertAcquired();

    // We should run before the callback.
    CHECK_EQ(0U, info->run_count());
    info->CompletionTaskDidRun(result);
    return kOverrideResultValue;
  }

  void CheckInitialState() {
    callbacks_created_event_.Wait();
    EXPECT_EQ(0U, info_did_run_.run_count());
    EXPECT_EQ(0U, info_did_run_.completion_task_run_count());

    EXPECT_EQ(0U, info_did_run_with_completion_task_.run_count());
    EXPECT_EQ(0U,
              info_did_run_with_completion_task_.completion_task_run_count());

    EXPECT_EQ(0U, info_did_abort_.run_count());
    EXPECT_EQ(0U, info_did_abort_.completion_task_run_count());

    EXPECT_EQ(0U, info_didnt_run_.run_count());
    EXPECT_EQ(0U, info_didnt_run_.completion_task_run_count());
  }

  void RunCallbacks() {
    callback_did_run_->Run(PP_OK);
    callback_did_run_with_completion_task_->Run(PP_OK);
    callback_did_abort_->Abort();
    info_did_run_.WaitUntilCompleted();
    info_did_run_with_completion_task_.WaitUntilCompleted();
    info_did_abort_.WaitUntilCompleted();
  }

  void CheckIntermediateState() {
    EXPECT_EQ(1U, info_did_run_.run_count());
    EXPECT_EQ(PP_OK, info_did_run_.result());
    EXPECT_EQ(0U, info_did_run_.completion_task_run_count());

    EXPECT_EQ(1U, info_did_run_with_completion_task_.run_count());
    // completion task should override the result.
    EXPECT_EQ(kOverrideResultValue,
              info_did_run_with_completion_task_.result());
    EXPECT_EQ(1U,
              info_did_run_with_completion_task_.completion_task_run_count());
    EXPECT_EQ(PP_OK,
              info_did_run_with_completion_task_.completion_task_result());

    EXPECT_EQ(1U, info_did_abort_.run_count());
    // completion task shouldn't override an abort.
    EXPECT_EQ(PP_ERROR_ABORTED, info_did_abort_.result());
    EXPECT_EQ(1U, info_did_abort_.completion_task_run_count());
    EXPECT_EQ(PP_ERROR_ABORTED, info_did_abort_.completion_task_result());

    EXPECT_EQ(0U, info_didnt_run_.completion_task_run_count());
    EXPECT_EQ(0U, info_didnt_run_.run_count());
  }

  void CheckFinalState() {
    info_didnt_run_.WaitUntilCompleted();
    EXPECT_EQ(1U, info_did_run_with_completion_task_.run_count());
    EXPECT_EQ(kOverrideResultValue,
              info_did_run_with_completion_task_.result());
    callback_did_run_with_completion_task_ = nullptr;
    EXPECT_EQ(1U, info_did_run_.run_count());
    EXPECT_EQ(PP_OK, info_did_run_.result());
    callback_did_run_ = nullptr;
    EXPECT_EQ(1U, info_did_abort_.run_count());
    EXPECT_EQ(PP_ERROR_ABORTED, info_did_abort_.result());
    callback_did_abort_ = nullptr;
    EXPECT_EQ(1U, info_didnt_run_.run_count());
    EXPECT_EQ(PP_ERROR_ABORTED, info_didnt_run_.result());
    callback_didnt_run_ = nullptr;
  }

 private:
  explicit CallbackMockResource(PP_Instance instance)
      : Resource(OBJECT_IS_PROXY, instance),
        info_did_run_(&thread_checker_),
        info_did_run_with_completion_task_(&thread_checker_),
        info_did_abort_(&thread_checker_),
        info_didnt_run_(&thread_checker_),
        callbacks_created_event_(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  void CreateCallbacks() {
    // Bind thread_checker_ to the thread where we create the callbacks.
    // Later, when the callback runs, it will check that it was invoked on this
    // same thread.
    CHECK(thread_checker_.CalledOnValidThread());

    callback_did_run_ = new TrackedCallback(
        this, PP_MakeCompletionCallback(&TestCallback, &info_did_run_));

    // In order to test that the completion task can override the callback
    // result, we need to test callbacks with and without a completion task.
    callback_did_run_with_completion_task_ = new TrackedCallback(
        this,
        PP_MakeCompletionCallback(&TestCallback,
                                  &info_did_run_with_completion_task_));
    callback_did_run_with_completion_task_->set_completion_task(
        base::BindOnce(&CallbackMockResource::CompletionTask, this,
                       &info_did_run_with_completion_task_));

    callback_did_abort_ = new TrackedCallback(
        this, PP_MakeCompletionCallback(&TestCallback, &info_did_abort_));
    callback_did_abort_->set_completion_task(base::BindOnce(
        &CallbackMockResource::CompletionTask, this, &info_did_abort_));

    callback_didnt_run_ = new TrackedCallback(
        this, PP_MakeCompletionCallback(&TestCallback, &info_didnt_run_));
    callback_didnt_run_->set_completion_task(base::BindOnce(
        &CallbackMockResource::CompletionTask, this, &info_didnt_run_));

    callbacks_created_event_.Signal();
  }

  // Used to verify that the callback runs on the same thread where it is
  // created.
  base::ThreadChecker thread_checker_;

  scoped_refptr<TrackedCallback> callback_did_run_;
  CallbackRunInfo info_did_run_;

  scoped_refptr<TrackedCallback> callback_did_run_with_completion_task_;
  CallbackRunInfo info_did_run_with_completion_task_;

  scoped_refptr<TrackedCallback> callback_did_abort_;
  CallbackRunInfo info_did_abort_;

  scoped_refptr<TrackedCallback> callback_didnt_run_;
  CallbackRunInfo info_didnt_run_;

  base::WaitableEvent callbacks_created_event_;

  ScopedPPResource reference_holder_;
};

}  // namespace

// Test that callbacks get aborted on the last resource unref.
TEST_F(CallbackResourceTest, DISABLED_AbortOnNoRef) {
  // Test several things: Unref-ing a resource (to zero refs) with callbacks
  // which (1) have been run, (2) have been aborted, (3) haven't been completed.
  // Check that the uncompleted one gets aborted, and that the others don't get
  // called again.
  scoped_refptr<CallbackMockResource> resource_1(
      CallbackMockResource::Create(pp_instance()));
  resource_1->CreateCallbacksOnLoop(thread().message_loop());
  resource_1->CheckInitialState();
  resource_1->RunCallbacks();
  resource_1->TakeRef();
  resource_1->CheckIntermediateState();

  // Also do the same for a second resource, and make sure that unref-ing the
  // first resource doesn't much up the second resource.
  scoped_refptr<CallbackMockResource> resource_2(
      CallbackMockResource::Create(pp_instance()));
  resource_2->CreateCallbacksOnLoop(thread().message_loop());
  resource_2->CheckInitialState();
  resource_2->RunCallbacks();
  resource_2->TakeRef();
  resource_2->CheckIntermediateState();

  // Double-check that resource #1 is still okay.
  resource_1->CheckIntermediateState();

  // Kill resource #1, spin the message loop to run posted calls, and check that
  // things are in the expected states.
  resource_1->ReleaseRef();

  resource_1->CheckFinalState();
  resource_2->CheckIntermediateState();

  // Kill resource #2.
  resource_2->ReleaseRef();

  resource_1->CheckFinalState();
  resource_2->CheckFinalState();

  {
    ProxyAutoLock lock;
    resource_1 = nullptr;
    resource_2 = nullptr;
  }
}

// Test that "resurrecting" a resource (getting a new ID for a |Resource|)
// doesn't resurrect callbacks.
TEST_F(CallbackResourceTest, DISABLED_Resurrection) {
  scoped_refptr<CallbackMockResource> resource(
      CallbackMockResource::Create(pp_instance()));
  resource->CreateCallbacksOnLoop(thread().message_loop());
  resource->CheckInitialState();
  resource->RunCallbacks();
  resource->TakeRef();
  resource->CheckIntermediateState();

  // Unref it and check that things are in the expected states.
  resource->ReleaseRef();
  resource->CheckFinalState();

  // "Resurrect" it and check that the callbacks are still dead.
  resource->TakeRef();
  resource->CheckFinalState();

  // Unref it again and do the same.
  resource->ReleaseRef();
  resource->CheckFinalState();
  {
    ProxyAutoLock lock;
    resource = nullptr;
  }
}

}  // namespace proxy
}  // namespace ppapi

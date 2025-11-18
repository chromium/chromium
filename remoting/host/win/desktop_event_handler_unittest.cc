// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/desktop_event_handler.h"

#include <windows.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_local.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class DesktopEventHandlerTest : public testing::Test {
 public:
  DesktopEventHandlerTest() = default;
  ~DesktopEventHandlerTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

// Struct to run `quit_closure` once the thread local storage is deleted, i.e.,
// the worker thread is deleted.
struct RunOnDeletionTls {
  static void InstallRunOnDeletionTls(base::OnceClosure quit_closure) {
    static base::NoDestructor<base::ThreadLocalOwnedPointer<RunOnDeletionTls>>
        tls_no_destructor;
    auto& tls_pointer = *tls_no_destructor;
    tls_pointer.Set(
        std::make_unique<RunOnDeletionTls>(std::move(quit_closure)));
  }

  explicit RunOnDeletionTls(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}
  ~RunOnDeletionTls() {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  base::OnceClosure quit_closure_;
};

class TestDelegate : public DesktopEventHandler::Delegate {
 public:
  explicit TestDelegate(base::OnceClosure on_thread_started,
                        base::OnceClosure on_thread_stopped)
      : on_thread_started_(std::move(on_thread_started)),
        on_thread_stopped_(std::move(on_thread_stopped)) {}
  ~TestDelegate() override = default;

  void OnWorkerThreadStarted() override {
    RunOnDeletionTls::InstallRunOnDeletionTls(std::move(on_thread_stopped_));
    if (on_thread_started_) {
      std::move(on_thread_started_).Run();
    }
  }

  void OnEvent(DWORD event, LONG object_id) override {}

 private:
  base::OnceClosure on_thread_started_;
  base::OnceClosure on_thread_stopped_;
};

}  // namespace

TEST_F(DesktopEventHandlerTest, DeleteThreadAfterDestruction) {
  base::RunLoop thread_started_run_loop;
  base::RunLoop thread_stopped_run_loop;
  auto handler = std::make_unique<DesktopEventHandler>();
  handler->Start(
      EVENT_MIN, EVENT_MAX,
      std::make_unique<TestDelegate>(thread_started_run_loop.QuitClosure(),
                                     thread_stopped_run_loop.QuitClosure()));

  // Wait for the worker thread to start.
  thread_started_run_loop.Run();

  // Destroy the handler, which should trigger thread cleanup.
  handler.reset();

  // Verify that the thread has stopped.
  thread_stopped_run_loop.Run();
}

TEST_F(DesktopEventHandlerTest, TwoHandlers) {
  base::RunLoop thread_started_run_loop_1;
  base::RunLoop thread_stopped_run_loop_1;
  auto handler_1 = std::make_unique<DesktopEventHandler>();
  handler_1->Start(
      EVENT_MIN, EVENT_MAX,
      std::make_unique<TestDelegate>(thread_started_run_loop_1.QuitClosure(),
                                     thread_stopped_run_loop_1.QuitClosure()));

  base::RunLoop thread_started_run_loop_2;
  base::RunLoop thread_stopped_run_loop_2;
  auto handler_2 = std::make_unique<DesktopEventHandler>();
  handler_2->Start(
      EVENT_MIN, EVENT_MAX,
      std::make_unique<TestDelegate>(thread_started_run_loop_2.QuitClosure(),
                                     thread_stopped_run_loop_2.QuitClosure()));

  thread_started_run_loop_1.Run();
  thread_started_run_loop_2.Run();

  handler_1.reset();
  handler_2.reset();

  thread_stopped_run_loop_1.Run();
  thread_stopped_run_loop_2.Run();
}

}  // namespace remoting

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/browser/utility_process_host.h"
#include "content/browser/utility_process_host_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/bind_interface_helpers.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_service.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {
namespace {

const std::string kTestMessage = "My hovercraft is full of eels!";

class MojoSandboxTest : public ContentBrowserTest {
 public:
  MojoSandboxTest() {}

  void SetUpOnMainThread() override {
    base::RunLoop run_loop;
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&MojoSandboxTest::StartUtilityProcessOnIoThread,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void TearDownOnMainThread() override {
    base::RunLoop run_loop;
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&MojoSandboxTest::StopUtilityProcessOnIoThread,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  std::unique_ptr<UtilityProcessHost> host_;

 private:
  void StartUtilityProcessOnIoThread() {
    host_.reset(new UtilityProcessHost(nullptr, nullptr));
    host_->SetMetricsName("mojo_sandbox_test_process");
    ASSERT_TRUE(host_->Start());
  }

  void StopUtilityProcessOnIoThread() { host_.reset(); }

  DISALLOW_COPY_AND_ASSIGN(MojoSandboxTest);
};

IN_PROC_BROWSER_TEST_F(MojoSandboxTest, SubprocessSharedBuffer) {
  // Ensures that a shared buffer can be created within a sandboxed process.

  mojom::TestServicePtr test_service;
  BindInterface(host_.get(), &test_service);

  bool got_response = false;
  base::RunLoop run_loop;
  test_service.set_connection_error_handler(run_loop.QuitClosure());
  test_service->CreateSharedBuffer(
      kTestMessage,
      base::BindOnce(
          [](const base::Closure& quit_closure, bool* got_response,
             mojo::ScopedSharedBufferHandle buffer) {
            ASSERT_TRUE(buffer.is_valid());
            mojo::ScopedSharedBufferMapping mapping =
                buffer->Map(kTestMessage.size());
            ASSERT_TRUE(mapping);
            std::string contents(static_cast<const char*>(mapping.get()),
                                 kTestMessage.size());
            EXPECT_EQ(kTestMessage, contents);
            *got_response = true;
            quit_closure.Run();
          },
          run_loop.QuitClosure(), &got_response));
  run_loop.Run();
  EXPECT_TRUE(got_response);
}

}  //  namespace
}  //  namespace content

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/visual_debugger_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/system/sys_info.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"

namespace content {

namespace protocol {

namespace {

GpuProcessHost* GetGpuProcessHost(bool force_create) {
  return GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED, force_create);
}

base::DictValue MakeFilterParams() {
  base::DictValue selector;
  selector.Set("anno", "");

  base::DictValue filter;
  filter.Set("selector", std::move(selector));
  filter.Set("active", true);

  base::ListValue filters;
  filters.Append(std::move(filter));

  base::DictValue filter_params;
  filter_params.Set("filters", std::move(filters));

  base::DictValue params;
  params.Set("filter", std::move(filter_params));
  return params;
}

}  // namespace

class VisualDebuggerHandlerTest : public DevToolsProtocolTest {
 public:
  void SetUpOnMainThread() override {
    DevToolsProtocolTest::SetUpOnMainThread();
    set_agent_host_can_close();
    NavigateToURLBlockUntilNavigationsComplete(
        shell(), GURL("data:text/html,<body></body>"), 1);
    Attach();
  }

 protected:
  VisualDebuggerHandler* handler() {
    auto* agent_host = static_cast<DevToolsAgentHostImpl*>(agent_host_.get());
    std::vector<VisualDebuggerHandler*> handlers =
        agent_host->HandlersByName<VisualDebuggerHandler>(
            VisualDebugger::Metainfo::domainName);
    CHECK_EQ(1u, handlers.size());
    return handlers[0];
  }

  bool enabled() { return handler()->enabled_; }

  void UseMockGpuProcessHostGetter() {
    handler()->gpu_process_host_getter_for_testing_ =
        gpu_process_host_getter_.Get();
  }

  void ExpectServerError() {
    ASSERT_TRUE(error());
    EXPECT_THAT(
        error()->FindInt("code"),
        testing::Optional(static_cast<int>(crdtp::DispatchCode::SERVER_ERROR)));
  }

  base::MockRepeatingCallback<GpuProcessHost*(bool)> gpu_process_host_getter_;
};

IN_PROC_BROWSER_TEST_F(VisualDebuggerHandlerTest, VisualDebuggerTest) {
  ASSERT_TRUE(SendCommandSync("VisualDebugger.startStream"));
  WaitForNotification("VisualDebugger.frameResponse", true);
  ASSERT_TRUE(
      SendCommandSync("VisualDebugger.filterStream", MakeFilterParams()));
  ASSERT_TRUE(SendCommandSync("VisualDebugger.stopStream"));
  EXPECT_FALSE(enabled());
}

IN_PROC_BROWSER_TEST_F(VisualDebuggerHandlerTest,
                       StartStreamFailsWhenGpuProcessIsUnavailable) {
  UseMockGpuProcessHostGetter();
  EXPECT_CALL(gpu_process_host_getter_, Run(true))
      .WillOnce(testing::Return(nullptr));

  ASSERT_FALSE(SendCommandSync("VisualDebugger.startStream"));
  ExpectServerError();
  EXPECT_FALSE(enabled());

  testing::Mock::VerifyAndClearExpectations(&gpu_process_host_getter_);
  EXPECT_CALL(gpu_process_host_getter_, Run(testing::_)).Times(0);
  ASSERT_TRUE(SendCommandSync("VisualDebugger.stopStream"));
  Detach();
}

IN_PROC_BROWSER_TEST_F(VisualDebuggerHandlerTest,
                       StartStreamCanRetryAfterGpuProcessFailure) {
  UseMockGpuProcessHostGetter();
  testing::InSequence sequence;
  EXPECT_CALL(gpu_process_host_getter_, Run(true))
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(gpu_process_host_getter_, Run(true)).WillOnce(GetGpuProcessHost);
  EXPECT_CALL(gpu_process_host_getter_, Run(false))
      .WillOnce(testing::Return(nullptr));

  ASSERT_FALSE(SendCommandSync("VisualDebugger.startStream"));
  ExpectServerError();
  EXPECT_FALSE(enabled());

  ASSERT_TRUE(SendCommandSync("VisualDebugger.startStream"));
  EXPECT_TRUE(enabled());

  ASSERT_TRUE(SendCommandSync("VisualDebugger.stopStream"));
  EXPECT_FALSE(enabled());
}

IN_PROC_BROWSER_TEST_F(VisualDebuggerHandlerTest,
                       FailedRestartClearsEnabledState) {
  UseMockGpuProcessHostGetter();
  EXPECT_CALL(gpu_process_host_getter_, Run(true))
      .WillOnce(GetGpuProcessHost)
      .WillOnce(testing::Return(nullptr));

  ASSERT_TRUE(SendCommandSync("VisualDebugger.startStream"));
  EXPECT_TRUE(enabled());
  ASSERT_FALSE(SendCommandSync("VisualDebugger.startStream"));
  ExpectServerError();
  EXPECT_FALSE(enabled());

  testing::Mock::VerifyAndClearExpectations(&gpu_process_host_getter_);
  EXPECT_CALL(gpu_process_host_getter_, Run(testing::_)).Times(0);
  ASSERT_TRUE(SendCommandSync("VisualDebugger.stopStream"));
  Detach();
}

IN_PROC_BROWSER_TEST_F(VisualDebuggerHandlerTest,
                       FilterStreamFailsWhenGpuProcessIsUnavailable) {
  UseMockGpuProcessHostGetter();
  EXPECT_CALL(gpu_process_host_getter_, Run(true))
      .WillOnce(testing::Return(nullptr));

  ASSERT_FALSE(
      SendCommandSync("VisualDebugger.filterStream", MakeFilterParams()));
  ExpectServerError();
  EXPECT_FALSE(enabled());

  testing::Mock::VerifyAndClearExpectations(&gpu_process_host_getter_);
  EXPECT_CALL(gpu_process_host_getter_, Run(testing::_)).Times(0);
  Detach();
}

IN_PROC_BROWSER_TEST_F(VisualDebuggerHandlerTest,
                       StopStreamSucceedsWhenGpuProcessIsUnavailable) {
  UseMockGpuProcessHostGetter();
  testing::InSequence sequence;
  EXPECT_CALL(gpu_process_host_getter_, Run(true)).WillOnce(GetGpuProcessHost);
  EXPECT_CALL(gpu_process_host_getter_, Run(false))
      .WillOnce(testing::Return(nullptr));

  ASSERT_TRUE(SendCommandSync("VisualDebugger.startStream"));
  EXPECT_TRUE(enabled());

  ASSERT_TRUE(SendCommandSync("VisualDebugger.stopStream"));
  EXPECT_FALSE(enabled());

  testing::Mock::VerifyAndClearExpectations(&gpu_process_host_getter_);
  EXPECT_CALL(gpu_process_host_getter_, Run(testing::_)).Times(0);
  ASSERT_TRUE(SendCommandSync("VisualDebugger.stopStream"));
  Detach();
}

IN_PROC_BROWSER_TEST_F(VisualDebuggerHandlerTest,
                       DetachSucceedsWhenGpuProcessIsUnavailable) {
  UseMockGpuProcessHostGetter();
  testing::InSequence sequence;
  EXPECT_CALL(gpu_process_host_getter_, Run(true)).WillOnce(GetGpuProcessHost);
  EXPECT_CALL(gpu_process_host_getter_, Run(false))
      .WillOnce(testing::Return(nullptr));

  ASSERT_TRUE(SendCommandSync("VisualDebugger.startStream"));
  EXPECT_TRUE(enabled());

  Detach();
}

}  // namespace protocol
}  // namespace content

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"

namespace content {
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, VisualDebuggerTest) {
  set_agent_host_can_close();
  GURL url = GURL("data:text/html,<body></body>");
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
  Attach();
  SendCommandSync("VisualDebugger.startStream");
  WaitForNotification("VisualDebugger.frameResponse", true);

  auto filter_param =
      std::string(R"({"filters":[{"selector":{"anno":""},"active":true}]})");
  base::Value::Dict command_params;
  command_params.Set("json", filter_param);
  SendCommandSync("VisualDebugger.filterStream", std::move(command_params));
  SendCommandSync("VisualDebugger.stopStream");
}

}  // namespace content

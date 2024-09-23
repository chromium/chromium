// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/scoped_page_focus_override.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

ScopedPageFocusOverride::ScopedPageFocusOverride(
    content::WebContents* web_contents)
    : agent_host_(content::DevToolsAgentHost::GetOrCreateFor(web_contents)) {
  agent_host_->AttachClient(this);
  SetFocusEmulationEnabled(true);
}

ScopedPageFocusOverride::~ScopedPageFocusOverride() {
  SetFocusEmulationEnabled(false);
  agent_host_->DetachClient(this);
}

void ScopedPageFocusOverride::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  std::string_view message_str(reinterpret_cast<const char*>(message.data()),
                               message.size());
  std::optional<base::Value> parsed_message =
      base::JSONReader::Read(message_str);
  ASSERT_TRUE(parsed_message.has_value());

  std::optional<int> id = parsed_message->GetDict().FindInt("id");
  if (!id || !*id || *id != last_sent_id_)
    return;

  ASSERT_TRUE(run_loop_quit_closure_);
  std::move(run_loop_quit_closure_).Run();
}

void ScopedPageFocusOverride::AgentHostClosed(DevToolsAgentHost* agent_host) {}

void ScopedPageFocusOverride::SetFocusEmulationEnabled(bool enabled) {
  base::Value::Dict command =
      base::Value::Dict()
          .Set("id", ++last_sent_id_)
          .Set("method", "Emulation.setFocusEmulationEnabled")
          .Set("params", base::Value::Dict().Set("enabled", enabled));

  std::string json_command;
  base::JSONWriter::Write(command, &json_command);
  agent_host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(json_command)));

  base::RunLoop run_loop;
  EXPECT_FALSE(run_loop_quit_closure_);
  run_loop_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

}  // namespace content

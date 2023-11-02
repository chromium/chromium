// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/scoped_page_focus_override.h"

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  base::StringPiece message_str(reinterpret_cast<const char*>(message.data()),
                                message.size());
  absl::optional<base::Value> parsed_message =
      base::JSONReader::Read(message_str);
  ASSERT_TRUE(parsed_message.has_value());

  absl::optional<int> id = parsed_message->FindIntPath("id");
  if (!id || !*id || *id != last_sent_id_)
    return;

  ASSERT_TRUE(run_loop_quit_closure_);
  std::move(run_loop_quit_closure_).Run();
}

void ScopedPageFocusOverride::AgentHostClosed(DevToolsAgentHost* agent_host) {}

void ScopedPageFocusOverride::SetFocusEmulationEnabled(bool enabled) {
  base::Value command(base::Value::Type::DICTIONARY);
  command.SetIntKey("id", ++last_sent_id_);
  command.SetStringKey("method", "Emulation.setFocusEmulationEnabled");
  base::Value params(base::Value::Type::DICTIONARY);
  params.SetBoolKey("enabled", enabled);
  command.SetKey("params", std::move(params));

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

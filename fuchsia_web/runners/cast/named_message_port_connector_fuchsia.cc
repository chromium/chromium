// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/named_message_port_connector_fuchsia.h"

#include <fuchsia/mem/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "components/cast/named_message_port_connector/grit/named_message_port_connector_resources.h"

namespace {
constexpr uint64_t kPortConnectorBindingsId = 1000;
}  // namespace

NamedMessagePortConnectorFuchsia::NamedMessagePortConnectorFuchsia(
    fuchsia::web::Frame* frame)
    : frame_(frame) {
  DCHECK(frame_);

  base::FilePath port_connector_js_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &port_connector_js_path));
  port_connector_js_path = port_connector_js_path.AppendASCII(
      "components/cast/named_message_port_connector/"
      "named_message_port_connector.js");

  std::string bindings_script_string;
  CHECK(
      base::ReadFileToString(port_connector_js_path, &bindings_script_string));
  DCHECK(!bindings_script_string.empty())
      << "NamedMessagePortConnector resources not loaded.";

  // Inject the JS connection API into the Frame.
  constexpr char kBindingsScriptVmoName[] = "port-connector-js";
  fuchsia::mem::Buffer bindings_script = base::MemBufferFromString(
      std::move(bindings_script_string), kBindingsScriptVmoName);

  std::vector<std::string> origins = {"*"};
  frame_->AddBeforeLoadJavaScript(
      kPortConnectorBindingsId, std::move(origins), std::move(bindings_script),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        CHECK(result.is_response())
            << "Couldn't inject port connector bindings.";
      });
}

NamedMessagePortConnectorFuchsia::~NamedMessagePortConnectorFuchsia() {
  // Nothing to do if there is no attached Frame.
  if (!frame_)
    return;

  frame_->RemoveBeforeLoadJavaScript(kPortConnectorBindingsId);
}

void NamedMessagePortConnectorFuchsia::DetachFromFrame() {
  frame_ = nullptr;
}

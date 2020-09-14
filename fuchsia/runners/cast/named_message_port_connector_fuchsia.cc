// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/named_message_port_connector_fuchsia.h"

#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "components/cast/named_message_port_connector/grit/named_message_port_connector_resources.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
constexpr uint64_t kPortConnectorBindingsId = 1000;
}  // namespace

NamedMessagePortConnectorFuchsia::NamedMessagePortConnectorFuchsia(
    fuchsia::web::Frame* frame)
    : frame_(frame) {
  DCHECK(frame_);

  std::string bindings_script_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_PORT_CONNECTOR_JS);
  DCHECK(!bindings_script_string.empty())
      << "NamedMessagePortConnector resources not loaded.";

  // Inject the JS connection API into the Frame.
  constexpr char kBindingsScriptVmoName[] = "port-connector-js";
  fuchsia::mem::Buffer bindings_script = cr_fuchsia::MemBufferFromString(
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

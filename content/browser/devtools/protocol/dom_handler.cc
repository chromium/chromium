// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/dom_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"

namespace content {
namespace protocol {

DOMHandler::DOMHandler(bool allow_file_access)
    : DevToolsDomainHandler(DOM::Metainfo::domainName),
      host_(nullptr),
      allow_file_access_(allow_file_access) {}

DOMHandler::~DOMHandler() {
}

void DOMHandler::Wire(UberDispatcher* dispatcher) {
  DOM::Dispatcher::wire(dispatcher, this);
}

void DOMHandler::SetRenderer(int process_host_id,
                             RenderFrameHostImpl* frame_host) {
  host_ = frame_host;
}

Response DOMHandler::Disable() {
  return Response::OK();
}

Response DOMHandler::SetFileInputFiles(
    std::unique_ptr<protocol::Array<std::string>> files,
    Maybe<DOM::NodeId> node_id,
    Maybe<DOM::BackendNodeId> backend_node_id,
    Maybe<String> in_object_id) {
  if (!allow_file_access_)
    return Response::Error("Not allowed");
  if (host_) {
    for (const std::string& file : *files) {
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
          host_->GetProcess()->GetID(), base::FilePath::FromUTF8Unsafe(file));
    }
  }
  return Response::FallThrough();
}

}  // namespace protocol
}  // namespace content

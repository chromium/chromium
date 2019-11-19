// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_frontend_host_impl.h"

#include <stddef.h>
#include <memory>
#include <string>

#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

#if !defined(OS_FUCHSIA)
#include "content/browser/devtools/grit/devtools_resources_map.h"  // nogncheck
#endif

namespace content {

namespace {
const char kCompatibilityScript[] = "devtools_compatibility.js";
const char kCompatibilityScriptSourceURL[] =
    "\n//# "
    "sourceURL=devtools://devtools/bundled/devtools_compatibility.js";
}

// static
std::unique_ptr<DevToolsFrontendHost> DevToolsFrontendHost::Create(
    RenderFrameHost* frame_host,
    const HandleMessageCallback& handle_message_callback) {
  DCHECK(!frame_host->GetParent());
  return std::make_unique<DevToolsFrontendHostImpl>(frame_host,
                                                    handle_message_callback);
}

// static
void DevToolsFrontendHost::SetupExtensionsAPI(
    RenderFrameHost* frame_host,
    const std::string& extension_api) {
  DCHECK(frame_host->GetParent());
  mojo::AssociatedRemote<blink::mojom::DevToolsFrontend> frontend;
  frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&frontend);
  frontend->SetupDevToolsExtensionAPI(extension_api);
}

// static
scoped_refptr<base::RefCountedMemory>
DevToolsFrontendHost::GetFrontendResourceBytes(const std::string& path) {
#if !defined(OS_FUCHSIA)
  for (size_t i = 0; i < kDevtoolsResourcesSize; ++i) {
    if (path == kDevtoolsResources[i].name) {
      return GetContentClient()->GetDataResourceBytes(
          kDevtoolsResources[i].value);
    }
  }
#endif  // defined(OS_FUCHSIA)
  return nullptr;
}

// static
std::string DevToolsFrontendHost::GetFrontendResource(const std::string& path) {
  scoped_refptr<base::RefCountedMemory> bytes = GetFrontendResourceBytes(path);
  if (!bytes)
    return std::string();
  return std::string(bytes->front_as<char>(), bytes->size());
}

DevToolsFrontendHostImpl::DevToolsFrontendHostImpl(
    RenderFrameHost* frame_host,
    const HandleMessageCallback& handle_message_callback)
    : web_contents_(WebContents::FromRenderFrameHost(frame_host)),
      handle_message_callback_(handle_message_callback) {
  mojo::AssociatedRemote<blink::mojom::DevToolsFrontend> frontend;
  frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&frontend);
  std::string api_script =
      content::DevToolsFrontendHost::GetFrontendResource(kCompatibilityScript) +
      kCompatibilityScriptSourceURL;
  frontend->SetupDevToolsFrontend(api_script,
                                  receiver_.BindNewEndpointAndPassRemote());
}

DevToolsFrontendHostImpl::~DevToolsFrontendHostImpl() {
}

void DevToolsFrontendHostImpl::BadMessageRecieved() {
  bad_message::ReceivedBadMessage(web_contents_->GetMainFrame()->GetProcess(),
                                  bad_message::DFH_BAD_EMBEDDER_MESSAGE);
}

void DevToolsFrontendHostImpl::DispatchEmbedderMessage(
    const std::string& message) {
  handle_message_callback_.Run(message);
}

}  // namespace content

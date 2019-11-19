// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/dev_tools_host_file_system.h"

#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_host.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

DOMFileSystem* DevToolsHostFileSystem::isolatedFileSystem(
    DevToolsHost& host,
    const String& file_system_name,
    const String& root_url) {
  ExecutionContext* context = host.FrontendFrame()->GetDocument();
  return MakeGarbageCollected<DOMFileSystem>(
      context, file_system_name, mojom::blink::FileSystemType::kIsolated,
      KURL(root_url));
}

void DevToolsHostFileSystem::upgradeDraggedFileSystemPermissions(
    DevToolsHost& host,
    DOMFileSystem* dom_file_system) {
  auto message = std::make_unique<JSONObject>();
  message->SetInteger("id", 0);
  message->SetString("method", "upgradeDraggedFileSystemPermissions");
  auto params = std::make_unique<JSONArray>();
  params->PushString(dom_file_system->RootURL().GetString());
  message->SetArray("params", std::move(params));
  host.sendMessageToEmbedder(message->ToJSONString());
}

}  // namespace blink

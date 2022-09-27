// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/dev_tools_host_file_system.h"

#include <utility>

#include "base/values.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
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
  ExecutionContext* context = host.FrontendFrame()->DomWindow();
  return MakeGarbageCollected<DOMFileSystem>(
      context, file_system_name, mojom::blink::FileSystemType::kIsolated,
      KURL(root_url));
}

void DevToolsHostFileSystem::upgradeDraggedFileSystemPermissions(
    DevToolsHost& host,
    DOMFileSystem* dom_file_system) {
  base::Value::Dict message;
  message.Set("id", 0);
  message.Set("method", base::Value("upgradeDraggedFileSystemPermissions"));
  base::Value::List params;
  params.Append(dom_file_system->RootURL().GetString().Utf8());
  message.Set("params", std::move(params));
  host.sendMessageToEmbedder(std::move(message));
}

}  // namespace blink

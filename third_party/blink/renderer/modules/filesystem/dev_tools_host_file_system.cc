// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/dev_tools_host_file_system.h"

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
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetKey("id", base::Value(0));
  message.SetKey("method", base::Value("upgradeDraggedFileSystemPermissions"));
  base::Value params(base::Value::Type::LIST);
  params.Append(base::Value(dom_file_system->RootURL().GetString().Utf8()));
  message.SetKey("params", std::move(params));
  host.sendMessageToEmbedder(std::move(message));
}

}  // namespace blink

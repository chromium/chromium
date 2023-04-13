// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_manager.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return base::Singleton<DevToolsManager>::get();
}

DevToolsManager::DevToolsManager()
    : delegate_(
          GetContentClient()->browser()->CreateDevToolsManagerDelegate()) {}

void DevToolsManager::ShutdownForTests() {
  base::Singleton<DevToolsManager>::OnExit(nullptr);
}

DevToolsManager::~DevToolsManager() = default;

}  // namespace content

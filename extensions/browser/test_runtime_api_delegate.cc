// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_runtime_api_delegate.h"

#include "extensions/common/api/runtime.h"
#include "extensions/common/extension_id.h"

namespace extensions {

using api::runtime::PlatformInfo;

TestRuntimeAPIDelegate::TestRuntimeAPIDelegate() {
}

TestRuntimeAPIDelegate::~TestRuntimeAPIDelegate() {
}

void TestRuntimeAPIDelegate::AddUpdateObserver(UpdateObserver* observer) {
}

void TestRuntimeAPIDelegate::RemoveUpdateObserver(UpdateObserver* observer) {
}

void TestRuntimeAPIDelegate::ReloadExtension(const ExtensionId& extension_id) {}

bool TestRuntimeAPIDelegate::CheckForUpdates(const ExtensionId& extension_id,
                                             UpdateCheckCallback callback) {
  return false;
}

void TestRuntimeAPIDelegate::OpenURL(const GURL& uninstall_url) {
}

bool TestRuntimeAPIDelegate::GetPlatformInfo(PlatformInfo* info) {
  // TODO(rockot): This probably isn't right. Maybe this delegate should just
  // support manual PlatformInfo override for tests if necessary.
  info->os = api::runtime::PlatformOs::kCros;
  return true;
}

bool TestRuntimeAPIDelegate::RestartDevice(std::string* error_message) {
  return false;
}

}  // namespace extensions

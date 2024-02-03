// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_RUNTIME_API_DELEGATE_H_
#define EXTENSIONS_BROWSER_TEST_RUNTIME_API_DELEGATE_H_

#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class TestRuntimeAPIDelegate : public RuntimeAPIDelegate {
 public:
  TestRuntimeAPIDelegate();

  TestRuntimeAPIDelegate(const TestRuntimeAPIDelegate&) = delete;
  TestRuntimeAPIDelegate& operator=(const TestRuntimeAPIDelegate&) = delete;

  ~TestRuntimeAPIDelegate() override;

  // RuntimeAPIDelegate implementation.
  void AddUpdateObserver(UpdateObserver* observer) override;
  void RemoveUpdateObserver(UpdateObserver* observer) override;
  void ReloadExtension(const ExtensionId& extension_id) override;
  bool CheckForUpdates(const ExtensionId& extension_id,
                       UpdateCheckCallback callback) override;
  void OpenURL(const GURL& uninstall_url) override;
  bool GetPlatformInfo(api::runtime::PlatformInfo* info) override;
  bool RestartDevice(std::string* error_message) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_RUNTIME_API_DELEGATE_H_

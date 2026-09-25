// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_FAKE_EXTENSION_SERVICE_H_
#define IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_FAKE_EXTENSION_SERVICE_H_

#import <memory>

#import "base/callback_list.h"
#import "base/functional/callback.h"
#import "ios/chrome/browser/web_extension/model/extension_service.h"

namespace web {
enum class UniversalOptOutState;
}  // namespace web

// A fake `ExtensionService` for testing.
class FakeExtensionService final : public ExtensionService {
 public:
  FakeExtensionService();
  FakeExtensionService(const FakeExtensionService&) = delete;
  FakeExtensionService& operator=(const FakeExtensionService&) = delete;
  ~FakeExtensionService() override;

  // ExtensionService:
  void Initialize(web::UniversalOptOutState state) override;
  web::ExtensionController* GetExtensionController() const override;
  bool IsReady() const override;
  bool WebExtensionsWereLoadedAtStartup() const override;
  base::CallbackListSubscription RunWhenReady(
      base::OnceClosure callback) override;

  // Sets whether the service is ready. When transitioning to true, all pending
  // callbacks registered via `RunWhenReady` are executed.
  void SetReady(bool ready);

  // Sets whether web extensions are considered loaded at startup.
  void SetWebExtensionsWereLoadedAtStartup(bool loaded);

  // Returns true if there are pending callbacks waiting for the service to
  // become ready.
  bool HasWaitingCallback() const;

  // Returns true if `RunWhenReady` was ever called.
  bool WasWaitedUpon() const;

 private:
  bool is_ready_ = false;
  bool web_extensions_were_loaded_at_startup_ = false;
  bool was_waited_upon_ = false;
  base::OnceClosureList ready_callbacks_;
};

#endif  // IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_FAKE_EXTENSION_SERVICE_H_

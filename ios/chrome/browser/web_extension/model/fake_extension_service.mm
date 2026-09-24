// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/model/fake_extension_service.h"

#import <utility>

#import "base/check.h"

FakeExtensionService::FakeExtensionService() = default;

FakeExtensionService::~FakeExtensionService() = default;

void FakeExtensionService::Initialize() {}

web::ExtensionController* FakeExtensionService::GetExtensionController() const {
  return nullptr;
}

bool FakeExtensionService::IsReady() const {
  return is_ready_;
}

bool FakeExtensionService::WebExtensionsWereLoadedAtStartup() const {
  return web_extensions_were_loaded_at_startup_;
}

base::CallbackListSubscription FakeExtensionService::RunWhenReady(
    base::OnceClosure callback) {
  CHECK(!is_ready_);
  was_waited_upon_ = true;
  return ready_callbacks_.Add(std::move(callback));
}

void FakeExtensionService::SetReady(bool ready) {
  if (is_ready_ == ready) {
    return;
  }
  is_ready_ = ready;
  if (is_ready_) {
    ready_callbacks_.Notify();
  }
}

void FakeExtensionService::SetWebExtensionsWereLoadedAtStartup(bool loaded) {
  web_extensions_were_loaded_at_startup_ = loaded;
}

bool FakeExtensionService::HasWaitingCallback() const {
  return !is_ready_ && !ready_callbacks_.empty();
}

bool FakeExtensionService::WasWaitedUpon() const {
  return was_waited_upon_;
}

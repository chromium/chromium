// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/account_manager.h"

#include "base/check.h"

namespace remoting {
namespace ios {

namespace {

// Leaky.
static AccountManager* g_account_manager = nullptr;

}  // namespace

AccountManager::AccountManager() = default;

AccountManager::~AccountManager() = default;

// static
void AccountManager::SetInstance(
    std::unique_ptr<AccountManager> account_manager) {
  DCHECK(!g_account_manager);
  g_account_manager = account_manager.release();
}

// static
AccountManager* AccountManager::GetInstance() {
  DCHECK(g_account_manager);
  return g_account_manager;
}

}  // namespace ios
}  // namespace remoting

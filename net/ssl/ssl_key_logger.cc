// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_key_logger.h"

#include "base/check.h"
#include "base/no_destructor.h"

namespace net {

// static
bool SSLKeyLoggerManager::IsActive() {
  return Get()->ssl_key_logger_ != nullptr;
}

// static
void SSLKeyLoggerManager::SetSSLKeyLogger(
    std::unique_ptr<SSLKeyLogger> logger) {
  DCHECK(!IsActive());
  Get()->ssl_key_logger_ = std::move(logger);
}

// static
void SSLKeyLoggerManager::KeyLogCallback(const SSL* /*ssl*/, const char* line) {
  DCHECK(IsActive());
  Get()->ssl_key_logger_->WriteLine(line);
}

SSLKeyLoggerManager::SSLKeyLoggerManager() = default;

// static
SSLKeyLoggerManager* SSLKeyLoggerManager::Get() {
  static base::NoDestructor<SSLKeyLoggerManager> owner;
  return owner.get();
}

}  // namespace net

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/scoped_lacontext.h"

#include <LocalAuthentication/LocalAuthentication.h>

#include <memory>

#include "base/check.h"

namespace crypto::apple {

struct ScopedLAContext::ObjCStorage {
  LAContext* __strong context;
};

ScopedLAContext::ScopedLAContext(LAContext* lacontext)
    : storage_(std::make_unique<ObjCStorage>()) {
  storage_->context = lacontext;
}

ScopedLAContext::ScopedLAContext(ScopedLAContext&&) = default;
ScopedLAContext& ScopedLAContext::operator=(ScopedLAContext&& other) = default;
ScopedLAContext::~ScopedLAContext() = default;

LAContext* ScopedLAContext::release() {
  CHECK(storage_);
  LAContext* context = storage_->context;
  storage_.reset();
  return context;
}

}  // namespace crypto::apple

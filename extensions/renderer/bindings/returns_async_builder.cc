// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/returns_async_builder.h"

namespace extensions {

ReturnsAsyncBuilder::ReturnsAsyncBuilder() = default;
ReturnsAsyncBuilder::~ReturnsAsyncBuilder() = default;
ReturnsAsyncBuilder::ReturnsAsyncBuilder(
    std::vector<std::unique_ptr<ArgumentSpec>> signature)
    : signature_(std::move(signature)) {}

ReturnsAsyncBuilder& ReturnsAsyncBuilder::MakeOptional() {
  optional_ = true;
  return *this;
}

ReturnsAsyncBuilder& ReturnsAsyncBuilder::AddPromiseSupport() {
  promise_support_ = binding::APIPromiseSupport::kSupported;
  return *this;
}

std::unique_ptr<APISignature::ReturnsAsync> ReturnsAsyncBuilder::Build() {
  std::unique_ptr<APISignature::ReturnsAsync> returns_async =
      std::make_unique<APISignature::ReturnsAsync>();
  if (signature_)
    returns_async->signature = std::move(signature_);
  returns_async->optional = optional_;
  returns_async->promise_support = promise_support_;

  return returns_async;
}

}  // namespace extensions

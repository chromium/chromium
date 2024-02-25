// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_RETURNS_ASYNC_BUILDER_H_
#define EXTENSIONS_RENDERER_BINDINGS_RETURNS_ASYNC_BUILDER_H_

#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/argument_spec.h"

namespace extensions {

// A utility class for helping to construct ReturnsAsync in tests.
class ReturnsAsyncBuilder {
 public:
  ReturnsAsyncBuilder();
  explicit ReturnsAsyncBuilder(
      std::vector<std::unique_ptr<ArgumentSpec>> signature);
  ~ReturnsAsyncBuilder();

  ReturnsAsyncBuilder& MakeOptional();
  ReturnsAsyncBuilder& AddPromiseSupport();

  std::unique_ptr<APISignature::ReturnsAsync> Build();

 private:
  std::optional<std::vector<std::unique_ptr<ArgumentSpec>>> signature_;
  bool optional_ = false;
  binding::APIPromiseSupport promise_support_ =
      binding::APIPromiseSupport::kUnsupported;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_RETURNS_ASYNC_BUILDER_H_

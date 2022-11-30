// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_GET_PER_CONTEXT_DATA_H_
#define EXTENSIONS_RENDERER_BINDINGS_GET_PER_CONTEXT_DATA_H_

#include "gin/per_context_data.h"
#include "v8/include/v8.h"

namespace extensions {

enum CreatePerContextData { kCreateIfMissing, kDontCreateIfMissing };

template <typename PerContextData, typename... ConstructorArgs>
PerContextData* GetPerContextData(v8::Local<v8::Context> context,
                                  CreatePerContextData should_create,
                                  ConstructorArgs... constructor_args) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  if (!per_context_data)
    return nullptr;
  auto* data = static_cast<PerContextData*>(
      per_context_data->GetUserData(PerContextData::kPerContextDataKey));

  if (!data && should_create == kCreateIfMissing) {
    auto created_data = std::make_unique<PerContextData>(constructor_args...);
    data = created_data.get();
    per_context_data->SetUserData(PerContextData::kPerContextDataKey,
                                  std::move(created_data));
  }

  return data;
}

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_GET_PER_CONTEXT_DATA_H_

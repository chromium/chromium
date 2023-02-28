// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_CONSOLE_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_CONSOLE_H_

#include "base/memory/raw_ptr.h"
#include "content/common/shared_storage_worklet_service.mojom.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace shared_storage_worklet {

class Console final : public gin::Wrappable<Console> {
 public:
  explicit Console(mojom::SharedStorageWorkletServiceClient* client);
  ~Console() override;

  static gin::WrapperInfo kWrapperInfo;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  const char* GetTypeName() override;

 private:
  void Log(gin::Arguments* args);

  raw_ptr<mojom::SharedStorageWorkletServiceClient> client_;

  base::WeakPtrFactory<Console> weak_ptr_factory_{this};
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_CONSOLE_H_

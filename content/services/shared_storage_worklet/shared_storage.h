// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace shared_storage_worklet {

class SharedStorage final : public gin::Wrappable<SharedStorage> {
 public:
  SharedStorage(blink::mojom::SharedStorageWorkletServiceClient* client,
                const absl::optional<std::u16string>& embedder_context);
  ~SharedStorage() override;

  static gin::WrapperInfo kWrapperInfo;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  const char* GetTypeName() override;

 private:
  v8::Local<v8::Promise> Set(gin::Arguments* args);
  v8::Local<v8::Promise> Append(gin::Arguments* args);
  v8::Local<v8::Promise> Delete(gin::Arguments* args);
  v8::Local<v8::Promise> Clear(gin::Arguments* args);
  v8::Local<v8::Promise> Get(gin::Arguments* args);
  v8::Local<v8::Object> Keys(gin::Arguments* args);
  v8::Local<v8::Object> Entries(gin::Arguments* args);
  v8::Local<v8::Promise> Length(gin::Arguments* args);
  v8::Local<v8::Promise> RemainingBudget(gin::Arguments* args);
  v8::Local<v8::Value> Context(gin::Arguments* args);

  void OnVoidOperationFinished(
      v8::Isolate* isolate,
      v8::Global<v8::Promise::Resolver> global_resolver,
      blink::SharedStorageVoidOperation caller,
      base::TimeTicks start_time,
      bool success,
      const std::string& error_message);

  void OnStringRetrievalOperationFinished(
      v8::Isolate* isolate,
      v8::Global<v8::Promise::Resolver> global_resolver,
      base::TimeTicks start_time,
      blink::mojom::SharedStorageGetStatus status,
      const std::string& error_message,
      const std::u16string& result);

  void OnLengthOperationFinished(
      v8::Isolate* isolate,
      v8::Global<v8::Promise::Resolver> global_resolver,
      base::TimeTicks start_time,
      bool success,
      const std::string& error_message,
      uint32_t length);

  void OnBudgetOperationFinished(
      v8::Isolate* isolate,
      v8::Global<v8::Promise::Resolver> global_resolver,
      base::TimeTicks start_time,
      bool success,
      const std::string& error_message,
      double bits);

  raw_ptr<blink::mojom::SharedStorageWorkletServiceClient> client_;

  // If this worklet is inside a fenced frame or a URN iframe,
  // `embedder_context_` represents any contextual information written to the
  // frame's `blink::FencedFrameConfig` by the embedder before navigation to the
  // config. `embedder_context_` is passed to the worklet upon initialization.
  absl::optional<std::u16string> embedder_context_;

  base::WeakPtrFactory<SharedStorage> weak_ptr_factory_{this};
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_UNNAMED_OPERATION_HANDLER_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_UNNAMED_OPERATION_HANDLER_H_

#include <map>

#include "base/memory/raw_ref.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace shared_storage_worklet {

class UnnamedOperationHandler {
 public:
  struct PendingRequest;

  explicit UnnamedOperationHandler(
      const std::map<std::string, v8::Global<v8::Function>>&
          operation_definition_map);

  ~UnnamedOperationHandler();

  void RunOperation(
      v8::Local<v8::Context> context,
      const std::string& name,
      const std::vector<uint8_t>& serialized_data,
      blink::mojom::SharedStorageWorkletService::RunOperationCallback callback);

  void OnPromiseFulfilled(PendingRequest* request, gin::Arguments* args);

  void OnPromiseRejected(PendingRequest* request, gin::Arguments* args);

 private:
  const raw_ref<const std::map<std::string, v8::Global<v8::Function>>>
      operation_definition_map_;

  std::map<PendingRequest*, std::unique_ptr<PendingRequest>> pending_requests_;

  base::WeakPtrFactory<UnnamedOperationHandler> weak_ptr_factory_{this};
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_UNNAMED_OPERATION_HANDLER_H_

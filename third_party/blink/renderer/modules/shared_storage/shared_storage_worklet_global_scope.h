// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_WORKLET_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_WORKLET_GLOBAL_SCOPE_H_

#include "services/network/public/mojom/url_loader_factory.mojom-blink-forward.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_interest_group.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class V8NoArgumentConstructor;
class SharedStorage;
class SharedStorageWorkletNavigator;
class PrivateAggregation;
class Crypto;

class MODULES_EXPORT SharedStorageWorkletGlobalScope final
    : public WorkletGlobalScope,
      public Supplementable<SharedStorageWorkletGlobalScope> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SharedStorageWorkletGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params,
      WorkerThread* thread);
  ~SharedStorageWorkletGlobalScope() override;

  // SharedStorageWorkletGlobalScope IDL
  void Register(const String& name,
                V8NoArgumentConstructor* operation_ctor,
                ExceptionState&);

  bool IsSharedStorageWorkletGlobalScope() const override { return true; }
  bool IsSecureContext() const override { return true; }

  WorkletToken GetWorkletToken() const override { return token_; }
  ExecutionContextToken GetExecutionContextToken() const override {
    return token_;
  }

  void Trace(Visitor*) const override;

  // SharedStorageWorkletGlobalScope IDL
  SharedStorage* sharedStorage(ScriptState*, ExceptionState&);
  PrivateAggregation* privateAggregation(ScriptState*, ExceptionState&);
  Crypto* crypto(ScriptState*, ExceptionState&);
  ScriptPromise<IDLSequence<StorageInterestGroup>> interestGroups(
      ScriptState*,
      ExceptionState&);
  SharedStorageWorkletNavigator* Navigator(ScriptState*, ExceptionState&);

  bool add_module_finished() const { return true; }

 private:
  network::mojom::RequestDestination GetDestination() const override {
    NOTREACHED();
  }

  SharedStorageWorkletToken token_;
};

template <>
struct DowncastTraits<SharedStorageWorkletGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsSharedStorageWorkletGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_WORKLET_GLOBAL_SCOPE_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_RESOURCE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_RESOURCE_MANAGER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_reader.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"

namespace blink {

class NavigatorBase;
class SmartCardReaderPresenceObserver;

class MODULES_EXPORT SmartCardResourceManager final
    : public ScriptWrappable,
      public Supplement<NavigatorBase>,
      public ExecutionContextLifecycleObserver,
      public mojom::blink::SmartCardServiceClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using SmartCardReaderInfoPtr = mojom::blink::SmartCardReaderInfoPtr;

  static const char kSupplementName[];

  // Getter for navigator.smartCard
  static SmartCardResourceManager* smartCard(NavigatorBase&);

  explicit SmartCardResourceManager(NavigatorBase&);

  // ExecutionContextLifecycleObserver overrides.
  void ContextDestroyed() override;

  // ScriptWrappable overrides
  void Trace(Visitor*) const override;

  // SmartCardResourceManager idl
  ScriptPromise getReaders(ScriptState* script_state,
                           ExceptionState& exception_state);
  ScriptPromise watchForReaders(ScriptState* script_state,
                                ExceptionState& exception_state);

  // mojom::blink::SmartCardServiceClient overrides:
  void ReaderAdded(SmartCardReaderInfoPtr reader_info) override;
  void ReaderRemoved(SmartCardReaderInfoPtr reader_info) override;
  void ReaderChanged(SmartCardReaderInfoPtr reader_info) override;
  void Error(device::mojom::blink::SmartCardError error) override;

  // Used by SmarCardReader::connect
  void Connect(const String& reader_name,
               device::mojom::blink::SmartCardShareMode share_mode,
               device::mojom::blink::SmartCardProtocolsPtr preferred_protocols,
               mojom::blink::SmartCardService::ConnectCallback callback);

 private:
  SmartCardReader* GetOrCreateReader(SmartCardReaderInfoPtr info);
  void EnsureServiceConnection();
  void CloseServiceConnection();

  void FinishGetReaders(ScriptPromiseResolver*,
                        mojom::blink::SmartCardGetReadersResultPtr);
  void UpdateReadersCache(mojom::blink::SmartCardGetReadersResultPtr);

  void OnServiceClientRegistered(bool supports_reader_presence_observer);
  void ResolveWatchForReadersPromise(ScriptPromiseResolver* resolver);
  SmartCardReaderPresenceObserver* GetOrCreatePresenceObserver();

  HeapMojoRemote<mojom::blink::SmartCardService> service_;
  HeapMojoAssociatedReceiver<mojom::blink::SmartCardServiceClient,
                             SmartCardResourceManager>
      receiver_;
  HeapHashSet<Member<ScriptPromiseResolver>> get_readers_promises_;
  HeapHashSet<Member<ScriptPromiseResolver>> watch_for_readers_promises_;
  bool tracking_started_ = false;

  // maps a reader name to its object
  HeapHashMap<String, WeakMember<SmartCardReader>> reader_cache_;

  Member<SmartCardReaderPresenceObserver> presence_observer_;

  absl::optional<bool> supports_reader_presence_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_RESOURCE_MANAGER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_CACHED_STORAGE_AREA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_CACHED_STORAGE_AREA_H_

#include "base/trace_event/memory_dump_provider.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/storage/storage_area_map.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// An in-process implementation of LocalStorage using a LevelDB Mojo service.
// Maintains a complete cache of the origin's Map of key/value pairs for fast
// access. The cache is primed on first access and changes are written to the
// backend through the level db interface pointer. Mutations originating in
// other processes are applied to the cache via mojom::LevelDBObserver
// callbacks.
// There is one CachedStorageArea for potentially many LocalStorageArea
// objects.
class MODULES_EXPORT CachedStorageArea
    : public mojom::blink::StorageAreaObserver,
      public RefCounted<CachedStorageArea>,
      public base::trace_event::MemoryDumpProvider {
 public:
  // Instances of this class are used to identify the "source" of any changes
  // made to this storage area, as well as to dispatch any incoming change
  // events. Change events are not sent back to the source that caused the
  // change. The source passed to the various methods that modify storage
  // should have been registered first by calling RegisterSource.
  class Source : public GarbageCollectedMixin {
   public:
    virtual ~Source() = default;
    virtual KURL GetPageUrl() const = 0;
    // Return 'true' to continue receiving events, and 'false' to stop.
    virtual bool EnqueueStorageEvent(const String& key,
                                     const String& old_value,
                                     const String& new_value,
                                     const String& url) = 0;
    virtual blink::WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
        const char* name,
        WebScopedVirtualTimePauser::VirtualTaskDuration duration) = 0;
  };

  // Used to send events to the InspectorDOMStorageAgent.
  class InspectorEventListener {
   public:
    virtual ~InspectorEventListener() = default;
    virtual void DidDispatchStorageEvent(const SecurityOrigin* origin,
                                         const String& key,
                                         const String& old_value,
                                         const String& new_value) = 0;
  };

  static scoped_refptr<CachedStorageArea> CreateForLocalStorage(
      scoped_refptr<const SecurityOrigin> origin,
      mojo::PendingRemote<mojom::blink::StorageArea> area,
      scoped_refptr<base::SingleThreadTaskRunner> ipc_runner,
      InspectorEventListener* listener);
  static scoped_refptr<CachedStorageArea> CreateForSessionStorage(
      scoped_refptr<const SecurityOrigin> origin,
      mojo::PendingAssociatedRemote<mojom::blink::StorageArea> area,
      scoped_refptr<base::SingleThreadTaskRunner> ipc_runner,
      InspectorEventListener* listener);

  // These correspond to blink::Storage.
  unsigned GetLength();
  String GetKey(unsigned index);
  String GetItem(const String& key);
  bool SetItem(const String& key, const String& value, Source* source);
  void RemoveItem(const String& key, Source* source);
  void Clear(Source* source);

  // Allow this object to keep track of the Source instances corresponding to
  // it, which is needed for mutation event notifications.
  // Returns the (unique) id allocated for this source for testing purposes.
  String RegisterSource(Source* source);

  size_t quota_used() const { return map_ ? map_->quota_used() : 0; }
  size_t memory_used() const { return map_ ? map_->memory_used() : 0; }

  // Only public to allow tests to parametrize on this type.
  enum class FormatOption {
    kLocalStorageDetectFormat,
    kSessionStorageForceUTF16,
    kSessionStorageForceUTF8
  };

 private:
  CachedStorageArea(scoped_refptr<const SecurityOrigin> origin,
                    mojo::PendingRemote<mojom::blink::StorageArea> area,
                    scoped_refptr<base::SingleThreadTaskRunner> ipc_runner,
                    InspectorEventListener* listener);
  CachedStorageArea(
      scoped_refptr<const SecurityOrigin> origin,
      mojo::PendingAssociatedRemote<mojom::blink::StorageArea> area,
      scoped_refptr<base::SingleThreadTaskRunner> ipc_runner,
      InspectorEventListener* listener);

  friend class RefCounted<CachedStorageArea>;
  ~CachedStorageArea() override;

  friend class CachedStorageAreaTest;
  friend class CachedStorageAreaStringFormatTest;

  // StorageAreaObserver:
  void KeyAdded(const Vector<uint8_t>& key,
                const Vector<uint8_t>& value,
                const String& source) override;
  void KeyChanged(const Vector<uint8_t>& key,
                  const Vector<uint8_t>& new_value,
                  const Vector<uint8_t>& old_value,
                  const String& source) override;
  void KeyDeleted(const Vector<uint8_t>& key,
                  const Vector<uint8_t>& old_value,
                  const String& source) override;
  void AllDeleted(const String& source) override;
  void ShouldSendOldValueOnMutations(bool value) override;

  // base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Common helper for KeyAdded() and KeyChanged()
  void KeyAddedOrChanged(const Vector<uint8_t>& key,
                         const Vector<uint8_t>& new_value,
                         const String& old_value,
                         const String& source);

  void OnSetItemComplete(const String& key,
                         WebScopedVirtualTimePauser,
                         bool success);
  void OnRemoveItemComplete(const String& key,
                            WebScopedVirtualTimePauser,
                            bool success);
  void OnClearComplete(WebScopedVirtualTimePauser, bool success);
  void OnGetAllComplete(bool success);

  // Synchronously fetches the areas data if it hasn't been fetched already.
  void EnsureLoaded();

  // Resets the object back to its newly constructed state.
  void Reset();

  bool IsSessionStorage() const;
  FormatOption GetKeyFormat() const;
  FormatOption GetValueFormat() const;

  void EnqueueStorageEvent(const String& key,
                           const String& old_value,
                           const String& new_value,
                           const String& url,
                           const String& storage_area_id);

  static String Uint8VectorToString(const Vector<uint8_t>& input,
                                    FormatOption format_option);
  static Vector<uint8_t> StringToUint8Vector(const String& input,
                                             FormatOption format_option);

  scoped_refptr<const SecurityOrigin> origin_;
  InspectorEventListener* inspector_event_listener_;

  std::unique_ptr<StorageAreaMap> map_;

  HashMap<String, int> ignore_key_mutations_;
  bool ignore_all_mutations_ = false;

  // See ShouldSendOldValueOnMutations().
  bool should_send_old_value_on_mutations_ = true;

  // Depending on if this is a session storage or local storage area only one of
  // |mojo_area_remote_| and |mojo_area_associated_remote_| will be non-null.
  // Either way |mojo_area_| will be equal to the non-null one.
  mojo::Remote<mojom::blink::StorageArea> mojo_area_remote_;
  mojo::AssociatedRemote<mojom::blink::StorageArea>
      mojo_area_associated_remote_;
  mojom::blink::StorageArea* mojo_area_;
  mojo::AssociatedReceiver<mojom::blink::StorageAreaObserver> receiver_{this};

  Persistent<HeapHashMap<WeakMember<Source>, String>> areas_;

  base::WeakPtrFactory<CachedStorageArea> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CachedStorageArea);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_CACHED_STORAGE_AREA_H_

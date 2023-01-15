// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_MANAGER_H_

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom-blink.h"
#include "third_party/blink/public/web/web_manifest_manager.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ManifestChangeNotifier;
class ManifestManagerTest;
class ManifestFetcher;
class ResourceResponse;

// The ManifestManager is a helper class that takes care of fetching and parsing
// the Manifest of the associated window. It uses the ManifestFetcher and
// the ManifestParser in order to do so.
//
// Consumers should use the mojo ManifestManager interface to use this class.
class MODULES_EXPORT ManifestManager
    : public GarbageCollected<ManifestManager>,
      public Supplement<LocalDOMWindow>,
      public mojom::blink::ManifestManager,
      public ExecutionContextLifecycleObserver {
 public:
  static const char kSupplementName[];

  static ManifestManager* From(LocalDOMWindow&);

  explicit ManifestManager(LocalDOMWindow&);

  ManifestManager(const ManifestManager&) = delete;
  ManifestManager& operator=(const ManifestManager&) = delete;

  ~ManifestManager() override;

  void DidChangeManifest();
  bool CanFetchManifest();

  KURL ManifestURL() const;
  bool ManifestUseCredentials() const;

  void RequestManifestForTesting(WebManifestManager::Callback callback);
  void SetManifestChangeNotifierForTest(ManifestChangeNotifier* notifier) {
    manifest_change_notifier_ = notifier;
  }

  // mojom::blink::ManifestManager implementation.
  void RequestManifest(RequestManifestCallback callback) override;
  void RequestManifestDebugInfo(
      RequestManifestDebugInfoCallback callback) override;
  void ParseManifestFromString(
      const KURL& document_url,
      const KURL& manifest_url,
      const String& manifest_contents,
      ParseManifestFromStringCallback callback) override;

  void Trace(Visitor*) const override;

 private:
  enum class ResolveState { kSuccess, kFailure };

  using InternalRequestManifestCallback =
      base::OnceCallback<void(const KURL&,
                              const mojom::blink::ManifestPtr&,
                              const mojom::blink::ManifestDebugInfo*)>;

  // From ExecutionContextLifecycleObserver
  void ContextDestroyed() override;

  void RequestManifestImpl(InternalRequestManifestCallback callback);

  void FetchManifest();
  void OnManifestFetchComplete(const KURL& document_url,
                               const ResourceResponse& response,
                               const String& data);
  void RecordMetrics(const mojom::blink::Manifest& manifest);
  void ResolveCallbacks(ResolveState state);

  void BindReceiver(
      mojo::PendingReceiver<mojom::blink::ManifestManager> receiver);

  friend class ManifestManagerTest;

  Member<ManifestFetcher> fetcher_;
  Member<ManifestChangeNotifier> manifest_change_notifier_;

  // Whether the window may have an associated Manifest. If true, the frame
  // may have a manifest, if false, it can't have one. This boolean is true when
  // DidChangeManifest() is called, if it is never called, it means that the
  // associated document has no <link rel="manifest">.
  bool may_have_manifest_;

  // Whether the current Manifest is dirty.
  bool manifest_dirty_;

  // Current Manifest. Might be outdated if manifest_dirty_ is true.
  mojom::blink::ManifestPtr manifest_;

  // The URL of the current manifest.
  KURL manifest_url_;

  // Current Manifest debug information.
  mojom::blink::ManifestDebugInfoPtr manifest_debug_info_;

  Vector<InternalRequestManifestCallback> pending_callbacks_;

  HeapMojoReceiverSet<mojom::blink::ManifestManager, ManifestManager>
      receivers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_MANAGER_H_

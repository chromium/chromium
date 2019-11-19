// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_MANAGER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom-blink.h"
#include "third_party/blink/public/web/web_manifest_manager.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ManifestChangeNotifier;
class ManifestManagerTest;
class ManifestFetcher;
class ResourceResponse;

// The ManifestManager is a helper class that takes care of fetching and parsing
// the Manifest of the associated LocalFrame. It uses the ManifestFetcher and
// the ManifestParser in order to do so.
//
// Consumers should use the mojo ManifestManager interface to use this class.
class MODULES_EXPORT ManifestManager : public GarbageCollected<ManifestManager>,
                                       public Supplement<LocalFrame>,
                                       public mojom::blink::ManifestManager,
                                       public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ManifestManager);
  USING_PRE_FINALIZER(ManifestManager, Prefinalize);

 public:
  static const char kSupplementName[];

  static ManifestManager* From(LocalFrame&);

  static void ProvideTo(LocalFrame&);

  explicit ManifestManager(LocalFrame&);
  ~ManifestManager() override;

  void DidChangeManifest();
  void DidCommitLoad();
  bool CanFetchManifest();

  KURL ManifestURL() const;
  bool ManifestUseCredentials() const;

  void RequestManifestForTesting(WebManifestManager::Callback callback);

  // mojom::blink::ManifestManager implementation.
  void RequestManifest(RequestManifestCallback callback) override;
  void RequestManifestDebugInfo(
      RequestManifestDebugInfoCallback callback) override;

  void Trace(blink::Visitor*) override;

 private:
  enum ResolveState { ResolveStateSuccess, ResolveStateFailure };

  using InternalRequestManifestCallback =
      base::OnceCallback<void(const KURL&,
                              const mojom::blink::ManifestPtr&,
                              const mojom::blink::ManifestDebugInfo*)>;

  // From ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  void RequestManifestImpl(InternalRequestManifestCallback callback);

  void FetchManifest();
  void OnManifestFetchComplete(const KURL& document_url,
                               const ResourceResponse& response,
                               const String& data);
  void ResolveCallbacks(ResolveState state);

  void BindReceiver(
      mojo::PendingReceiver<mojom::blink::ManifestManager> receiver);

  void Prefinalize();

  friend class ManifestManagerTest;

  Member<ManifestFetcher> fetcher_;
  Member<ManifestChangeNotifier> manifest_change_notifier_;

  // Whether the LocalFrame may have an associated Manifest. If true, the frame
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

  mojo::ReceiverSet<mojom::blink::ManifestManager> receivers_;

  DISALLOW_COPY_AND_ASSIGN(ManifestManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_MANAGER_H_

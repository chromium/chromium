// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_SYNC_SYNC_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_SYNC_SYNC_MANAGER_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;

class SyncManager final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SyncManager* Create(
      ServiceWorkerRegistration* registration,
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    return MakeGarbageCollected<SyncManager>(registration,
                                             std::move(task_runner));
  }

  SyncManager(ServiceWorkerRegistration*,
              scoped_refptr<base::SequencedTaskRunner>);

  ScriptPromise registerFunction(ScriptState*, const String& tag);
  ScriptPromise getTags(ScriptState*);

  void Trace(blink::Visitor*) override;

  enum { kUnregisteredSyncID = -1 };

 private:
  // Returns an initialized
  // mojo::Remote<mojom::blink::OneShotBackgroundSyncService>. A connection with
  // the browser's OneShotBackgroundSyncService is created the first time this
  // method is called.
  const mojo::Remote<mojom::blink::OneShotBackgroundSyncService>&
  GetBackgroundSyncServiceRemote();

  // Callbacks
  void RegisterCallback(ScriptPromiseResolver*,
                        mojom::blink::BackgroundSyncError,
                        mojom::blink::SyncRegistrationOptionsPtr options);
  static void GetRegistrationsCallback(
      ScriptPromiseResolver*,
      mojom::blink::BackgroundSyncError,
      WTF::Vector<mojom::blink::SyncRegistrationOptionsPtr> registrations);

  Member<ServiceWorkerRegistration> registration_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::Remote<mojom::blink::OneShotBackgroundSyncService>
      background_sync_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_SYNC_SYNC_MANAGER_H_

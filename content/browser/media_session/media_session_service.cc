// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_session_service.h"

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/media_session_service.h"
#include "services/media_session/public/cpp/features.h"

namespace content {

media_session::mojom::MediaSessionService& GetMediaSessionService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // NOTE: We use sequence-local storage here strictly to limit the lifetime of
  // this Remote to that of the UI-thread sequence. This ensures that it doesn't
  // persist when the task environment is torn down and reinitialized e.g.
  // between unit tests.
  static base::NoDestructor<base::SequenceLocalStorageSlot<
      mojo::Remote<media_session::mojom::MediaSessionService>>>
      remote_slot;
  auto& remote = remote_slot->GetOrCreateValue();
  if (!remote) {
    if (base::FeatureList::IsEnabled(
            media_session::features::kMediaSessionService)) {
      static base::NoDestructor<
          std::unique_ptr<media_session::MediaSessionService>>
          service;
      *service = std::make_unique<media_session::MediaSessionService>(
          remote.BindNewPipeAndPassReceiver());
    } else {
      // If the service is not enabled, bind to a disconnected pipe.
      ignore_result(remote.BindNewPipeAndPassReceiver());
    }
  }
  return *remote.get();
}

}  // namespace content

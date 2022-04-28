// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/stable_video_decoder_factory.h"

#include "base/threading/sequence_local_storage_slot.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/service_process_host.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

media::stable::mojom::StableVideoDecoderFactory&
GetStableVideoDecoderFactory() {
  // NOTE: We use sequence-local storage to limit the lifetime of this Remote to
  // that of the UI-thread sequence. This ensures that the Remote is destroyed
  // when the task environment is torn down and reinitialized, e.g. between unit
  // tests.
  static base::SequenceLocalStorageSlot<
      mojo::Remote<media::stable::mojom::StableVideoDecoderFactory>>
      remote_slot;
  auto& remote = remote_slot.GetOrCreateValue();
  remote.reset_on_disconnect();
  [[maybe_unused]] auto receiver = remote.BindNewPipeAndPassReceiver();
  if (!remote) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // TODO(pmolinalopez): for LaCrOS, we need to use crosapi to establish a
    // StableVideoDecoderFactory connection to ash-chrome.
    NOTIMPLEMENTED();
#else
    ServiceProcessHost::Launch(
        std::move(receiver),
        ServiceProcessHost::Options().WithDisplayName("Video Decoder").Pass());
#endif
  }
  return *remote.get();
}

}  // namespace content

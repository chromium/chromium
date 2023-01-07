// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/media_keys_controller.h"

#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

namespace blink {

// static
const char MediaKeysController::kSupplementName[] = "MediaKeysController";

MediaKeysController::MediaKeysController() : Supplement(nullptr) {}

WebEncryptedMediaClient* MediaKeysController::EncryptedMediaClient(
    ExecutionContext* context) {
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(To<LocalDOMWindow>(context)->GetFrame());
  return web_frame->Client()->EncryptedMediaClient();
}

void MediaKeysController::ProvideMediaKeysTo(Page& page) {
  MediaKeysController::ProvideTo(page,
                                 MakeGarbageCollected<MediaKeysController>());
}

}  // namespace blink

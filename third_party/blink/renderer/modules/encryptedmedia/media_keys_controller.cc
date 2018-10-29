// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/media_keys_controller.h"

#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

namespace blink {

// static
const char MediaKeysController::kSupplementName[] = "MediaKeysController";

MediaKeysController::MediaKeysController() = default;

WebEncryptedMediaClient* MediaKeysController::EncryptedMediaClient(
    ExecutionContext* context) {
  Document* document = To<Document>(context);
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());
  return web_frame->Client()->EncryptedMediaClient();
}

void MediaKeysController::ProvideMediaKeysTo(Page& page) {
  MediaKeysController::ProvideTo(page, new MediaKeysController());
}

}  // namespace blink

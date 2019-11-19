// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_METADATA_COPYLESS_PASTE_SERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_METADATA_COPYLESS_PASTE_SERVER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/document_metadata/copyless_paste.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class LocalFrame;

// Mojo interface to return extracted metadata for AppIndexing.
class MODULES_EXPORT CopylessPasteServer final
    : public mojom::document_metadata::blink::CopylessPaste {
 public:
  explicit CopylessPasteServer(LocalFrame&);

  static void BindMojoReceiver(
      LocalFrame*,
      mojo::PendingReceiver<mojom::document_metadata::blink::CopylessPaste>);

  void GetEntities(GetEntitiesCallback) override;

 private:
  WeakPersistent<LocalFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_METADATA_COPYLESS_PASTE_SERVER_H_

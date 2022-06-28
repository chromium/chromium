// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_HANDLE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_HANDLE_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/core/html/media/media_source_handle.h"
#include "third_party/blink/renderer/modules/mediasource/handle_attachment_provider.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MediaSourceHandleImpl final : public ScriptWrappable,
                                    public MediaSourceHandle {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit MediaSourceHandleImpl(
      scoped_refptr<HandleAttachmentProvider> attachment_provider,
      String internal_blob_url);
  ~MediaSourceHandleImpl() override;

  scoped_refptr<HandleAttachmentProvider> TakeAttachmentProvider();

  scoped_refptr<MediaSourceAttachment> TakeAttachment() override;
  String GetInternalBlobURL() override;

  void mark_serialized();

  void Trace(Visitor*) const override;

 private:
  scoped_refptr<HandleAttachmentProvider> attachment_provider_;
  String internal_blob_url_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_HANDLE_IMPL_H_

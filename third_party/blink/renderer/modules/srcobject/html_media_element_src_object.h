// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SRCOBJECT_HTML_MEDIA_ELEMENT_SRC_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SRCOBJECT_HTML_MEDIA_ELEMENT_SRC_OBJECT_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class MediaStream;
class HTMLMediaElement;

class MODULES_EXPORT HTMLMediaElementSrcObject {
  STATIC_ONLY(HTMLMediaElementSrcObject);

 public:
  static MediaStream* srcObject(HTMLMediaElement&);
  static void setSrcObject(HTMLMediaElement&, MediaStream*);
};

}  // namespace blink

#endif

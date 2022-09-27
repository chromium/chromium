// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_SHADOW_ROOT_PICTURE_IN_PICTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_SHADOW_ROOT_PICTURE_IN_PICTURE_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;
class TreeScope;

class ShadowRootPictureInPicture {
  STATIC_ONLY(ShadowRootPictureInPicture);

 public:
  static Element* pictureInPictureElement(TreeScope&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_SHADOW_ROOT_PICTURE_IN_PICTURE_H_

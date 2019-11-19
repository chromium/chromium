// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_MEDIA_TYPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_MEDIA_TYPE_H_

namespace blink {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.blink_public.common
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ContextMenuDataMediaType
enum class ContextMenuDataMediaType {
  // No special node is in context.
  kNone,
  // An image node is selected.
  kImage,
  // A video node is selected.
  kVideo,
  // An audio node is selected.
  kAudio,
  // A canvas node is selected.
  kCanvas,
  // A file node is selected.
  kFile,
  // A plugin node is selected.
  kPlugin,
  kLast = kPlugin
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_MEDIA_TYPE_H_

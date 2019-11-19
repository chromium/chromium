// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_GUEST_VIEW_MIME_HANDLER_VIEW_UMA_TYPES_H_
#define EXTENSIONS_COMMON_GUEST_VIEW_MIME_HANDLER_VIEW_UMA_TYPES_H_

namespace extensions {

// Static non-instantiable class which holds certain enum types used for UMA and
// and tracking MimeHandlerView postMessage uses on websites.
class MimeHandlerViewUMATypes {
 public:
  static const char kUMAName[];

  // Tracks creation of MimeHandlerView classes as well as use cases of
  // postMessage API for both same-origin and cross-origin resources.
  enum class Type {
    // Emitted when a container is created; this does not necessarily lead to
    // a MimeHandlerViewGuest but postMessage is possible.
    kDidCreateMimeHandlerViewContainerBase = 0,
    // Emitted when a MimeHandlerViewGuest is created and the extension inside
    // the guest has loaded.
    kDidLoadExtension = 1,
    // Emitted for messages sent to an accessible resource (e.g., same-origin).
    kAccessibleInvalid = 2,
    kAccessibleGetSelectedText = 3,
    kAccessiblePrint = 4,
    kAccessibleSelectAll = 5,
    // Emitted for messages sent to an inaccessible resource (e.g.,
    // same-origin).
    kInaccessibleInvalid = 6,
    kInaccessibleGetSelectedText = 7,
    kInaccessiblePrint = 8,
    kInaccessibleSelectAll = 9,
    // For recording postMessage to embedded MimeHandlerViews (e.g., MHVs not
    // created due to navigations to the resource).
    kPostMessageToEmbeddedMimeHandlerView = 10,
    // For recording postMessage from internal APIs (includes 'print' messages
    // full page MimeHandlerView).
    kPostMessageInternal = 11,
    // The following track lifetime events for a frame-based MimeHandlerView.
    kCreateFrameContainer = 12,
    kReuseFrameContaienr = 13,
    kRemoveFrameContainerUpdatePlugin = 14,
    kRemoveFrameContainerUnexpectedFrames = 15,
    kMaxValue = kRemoveFrameContainerUnexpectedFrames,
  };

 private:
  MimeHandlerViewUMATypes();
  ~MimeHandlerViewUMATypes();

  MimeHandlerViewUMATypes(const MimeHandlerViewUMATypes&) = delete;
  MimeHandlerViewUMATypes& operator=(const MimeHandlerViewUMATypes&) = delete;
};

}  // namespace extensions
#endif  // EXTENSIONS_COMMON_GUEST_VIEW_MIME_HANDLER_VIEW_UMA_TYPES_H_
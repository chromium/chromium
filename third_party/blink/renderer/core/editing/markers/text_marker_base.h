// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_MARKER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_MARKER_BASE_H_

#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// A subclass of DocumentMarker used to implement functionality shared between
// text match and text fragment markers, which share painting logic.
class CORE_EXPORT TextMarkerBase : public DocumentMarker {
 public:
  TextMarkerBase(unsigned start_offset, unsigned end_offset);

  virtual bool IsActiveMatch() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextMarkerBase);
};

bool CORE_EXPORT IsTextMarker(const DocumentMarker&);

template <>
struct DowncastTraits<TextMarkerBase> {
  static bool AllowFrom(const DocumentMarker& marker) {
    return IsTextMarker(marker);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_MARKER_BASE_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_TEXT_SEGMENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_TEXT_SEGMENTS_H_

#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

// Provides functions for finding boundary of text segments.
class TextSegments final {
  STATIC_ONLY(TextSegments);

 public:
  // The interface to find boundary of text segment.
  class Finder {
   public:
    class Position final {
      STACK_ALLOCATED();

     public:
      Position();

      static Position Before(wtf_size_t offset);
      static Position After(wtf_size_t offset);

      bool IsAfter() const { return type_ == kAfter; }
      bool IsBefore() const { return type_ == kBefore; }
      bool IsNone() const { return type_ == kNone; }

      wtf_size_t Offset() const;

     private:
      enum Type { kNone, kBefore, kAfter };

      Position(wtf_size_t value, Type type);

      const wtf_size_t offset_ = 0;
      const Type type_ = kNone;
    };

    // Returns a text segment boundary position in |text| from |offset|.
    // Note: |text| must contains character 16.
    // Note: Since implementations can have state, |Find()| function isn't
    // marked |const| intentionally.
    virtual Position Find(const String text, wtf_size_t offset) = 0;
  };

  // Returns a boundary position found by |finder| followed by |position|
  // (inclusive). |finder| can be stateful or stateless.
  static PositionInFlatTree FindBoundaryForward(
      const PositionInFlatTree& position,
      Finder* finder);

  static PositionInFlatTree FindBoundaryBackward(
      const PositionInFlatTree& position,
      Finder* finder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_TEXT_SEGMENTS_H_

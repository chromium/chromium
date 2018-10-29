// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_HYPHENATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_HYPHENATION_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class PLATFORM_EXPORT Hyphenation : public RefCounted<Hyphenation> {
 public:
  virtual ~Hyphenation() = default;

  // Find the last hyphenation location before |before_index|.
  // Returns 0 if no hyphenation locations were found.
  virtual wtf_size_t LastHyphenLocation(const StringView&,
                                        wtf_size_t before_index) const = 0;

  // Find the first hyphenation location after |after_index|.
  // Returns 0 if no hyphenation locations were found.
  virtual wtf_size_t FirstHyphenLocation(const StringView&,
                                         wtf_size_t after_index) const;

  // Find all hyphenation locations in the reverse order.
  virtual Vector<wtf_size_t, 8> HyphenLocations(const StringView&) const;

  static const unsigned kMinimumPrefixLength = 2;
  static const unsigned kMinimumSuffixLength = 2;

 private:
  friend class LayoutLocale;
  static scoped_refptr<Hyphenation> PlatformGetHyphenation(
      const AtomicString& locale);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_HYPHENATION_H_

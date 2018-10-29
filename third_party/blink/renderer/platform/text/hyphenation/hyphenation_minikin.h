// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_HYPHENATION_HYPHENATION_MINIKIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_HYPHENATION_HYPHENATION_MINIKIN_H_

#include "third_party/blink/renderer/platform/text/hyphenation.h"

#include "base/files/memory_mapped_file.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace base {
class File;
}  // namespace base

namespace android {
class Hyphenator;
}  // namespace andorid

namespace blink {

class PLATFORM_EXPORT HyphenationMinikin final : public Hyphenation {
 public:
  bool OpenDictionary(const AtomicString& locale);

  wtf_size_t LastHyphenLocation(const StringView& text,
                                wtf_size_t before_index) const override;
  Vector<wtf_size_t, 8> HyphenLocations(const StringView&) const override;

  static scoped_refptr<HyphenationMinikin> FromFileForTesting(base::File);

 private:
  bool OpenDictionary(base::File);

  std::vector<uint8_t> Hyphenate(const StringView&) const;

  base::MemoryMappedFile file_;
  std::unique_ptr<android::Hyphenator> hyphenator_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_HYPHENATION_HYPHENATION_MINIKIN_H_

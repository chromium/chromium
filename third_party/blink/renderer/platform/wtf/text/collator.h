/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_COLLATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_COLLATOR_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

struct UCollator;

namespace WTF {

class WTF_EXPORT Collator {
  USING_FAST_MALLOC(Collator);

 public:
  enum Result { kEqual = 0, kGreater = 1, kLess = -1 };

  // From ICU's uloc.h (ULOC_FULLNAME_CAPACITY)
  static const size_t kUlocFullnameCapacity = 157;

  // Parsing is lenient; e.g. language identifiers (such as "en-US") are
  // accepted, too.
  explicit Collator(const char* locale);

  ~Collator();
  void SetOrderLowerFirst(bool);

  static std::unique_ptr<Collator> UserDefault();

  Result Collate(const ::UChar*, uint32_t, const ::UChar*, uint32_t) const;

 private:
  void CreateCollator() const;
  void ReleaseCollator();
  void SetEquivalentLocale(const char*, char*);
  mutable UCollator* collator_;

  char* locale_;
  char equivalent_locale_[kUlocFullnameCapacity];
  bool lower_first_;

  DISALLOW_COPY_AND_ASSIGN(Collator);
};

}  // namespace WTF

using WTF::Collator;

#endif

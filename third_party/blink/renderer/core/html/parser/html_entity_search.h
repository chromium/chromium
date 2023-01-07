/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_ENTITY_SEARCH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_ENTITY_SEARCH_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct HTMLEntityTableEntry;

class HTMLEntitySearch {
  STACK_ALLOCATED();

 public:
  HTMLEntitySearch();

  void Advance(UChar);

  bool IsEntityPrefix() const { return !!first_; }
  uint16_t CurrentLength() const { return current_length_; }

  const HTMLEntityTableEntry* MostRecentMatch() const {
    return most_recent_match_;
  }

 private:
  enum CompareResult {
    kBefore,
    kPrefix,
    kAfter,
  };

  CompareResult Compare(const HTMLEntityTableEntry*, UChar) const;
  const HTMLEntityTableEntry* FindFirst(UChar) const;
  const HTMLEntityTableEntry* FindLast(UChar) const;

  void Fail() {
    first_ = nullptr;
    last_ = nullptr;
  }

  uint16_t current_length_;

  const HTMLEntityTableEntry* most_recent_match_;
  const HTMLEntityTableEntry* first_;
  const HTMLEntityTableEntry* last_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_ENTITY_SEARCH_H_

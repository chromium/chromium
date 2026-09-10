/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/hash_functions_memory.h"

#include <stdint.h>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

namespace blink {

namespace {

const UChar kNullUChars[1] = {0};

const uint64_t kEmptyStringHash = 0x5A6EF77074EBC84B;
const uint64_t kSingleNullCharacterHash = 0x48DFCE108249B3F8;

const LChar kTestALChars[5] = {0x41, 0x95, 0xFF, 0x50, 0x01};
const UChar kTestBUChars[5] = {0x41, 0x95, 0xFFFF, 0x1080, 0x01};

const uint64_t kTestAHash = 0xE9422771E0A5DDE6;
const uint64_t kTestBHash = 0x4A2DA770EEA75C1E;

}  // namespace

TEST(HashFunctionsMemoryTest, HashMemory) {
  EXPECT_EQ(kEmptyStringHash, HashMemory64(base::span<const uint8_t>()));
  EXPECT_EQ(kEmptyStringHash, HashMemory64(base::span<const uint8_t, 0>()));
  EXPECT_EQ(kEmptyStringHash,
            HashMemory64(base::as_byte_span(kNullUChars).first(0u)));

  EXPECT_EQ(kSingleNullCharacterHash,
            HashMemory64(base::as_byte_span(kNullUChars).first(1u)));

  EXPECT_EQ(kTestAHash, HashMemory64(kTestALChars));
  EXPECT_EQ(kTestBHash, HashMemory64(base::as_byte_span(kTestBUChars)));

  EXPECT_EQ(static_cast<uint32_t>(kEmptyStringHash),
            HashMemory32(base::span<const uint8_t>()));
  EXPECT_EQ(static_cast<uint32_t>(kTestAHash), HashMemory32(kTestALChars));
  EXPECT_EQ(static_cast<uint32_t>(kTestBHash),
            HashMemory32(base::as_byte_span(kTestBUChars)));
}

}  // namespace blink

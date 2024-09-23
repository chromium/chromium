/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

#include <stddef.h>

#include <memory>
#include <type_traits>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/wtf/container_annotations.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace WTF {

struct SameSizeAsRefCounted {
  uint32_t a;
  // Don't add anything here because this should stay small.
};

template <typename T, unsigned inlineCapacity = 0>
struct SameSizeAsVectorWithInlineCapacity;

template <typename T>
struct SameSizeAsVectorWithInlineCapacity<T, 0> {
  void* buffer_pointer;
  unsigned capacity;
  unsigned size;
};

template <typename T, unsigned inlineCapacity>
struct SameSizeAsVectorWithInlineCapacity {
  SameSizeAsVectorWithInlineCapacity<T, 0> base_capacity;
#if !defined(ANNOTATE_CONTIGUOUS_CONTAINER)
  T inline_buffer[inlineCapacity];
#endif
};

#if !DCHECK_IS_ON()
ASSERT_SIZE(RefCounted<int>, SameSizeAsRefCounted);
#endif

ASSERT_SIZE(std::unique_ptr<int>, int*);
ASSERT_SIZE(scoped_refptr<RefCounted<int>>, int*);
ASSERT_SIZE(Vector<int>, SameSizeAsVectorWithInlineCapacity<int>);
// This is to avoid problem of comma in macro parameters.
#define INLINE_CAPACITY_PARAMS(i) int, i
ASSERT_SIZE(Vector<INLINE_CAPACITY_PARAMS(1)>,
            SameSizeAsVectorWithInlineCapacity<INLINE_CAPACITY_PARAMS(1)>);
ASSERT_SIZE(Vector<INLINE_CAPACITY_PARAMS(2)>,
            SameSizeAsVectorWithInlineCapacity<INLINE_CAPACITY_PARAMS(2)>);
ASSERT_SIZE(Vector<INLINE_CAPACITY_PARAMS(3)>,
            SameSizeAsVectorWithInlineCapacity<INLINE_CAPACITY_PARAMS(3)>);

// Check that the properties documented for wtf_size_t to size_t conversions
// are met.
static_assert(sizeof(wtf_size_t) <= sizeof(size_t));
static_assert(std::is_signed_v<wtf_size_t> == std::is_signed_v<size_t>);

}  // namespace WTF

/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_view.h"

namespace blink {

ArrayBuffer::ArrayBuffer(ArrayBufferContents& contents) : is_detached_(false) {
  if (contents.IsShared())
    contents.ShareWith(contents_);
  else
    contents.Transfer(contents_);
}

bool ArrayBuffer::Transfer(ArrayBufferContents& result) {
  DCHECK(!IsShared());
  scoped_refptr<ArrayBuffer> keep_alive(this);

  if (!contents_.Data()) {
    result.Detach();
    return false;
  }

  bool all_views_are_detachable = true;
  for (auto* view : views_) {
    if (!view->IsDetachable()) {
      all_views_are_detachable = false;
    }
  }

  if (all_views_are_detachable) {
    contents_.Transfer(result);

    for (auto* view : views_) {
      view->Detach();
    }
    views_.clear();

    is_detached_ = true;
  } else {
    // TODO(https://crbug.com/763038): See original bug at
    // https://crbug.com/254728. Copying the buffer instead of transferring is
    // not spec compliant but was added for a WebAudio bug fix. The only time
    // this branch is taken is when attempting to transfer an AudioBuffer's
    // channel data ArrayBuffer.
    contents_.CopyTo(result);
    if (!result.Data())
      return false;
  }

  return true;
}

bool ArrayBuffer::ShareContentsWith(ArrayBufferContents& result) {
  DCHECK(IsShared());
  scoped_refptr<ArrayBuffer> keep_alive(this);

  if (!contents_.DataShared()) {
    result.Detach();
    return false;
  }

  contents_.ShareWith(result);
  return true;
}

bool ArrayBuffer::ShareNonSharedForInternalUse(ArrayBufferContents& result) {
  DCHECK(!IsShared());
  scoped_refptr<ArrayBuffer> keep_alive(this);

  if (!contents_.Data()) {
    result.Detach();
    return false;
  }

  contents_.ShareNonSharedForInternalUse(result);
  return true;
}

void ArrayBuffer::AddView(ArrayBufferView* view) {
  views_.insert(view);
}

void ArrayBuffer::RemoveView(ArrayBufferView* view) {
  DCHECK(views_.Contains(view));
  views_.erase(view);
}

}  // namespace blink

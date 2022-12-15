// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_UTILS_H_

#include <cstddef>
#include <memory>

#include "base/check_op.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"

namespace blink {

class ScriptState;

// Extracts the read/write operation size from the buffer size.
int NativeIOOperationSize(const DOMArrayBufferView& buffer);

// Transfers the buffer from source to a new DOMArrayBufferView, preserving the
// its specific type. Returns nullptr when source is not detachable or when the
// transfer fails.
DOMArrayBufferView* TransferToNewArrayBufferView(
    v8::Isolate* isolate,
    NotShared<DOMArrayBufferView> source,
    ExceptionState& exception_state);

// Provides cross-thread access to the buffer backing a DOMArrayBufferView.
//
// This class is necessary because DOMArrayBufferView is garbage-collected,
// which entails that each DOMArrayBufferView instance can only be safely
// accessed on the thread where it was created. Note that CrossThreadPersistent
// can be used to keep a DOMArrayBufferView alive across threads, but the
// instance cannot be safely accessed on a different thread. See the comments on
// cppgc::subtle::CrossThreadPersistent for details.
//
// An instance takes over a DOMArrayBufferView's backing buffer at construction.
// The instance exposes the backing buffer via the Data() and DataLength()
// methods. At some point, the backing buffer is turned back into a
// DOMArrayBufferView via the Take() method. Once Take() is called, the instance
// is invalid, and Data() / DataLength() must not be called anymore.
//
// An instance should be owned by a single sequence at a time. Changing the
// owning sequence should be accomplished by PostTask-ing an owning pointer to
// the instance.
//
// Each instance must be destroyed on the same sequence where it was created.
// Take() must be called on the same sequence where the instance was created.
class NativeIODataBuffer {
 public:
  // Detaches the buffer backing `source`.
  //
  // Returns nullptr if detaching failed.
  static std::unique_ptr<NativeIODataBuffer> Create(
      ScriptState* script_state,
      NotShared<DOMArrayBufferView> source,
      ExceptionState& exception_state);

  // Exposed for std::make_unique. Instances should be obtained from Create().
  NativeIODataBuffer(ArrayBufferContents contents,
                     DOMArrayBufferView::ViewType type,
                     size_t offset,
#if DCHECK_IS_ON()
                     size_t byte_length,
#endif  // DCHECK_IS_ON()
                     size_t length,
                     base::PassKey<NativeIODataBuffer>);

  NativeIODataBuffer(const NativeIODataBuffer&) = delete;
  NativeIODataBuffer& operator=(const NativeIODataBuffer&) = delete;

  ~NativeIODataBuffer();

  // Re-creates the DOMArrayBufferView.
  //
  // Must only be called while this instance is onwed by the same sequence where
  // Create() was called. Must only be called if IsValid() is true.
  // After the call, IsValid() will return false.
  NotShared<DOMArrayBufferView> Take();

  // Exposed for DCHECKs.
  //
  // Can be called while this instance is owned by any sequence.
  bool IsValid() const;

  // Returns a raw pointer to the DOMArrayBufferView's view.
  //
  // The return type was chosen so that the raw pointer can be conveniently
  // passed to base::File methods.
  //
  // Can be called while this instance is owned by any sequence. Must only be
  // called if IsValid() is true.
  char* Data() {
    DCHECK(IsValid());

    // An invalid ArrayBufferContents (backing an empty array) returns nullptr
    // when Data() is called. However, in that case, the offset must be zero.
    DCHECK(contents_.Data() || contents_.DataLength() == 0);
    DCHECK(contents_.Data() || offset_ == 0);

    // According to the DCHECKs above, this branch isn't strictly needed. The
    // return statement below the branch will never do pointer arithmetic on
    // nullptr, because `offset_` is guaranteed to be zero when
    // the ArrayBufferContents is not valid but this instance is.
    char* data = static_cast<char*>(contents_.Data());
    if (!data) {
      DCHECK_EQ(offset_, 0u);
      return data;
    }

    return data + offset_;
  }

#if DCHECK_IS_ON()
  // Returns the size of the DOMArrayBufferView's view, in bytes.
  //
  // Exposed for DCHECKs around base::File calls.
  //
  // Can be called while this instance is owned by any sequence. Must only be
  // called if IsValid() is true.
  size_t DataLength() const {
    DCHECK(IsValid());
    return byte_length_;
  }
#endif  // DCHECK_IS_ON()

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // May not be valid, as reported by ArrayBufferContents::IsValid().
  //
  // If valid, guaranteed not to be shared, as reported by
  // ArrayBufferContents::IsShared().
  ArrayBufferContents contents_;

  DOMArrayBufferView::ViewType type_;
  const size_t offset_;
#if DCHECK_IS_ON()
  const size_t byte_length_;
#endif  // DCHECK_IS_ON()
  const size_t length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_UTILS_H_

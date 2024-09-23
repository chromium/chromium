// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_SCOPED_D3D_BUFFERS_H_
#define MEDIA_GPU_WINDOWS_SCOPED_D3D_BUFFERS_H_

#include <Windows.h>

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// The base class for a scoped buffer, which do buffer allocation in the
// constructor and release the buffer in Commit() or destructor. In case of
// any failure, the empty() should be true.
class MEDIA_GPU_EXPORT ScopedD3DBuffer {
 public:
  explicit ScopedD3DBuffer(base::span<uint8_t> data = {});
  virtual ~ScopedD3DBuffer();
  uint8_t* data() { return data_.data(); }
  size_t size() const { return data_.size(); }
  bool empty() const { return data_.empty(); }

  // Declare that we have done the access to the buffer. It will also
  // automatically be called when this object destructs. In a
  // ScopedD3D11DecoderBuffer implementation, the underlying decoder buffer
  // will be released and corresponding decoder buffer description will be
  // created. Returns whether the operation succeeds.
  virtual bool Commit() = 0;

  // Same with Commit(), but declares only |written_size| bytes are used.
  virtual bool Commit(uint32_t written_size) = 0;

 protected:
  base::raw_span<uint8_t, DanglingUntriaged> data_;
};

// This class provides basic interface for getting the buffer's attribute.
class MEDIA_GPU_EXPORT D3DInputBuffer {
 public:
  explicit D3DInputBuffer(std::unique_ptr<ScopedD3DBuffer> buffer);
  virtual ~D3DInputBuffer();
  size_t size() const { return buffer_->size(); }
  bool empty() const { return buffer_->empty(); }
  [[nodiscard]] virtual bool Commit();

 protected:
  std::unique_ptr<ScopedD3DBuffer> buffer_;
};

// A random accessible buffer, which provides a data pointer for accessing at
// arbitrary location.
class MEDIA_GPU_EXPORT ScopedRandomAccessD3DInputBuffer
    : public D3DInputBuffer {
 public:
  explicit ScopedRandomAccessD3DInputBuffer(
      std::unique_ptr<ScopedD3DBuffer> buffer)
      : D3DInputBuffer(std::move(buffer)) {}
  ~ScopedRandomAccessD3DInputBuffer() override = default;
  uint8_t* data() const { return buffer_->data(); }
};

// A sequence buffer, which provides only a sequential Write() method like a
// output stream.
class MEDIA_GPU_EXPORT ScopedSequenceD3DInputBuffer : public D3DInputBuffer {
 public:
  explicit ScopedSequenceD3DInputBuffer(std::unique_ptr<ScopedD3DBuffer> buffer)
      : D3DInputBuffer(std::move(buffer)) {}
  ~ScopedSequenceD3DInputBuffer() override;
  size_t BytesWritten() const { return offset_; }
  size_t BytesAvailable() const;
  // Fill the buffer with as much bytes as possible, and return the number of
  // bytes written in this call.
  [[nodiscard]] size_t Write(base::span<const uint8_t> source);
  [[nodiscard]] bool Commit() override;

 private:
  size_t offset_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_SCOPED_D3D_BUFFERS_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_BUFFER_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_BUFFER_H_

#include <memory>

#include <va/va.h>

namespace media::internal {

// Class used for tracking a VABuffer and all information relevant to it. All
// objects of this class are immutable in the sense that none of the members
// change in value throughout the lifetime of the object. However, the
// underlying buffer data may be changed by users of a FakeBuffer by using the
// pointer returned by GetData(). Such changes must be synchronized
// externally, but calls to the FakeBuffer public methods themselves are
// thread-safe. Users of FakeBuffer must not free the memory pointed to by
// the pointer that GetData() returns.
class FakeBuffer {
 public:
  using IdType = VABufferID;

  FakeBuffer(IdType id,
             VAContextID context,
             VABufferType type,
             unsigned int size_per_element,
             unsigned int num_elements,
             const void* data);
  FakeBuffer(const FakeBuffer&) = delete;
  FakeBuffer& operator=(const FakeBuffer&) = delete;
  ~FakeBuffer();

  IdType GetID() const;
  VAContextID GetContextID() const;
  VABufferType GetType() const;
  size_t GetDataSize() const;
  void* GetData() const;

 private:
  const IdType id_;
  const VAContextID context_;
  const VABufferType type_;
  const size_t data_size_;
  const std::unique_ptr<uint8_t[]> data_;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_BUFFER_H_

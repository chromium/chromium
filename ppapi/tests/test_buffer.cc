// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_buffer.h"

#include <stdint.h>

#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Buffer);

bool TestBuffer::Init() {
  buffer_interface_ = static_cast<const PPB_Buffer_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_BUFFER_DEV_INTERFACE));
  return !!buffer_interface_;
}

void TestBuffer::RunTests(const std::string& filter) {
  RUN_TEST(InvalidSize, filter);
  RUN_TEST(InitToZero, filter);
  RUN_TEST(IsBuffer, filter);
  RUN_TEST(BasicLifeCycle, filter);
}

std::string TestBuffer::TestInvalidSize() {
  pp::Buffer_Dev zero_size(instance_, 0);
  if (!zero_size.is_null())
    return "Zero size accepted";

  PASS();
}

std::string TestBuffer::TestInitToZero() {
  pp::Buffer_Dev buffer(instance_, 100);
  if (buffer.is_null())
    return "Could not create buffer";

  if (buffer.size() != 100)
    return "Buffer size not as expected";

  // Now check that everything is 0.
  unsigned char* bytes = static_cast<unsigned char *>(buffer.data());
  for (uint32_t index = 0; index < buffer.size(); index++) {
    if (bytes[index] != 0)
      return "Buffer isn't entirely zero";
  }

  PASS();
}

std::string TestBuffer::TestIsBuffer() {
  // Test that a NULL resource isn't a buffer.
  pp::Resource null_resource;
  if (buffer_interface_->IsBuffer(null_resource.pp_resource()))
    return "Null resource was reported as a valid buffer";

  // Make another resource type and test it.
  const int w = 16, h = 16;
  pp::Graphics2D device(instance_, pp::Size(w, h), true);
  if (device.is_null())
    return "Couldn't create device context";
  if (buffer_interface_->IsBuffer(device.pp_resource()))
    return "Device context was reported as a buffer";

  // Make a valid buffer.
  pp::Buffer_Dev buffer(instance_, 100);
  if (buffer.is_null())
    return "Couldn't create buffer";
  if (!buffer_interface_->IsBuffer(buffer.pp_resource()))
    return "Buffer should be identified as a buffer";

  PASS();
}

std::string TestBuffer::TestBasicLifeCycle() {
  enum { kBufferSize = 100 };

  pp::Buffer_Dev *buffer = new pp::Buffer_Dev(instance_, kBufferSize);
  if (buffer->is_null() ||
      !buffer_interface_->IsBuffer(buffer->pp_resource()) ||
      buffer->size() != kBufferSize) {
    return "Error creating buffer (earlier test should have failed)";
  }

  // Test that the buffer got created & mapped.
  if (buffer->data() == NULL)
    return "Failed to Map() buffer";

  // Test that the buffer is writeable.
  char* data = static_cast<char*>(buffer->data());
  for (int i = 0; i < kBufferSize; ++i)
    data[i] = 'X';

  // Implicitly test that the copy constructor doesn't cause a double-unmap on
  // delete.
  pp::Buffer_Dev* copy = new pp::Buffer_Dev(*buffer);

  // Implicitly test that destroying the buffer doesn't encounter a fatal error
  // in Unmap.
  delete buffer;

  // Test that we can still write to copy's copy of the data.
  char* copy_data = static_cast<char*>(copy->data());
  for (int i = 0; i < kBufferSize; ++i)
    copy_data[i] = 'Y';

  delete copy;

  PASS();
}

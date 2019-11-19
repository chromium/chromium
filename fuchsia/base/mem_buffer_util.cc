// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/mem_buffer_util.h"

#include <lib/fdio/io.h>

#include <lib/zx/vmo.h>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"

namespace cr_fuchsia {

bool ReadUTF8FromVMOAsUTF16(const fuchsia::mem::Buffer& buffer,
                            base::string16* output) {
  std::string output_utf8;
  if (!StringFromMemBuffer(buffer, &output_utf8))
    return false;
  return base::UTF8ToUTF16(&output_utf8.front(), output_utf8.size(), output);
}

fuchsia::mem::Buffer MemBufferFromString(base::StringPiece data,
                                         base::StringPiece name) {
  fuchsia::mem::Buffer buffer;

  zx_status_t status = zx::vmo::create(data.size(), 0, &buffer.vmo);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_create";

  status = buffer.vmo.set_property(ZX_PROP_NAME, name.data(), name.size());
  ZX_DCHECK(status == ZX_OK, status);

  status = buffer.vmo.write(data.data(), 0, data.size());
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_write";

  buffer.size = data.size();
  return buffer;
}

fuchsia::mem::Buffer MemBufferFromString16(const base::StringPiece16& data,
                                           base::StringPiece name) {
  return MemBufferFromString(
      base::StringPiece(reinterpret_cast<const char*>(data.data()),
                        data.size() * sizeof(base::char16)),
      name);
}

bool StringFromMemBuffer(const fuchsia::mem::Buffer& buffer,
                         std::string* output) {
  output->resize(buffer.size);
  zx_status_t status = buffer.vmo.read(&output->at(0), 0, buffer.size);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_vmo_read";
    return false;
  }
  return true;
}

fuchsia::mem::Buffer MemBufferFromFile(base::File file) {
  if (!file.IsValid())
    return {};

  zx::vmo vmo;
  zx_status_t status =
      fdio_get_vmo_copy(file.GetPlatformFile(), vmo.reset_and_get_address());
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "fdio_get_vmo_copy";
    return {};
  }

  fuchsia::mem::Buffer output;
  output.vmo = std::move(vmo);
  output.size = file.GetLength();
  return output;
}

fuchsia::mem::Buffer CloneBuffer(const fuchsia::mem::Buffer& buffer,
                                 base::StringPiece name) {
  fuchsia::mem::Buffer output;
  output.size = buffer.size;
  zx_status_t status = buffer.vmo.create_child(ZX_VMO_CHILD_COPY_ON_WRITE, 0,
                                               buffer.size, &output.vmo);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_create_child";

  status = output.vmo.set_property(ZX_PROP_NAME, name.data(), name.size());
  ZX_DCHECK(status == ZX_OK, status);

  return output;
}

}  // namespace cr_fuchsia

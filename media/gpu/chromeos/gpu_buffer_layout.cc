// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/gpu_buffer_layout.h"

#include <sstream>

#include "media/gpu/macros.h"

namespace media {

namespace {

template <class T>
std::string VectorToString(const std::vector<T>& vec) {
  std::ostringstream result;
  std::string delim;
  result << "[";
  for (auto& v : vec) {
    result << delim << v;
    if (delim.size() == 0)
      delim = ", ";
  }
  result << "]";
  return result.str();
}

}  // namespace

// static
base::Optional<GpuBufferLayout> GpuBufferLayout::Create(
    const Fourcc& fourcc,
    const gfx::Size& size,
    const std::vector<ColorPlaneLayout>& planes) {
  // TODO(akahuang): Check planes.size() is equal to the expected value
  // according to |fourcc|.
  if (!fourcc || size.IsEmpty() || planes.size() == 0) {
    VLOGF(1) << "Invalid parameters. fourcc: " << fourcc.ToString()
             << ", size: " << size.ToString()
             << ", planes: " << VectorToString(planes);
    return base::nullopt;
  }

  return GpuBufferLayout(fourcc, size, planes);
}

GpuBufferLayout::GpuBufferLayout(const Fourcc& fourcc,
                                 const gfx::Size& size,
                                 const std::vector<ColorPlaneLayout>& planes)
    : fourcc_(fourcc), size_(size), planes_(planes) {}

GpuBufferLayout::~GpuBufferLayout() = default;
GpuBufferLayout::GpuBufferLayout(const GpuBufferLayout&) = default;
GpuBufferLayout::GpuBufferLayout(GpuBufferLayout&&) = default;
GpuBufferLayout& GpuBufferLayout::operator=(const GpuBufferLayout& other) =
    default;

bool GpuBufferLayout::operator==(const GpuBufferLayout& rhs) const {
  return fourcc_ == rhs.fourcc_ && size_ == rhs.size_ && planes_ == rhs.planes_;
}

bool GpuBufferLayout::operator!=(const GpuBufferLayout& rhs) const {
  return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& ostream, const GpuBufferLayout& layout) {
  ostream << "GpuBufferLayout(fourcc: " << layout.fourcc().ToString()
          << ", size: " << layout.size().ToString()
          << ", planes (stride, offset, size): "
          << VectorToString(layout.planes());
  return ostream;
}

}  // namespace media

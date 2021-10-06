// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"

#include <linux/media.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <unordered_map>

#include "base/containers/contains.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/video_types.h"

namespace media {

namespace v4l2_test {

constexpr int kIoctlOk = 0;

static const base::FilePath kDecodeDevice("/dev/video-dec0");
static const base::FilePath kMediaDevice("/dev/media0");

#define V4L2_REQUEST_CODE_AND_STRING(x) \
  { x, #x }

// This map maintains a table with pairs of V4L2 request code
// and corresponding name. New pair has to be added here
// when new V4L2 request code has to be used.
static const std::unordered_map<int, std::string>
    kMapFromV4L2RequestCodeToString = {
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_QUERYCAP),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_ENUM_FMT),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_ENUM_FRAMESIZES),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_S_FMT),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_G_FMT),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_TRY_FMT),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_REQBUFS),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_QUERYBUF),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_QBUF),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_STREAMON),
        V4L2_REQUEST_CODE_AND_STRING(MEDIA_IOC_REQUEST_ALLOC)};

// Finds corresponding defined V4L2 request code name
// for a given V4L2 request code value.
std::string V4L2RequestCodeToString(int request_code) {
  DCHECK(base::Contains(kMapFromV4L2RequestCodeToString, request_code));

  const auto& request_code_pair =
      kMapFromV4L2RequestCodeToString.find(request_code);

  return request_code_pair->second;
}

// Returns buffer type name (OUTPUT or CAPTURE) given v4l2_buf_type |type|.
std::ostream& operator<<(std::ostream& ostream,
                         const enum v4l2_buf_type& type) {
  ostream << ((type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) ? "OUTPUT"
                                                          : "CAPTURE");

  return ostream;
}

// Returns resolution and number of planes given |pix_mp|.
std::ostream& operator<<(std::ostream& ostream,
                         const struct v4l2_pix_format_mplane& pix_mp) {
  ostream << pix_mp.width << " x " << pix_mp.height
          << ", num_planes = " << static_cast<size_t>(pix_mp.num_planes) << ".";

  return ostream;
}

// Logs whether given ioctl request |request_code| succeeded
// or failed given |ret|.
void LogIoctlResult(int ret, int request_code) {
  PLOG_IF(ERROR, ret != kIoctlOk)
      << "Ioctl request failed for " << V4L2RequestCodeToString(request_code)
      << ".";
  VLOG_IF(4, ret == kIoctlOk)
      << V4L2RequestCodeToString(request_code) << " succeeded.";
}

V4L2Queue::V4L2Queue(enum v4l2_buf_type type,
                     uint32_t fourcc,
                     const gfx::Size& size,
                     uint32_t num_planes,
                     enum v4l2_memory memory)
    : type_(type),
      fourcc_(fourcc),
      display_size_(size),
      num_planes_(num_planes),
      memory_(memory) {}

V4L2Queue::~V4L2Queue() = default;

V4L2IoctlShim::V4L2IoctlShim()
    : decode_fd_(kDecodeDevice,
                 base::File::FLAG_OPEN | base::File::FLAG_READ |
                     base::File::FLAG_WRITE),
      media_fd_(kMediaDevice,
                base::File::FLAG_OPEN | base::File::FLAG_READ |
                    base::File::FLAG_WRITE) {
  PCHECK(decode_fd_.IsValid()) << "Failed to open " << kDecodeDevice;
  PCHECK(media_fd_.IsValid()) << "Failed to open " << kMediaDevice;
}

V4L2IoctlShim::~V4L2IoctlShim() = default;

template <typename T>
bool V4L2IoctlShim::Ioctl(int request_code, T* argp) const {
  NOTREACHED() << "Please add a specialized function for the given V4L2 ioctl "
                  "request code.";
  return !kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code, struct v4l2_capability* cap) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_QUERYCAP));
  LOG_ASSERT(cap != nullptr) << "|cap| check failed.\n";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, cap);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct v4l2_fmtdesc* fmtdesc) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_ENUM_FMT));
  LOG_ASSERT(fmtdesc != nullptr) << "|fmtdesc| check failed.\n";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, fmtdesc);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct v4l2_frmsizeenum* frame_size) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_ENUM_FRAMESIZES));
  LOG_ASSERT(frame_size != nullptr) << "|frame_size| check failed.\n";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, frame_size);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code, struct v4l2_format* fmt) const {
  DCHECK(request_code == static_cast<int>(VIDIOC_S_FMT) ||
         request_code == static_cast<int>(VIDIOC_G_FMT) ||
         request_code == static_cast<int>(VIDIOC_TRY_FMT));
  LOG_ASSERT(fmt != nullptr) << "|fmt| check failed.\n";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, fmt);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct v4l2_requestbuffers* reqbuf) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_REQBUFS));
  LOG_ASSERT(reqbuf != nullptr) << "|reqbuf| check failed.\n";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, reqbuf);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code, struct v4l2_buffer* buffer) const {
  DCHECK(request_code == static_cast<int>(VIDIOC_QUERYBUF) ||
         request_code == static_cast<int>(VIDIOC_QBUF));
  LOG_ASSERT(buffer != nullptr) << "|buffer| check failed.\n";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, buffer);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code, int* arg) const {
  DCHECK(request_code == static_cast<int>(VIDIOC_STREAMON) ||
         request_code == static_cast<int>(MEDIA_IOC_REQUEST_ALLOC));
  LOG_ASSERT(arg != nullptr) << "|arg| check failed.\n";

  base::PlatformFile ioctl_fd;

  if (request_code == static_cast<int>(MEDIA_IOC_REQUEST_ALLOC))
    ioctl_fd = media_fd_.GetPlatformFile();
  else
    ioctl_fd = decode_fd_.GetPlatformFile();

  const int ret = ioctl(ioctl_fd, request_code, arg);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

bool V4L2IoctlShim::EnumFrameSizes(uint32_t fourcc) const {
  struct v4l2_frmsizeenum frame_size;

  memset(&frame_size, 0, sizeof(frame_size));
  frame_size.pixel_format = fourcc;

  return Ioctl(VIDIOC_ENUM_FRAMESIZES, &frame_size);
}

bool V4L2IoctlShim::SetFmt(const std::unique_ptr<V4L2Queue>& queue) const {
  struct v4l2_format fmt;

  memset(&fmt, 0, sizeof(fmt));
  fmt.type = queue->type();
  fmt.fmt.pix_mp.pixelformat = queue->fourcc();
  if (queue->type() == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    constexpr size_t kInputBufferMaxSize = 4 * 1024 * 1024;
    fmt.fmt.pix_mp.plane_fmt[0].sizeimage = kInputBufferMaxSize;
  }

  fmt.fmt.pix_mp.num_planes = queue->num_planes();
  fmt.fmt.pix_mp.width = queue->display_size().width();
  fmt.fmt.pix_mp.height = queue->display_size().height();

  const bool ret = Ioctl(VIDIOC_S_FMT, &fmt);

  LOG(INFO) << queue->type() << " - VIDIOC_S_FMT: " << fmt.fmt.pix_mp;

  return ret;
}

bool V4L2IoctlShim::GetFmt(const enum v4l2_buf_type type,
                           gfx::Size* coded_size,
                           uint32_t* num_planes) const {
  struct v4l2_format fmt;

  memset(&fmt, 0, sizeof(fmt));
  fmt.type = type;

  const bool ret = Ioctl(VIDIOC_G_FMT, &fmt);

  coded_size->SetSize(fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height);
  *num_planes = fmt.fmt.pix_mp.num_planes;

  LOG(INFO) << type << " - VIDIOC_G_FMT: " << fmt.fmt.pix_mp;

  return ret;
}

bool V4L2IoctlShim::TryFmt(const std::unique_ptr<V4L2Queue>& queue) const {
  struct v4l2_format fmt;

  memset(&fmt, 0, sizeof(fmt));
  fmt.type = queue->type();
  fmt.fmt.pix_mp.pixelformat = queue->fourcc();
  fmt.fmt.pix_mp.num_planes = queue->num_planes();
  fmt.fmt.pix_mp.width = queue->coded_size().width();
  fmt.fmt.pix_mp.height = queue->coded_size().height();

  const bool ret = Ioctl(VIDIOC_TRY_FMT, &fmt);

  LOG(INFO) << queue->type() << " - VIDIOC_TRY_FMT: " << fmt.fmt.pix_mp;

  return ret;
}

bool V4L2IoctlShim::ReqBufs(const std::unique_ptr<V4L2Queue>& queue) const {
  struct v4l2_requestbuffers reqbuf;

  memset(&reqbuf, 0, sizeof(reqbuf));
  reqbuf.count = kRequestBufferCount;
  reqbuf.type = queue->type();
  reqbuf.memory = queue->memory();

  const bool ret = Ioctl(VIDIOC_REQBUFS, &reqbuf);

  LOG(INFO) << kRequestBufferCount << " buffers requested, " << reqbuf.count
            << " buffers returned for " << queue->type() << ".";

  return ret;
}

bool V4L2IoctlShim::QBuf(const std::unique_ptr<V4L2Queue>& queue,
                         const uint32_t index) const {
  LOG_ASSERT(queue->memory() == V4L2_MEMORY_MMAP)
      << "Only V4L2_MEMORY_MMAP is currently supported.\n";

  struct v4l2_buffer v4l2_buffer;
  std::vector<v4l2_plane> planes(VIDEO_MAX_PLANES);

  memset(&v4l2_buffer, 0, sizeof v4l2_buffer);
  v4l2_buffer.type = queue->type();
  v4l2_buffer.memory = queue->memory();
  v4l2_buffer.index = index;
  v4l2_buffer.m.planes = planes.data();
  v4l2_buffer.length = queue->num_planes();

  MmapedBuffers buffers = queue->buffers();

  for (uint32_t i = 0; i < queue->num_planes(); ++i) {
    v4l2_buffer.m.planes[i].length = buffers[index][i].length;
    v4l2_buffer.m.planes[i].bytesused = buffers[index][i].length;
    v4l2_buffer.m.planes[i].data_offset = 0;
  }

  return Ioctl(VIDIOC_QBUF, &v4l2_buffer);
}

bool V4L2IoctlShim::StreamOn(const enum v4l2_buf_type type) const {
  int arg = static_cast<int>(type);

  return Ioctl(VIDIOC_STREAMON, &arg);
}

bool V4L2IoctlShim::MediaIocRequestAlloc() const {
  // TODO(stevecho): need to use the file descriptor representing the request
  // for MEDIA_REQUEST_IOC_QUEUE() call to queue to the request.
  int req_fd;

  return Ioctl(MEDIA_IOC_REQUEST_ALLOC, &req_fd);
}

bool V4L2IoctlShim::QueryFormat(enum v4l2_buf_type type,
                                uint32_t fourcc) const {
  struct v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = type;

  while (Ioctl(VIDIOC_ENUM_FMT, &fmtdesc)) {
    if (fourcc == fmtdesc.pixelformat)
      return true;

    fmtdesc.index++;
  }

  return false;
}

bool V4L2IoctlShim::VerifyCapabilities(uint32_t compressed_format,
                                       uint32_t uncompressed_format) const {
  struct v4l2_capability cap;
  memset(&cap, 0, sizeof(cap));

  DCHECK(Ioctl(VIDIOC_QUERYCAP, &cap));

  LOG(INFO) << "Driver=\"" << cap.driver << "\" bus_info=\"" << cap.bus_info
            << "\" card=\"" << cap.card;

  const bool is_compressed_format_supported =
      QueryFormat(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, compressed_format);

  LOG_IF(ERROR, !is_compressed_format_supported)
      << media::FourccToString(compressed_format)
      << " is not a supported compressed OUTPUT format.";

  const bool is_uncompressed_format_supported =
      QueryFormat(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, uncompressed_format);

  LOG_IF(ERROR, !is_uncompressed_format_supported)
      << media::FourccToString(uncompressed_format)
      << " is not a supported uncompressed CAPTURE format.";

  return is_compressed_format_supported && is_uncompressed_format_supported;
}

bool V4L2IoctlShim::QueryAndMmapQueueBuffers(
    const std::unique_ptr<V4L2Queue>& queue) const {
  DCHECK_EQ(queue->memory(), V4L2_MEMORY_MMAP);

  auto buffers = queue->buffers();

  for (uint32_t i = 0; i < kRequestBufferCount; ++i) {
    struct v4l2_buffer v4l_buffer;
    std::vector<v4l2_plane> planes(VIDEO_MAX_PLANES);

    memset(&v4l_buffer, 0, sizeof(v4l_buffer));
    v4l_buffer.type = queue->type();
    v4l_buffer.memory = queue->memory();
    v4l_buffer.index = i;
    v4l_buffer.length = queue->num_planes();
    v4l_buffer.m.planes = planes.data();

    DCHECK(Ioctl(VIDIOC_QUERYBUF, &v4l_buffer));

    for (uint32_t j = 0; j < queue->num_planes(); ++j) {
      buffers[i][j].length = v4l_buffer.m.planes[j].length;
      buffers[i][j].start =
          mmap(NULL, v4l_buffer.m.planes[j].length, PROT_READ | PROT_WRITE,
               MAP_SHARED, decode_fd_.GetPlatformFile(),
               v4l_buffer.m.planes[j].m.mem_offset);
      if (buffers[i][j].start == MAP_FAILED) {
        LOG(ERROR) << "Failed to mmap buffer of length("
                   << v4l_buffer.m.planes[j].length << ") and offset("
                   << std::hex << v4l_buffer.m.planes[j].m.mem_offset << ").";

        return false;
      }
    }
  }

  return true;
}

}  // namespace v4l2_test
}  // namespace media
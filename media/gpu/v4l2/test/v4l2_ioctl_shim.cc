// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"

#include <fcntl.h>
#include <linux/media.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <unordered_map>

#include "base/containers/contains.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/video_types.h"

namespace media {

namespace v4l2_test {

constexpr int kIoctlOk = 0;
// |kMaxRetryCount = 2^24| takes around 20 seconds to exhaust all retries on
// Trogdor when a decode stalls.
constexpr int kMaxRetryCount = 1 << 24;

#define V4L2_REQUEST_CODE_AND_STRING(x) \
  { x, #x }

constexpr uint32_t kMaximumDeviceNumber = 10;

constexpr std::string_view kDecoderDevicePrefix = "/dev/video-dec";
constexpr std::string_view kMediaDevicePrefix = "/dev/media-dec";

// This map maintains a table with pairs of V4L2 request code
// and corresponding name. New pair has to be added here
// when new V4L2 request code has to be used.
static const std::unordered_map<int, std::string>
    kMapFromV4L2RequestCodeToString = {
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_QUERYCAP),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_QUERYCTRL),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_ENUM_FMT),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_ENUM_FRAMESIZES),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_S_FMT),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_G_FMT),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_TRY_FMT),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_REQBUFS),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_QUERYBUF),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_QBUF),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_DQBUF),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_STREAMON),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_STREAMOFF),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_S_EXT_CTRLS),
        V4L2_REQUEST_CODE_AND_STRING(MEDIA_IOC_REQUEST_ALLOC),
        V4L2_REQUEST_CODE_AND_STRING(MEDIA_REQUEST_IOC_QUEUE),
        V4L2_REQUEST_CODE_AND_STRING(MEDIA_REQUEST_IOC_REINIT)};

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
  PLOG_IF(ERROR, ret != kIoctlOk && errno != EAGAIN)
      << "Ioctl request failed for " << V4L2RequestCodeToString(request_code)
      << ".";
  PLOG_IF(INFO, ret != kIoctlOk && errno == EAGAIN)
      << "Ioctl request failed for " << V4L2RequestCodeToString(request_code)
      << "with error code EAGAIN.";
  VLOG_IF(4, ret == kIoctlOk)
      << V4L2RequestCodeToString(request_code) << " succeeded.";
}

MmapedBuffer::MmapedBuffer(const base::PlatformFile ioctl_fd,
                           const struct v4l2_buffer& v4l2_buffer)
    : num_planes_(v4l2_buffer.length) {
  for (uint32_t i = 0; i < num_planes_; ++i) {
    void* start_addr =
        mmap(NULL, v4l2_buffer.m.planes[i].length, PROT_READ | PROT_WRITE,
             MAP_SHARED, ioctl_fd, v4l2_buffer.m.planes[i].m.mem_offset);

    LOG_IF(FATAL, start_addr == MAP_FAILED)
        << "Failed to mmap buffer of length(" << v4l2_buffer.m.planes[i].length
        << ") and offset(" << std::hex << v4l2_buffer.m.planes[i].m.mem_offset
        << ").";

    mmaped_planes_.emplace_back(start_addr, v4l2_buffer.m.planes[i].length);
  }
}

MmapedBuffer::~MmapedBuffer() {
  for (const auto& [start_addr, length, bytes_used] : mmaped_planes_)
    munmap(start_addr, length);
}

V4L2Queue::V4L2Queue(enum v4l2_buf_type type,
                     uint32_t fourcc,
                     const gfx::Size& size,
                     uint32_t num_planes,
                     enum v4l2_memory memory,
                     uint32_t num_buffers)
    : type_(type),
      fourcc_(fourcc),
      num_buffers_(num_buffers),
      display_size_(size),
      num_planes_(num_planes),
      memory_(memory) {}

V4L2Queue::~V4L2Queue() = default;

scoped_refptr<MmapedBuffer> V4L2Queue::GetBuffer(const size_t index) const {
  DCHECK_LT(index, buffers_.size());

  return buffers_[index];
}

V4L2IoctlShim::V4L2IoctlShim(const uint32_t coded_fourcc) {
  uint32_t i;

  for (i = 0; i < kMaximumDeviceNumber; ++i) {
    std::string path =
        std::string(kDecoderDevicePrefix) + base::NumberToString(i);
    decode_fd_ = base::File(base::FilePath(path), base::File::FLAG_OPEN |
                                                      base::File::FLAG_READ |
                                                      base::File::FLAG_WRITE);

    // Check if the device supports the requested coded format
    if (QueryFormat(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, coded_fourcc))
      break;

    // Close file descriptor on failure
    decode_fd_.Close();
  }

  // TODO(wenst) media devices should be matched by their driver name or bus
  // info
  media_fd_ = base::File(
      base::FilePath(std::string(kMediaDevicePrefix) + base::NumberToString(i)),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);

  PCHECK(decode_fd_.IsValid()) << "Failed to find available decode device.";
  PCHECK(media_fd_.IsValid()) << "Failed to find available media device.";
}

V4L2IoctlShim::~V4L2IoctlShim() = default;

template <typename T>
bool V4L2IoctlShim::Ioctl(int request_code, T arg) const {
  NOTREACHED() << "Please add a specialized function for the given V4L2 ioctl "
                  "request code.";
  return !kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code, struct v4l2_capability* cap) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_QUERYCAP));
  LOG_ASSERT(cap != nullptr) << "|cap| check failed.";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, cap);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct v4l2_queryctrl* query_ctrl) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_QUERYCTRL));
  LOG_ASSERT(query_ctrl != nullptr) << "|query_ctrl| check failed.";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, query_ctrl);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct v4l2_fmtdesc* fmtdesc) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_ENUM_FMT));
  LOG_ASSERT(fmtdesc != nullptr) << "|fmtdesc| check failed.";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, fmtdesc);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct v4l2_frmsizeenum* frame_size) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_ENUM_FRAMESIZES));
  LOG_ASSERT(frame_size != nullptr) << "|frame_size| check failed.";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, frame_size);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code, struct v4l2_format* fmt) const {
  DCHECK(request_code == static_cast<int>(VIDIOC_S_FMT) ||
         request_code == static_cast<int>(VIDIOC_G_FMT) ||
         request_code == static_cast<int>(VIDIOC_TRY_FMT));
  LOG_ASSERT(fmt != nullptr) << "|fmt| check failed.";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, fmt);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct v4l2_requestbuffers* reqbuf) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_REQBUFS));
  LOG_ASSERT(reqbuf != nullptr) << "|reqbuf| check failed.";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, reqbuf);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code, struct v4l2_buffer* buffer) const {
  DCHECK(request_code == static_cast<int>(VIDIOC_QUERYBUF) ||
         request_code == static_cast<int>(VIDIOC_QBUF) ||
         request_code == static_cast<int>(VIDIOC_DQBUF));
  LOG_ASSERT(buffer != nullptr) << "|buffer| check failed.";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, buffer);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code, int* arg) const {
  DCHECK(request_code == static_cast<int>(VIDIOC_STREAMON) ||
         request_code == static_cast<int>(VIDIOC_STREAMOFF) ||
         request_code == static_cast<int>(MEDIA_IOC_REQUEST_ALLOC));
  LOG_ASSERT(arg != nullptr) << "|arg| check failed.";

  base::PlatformFile ioctl_fd;

  if (request_code == static_cast<int>(MEDIA_IOC_REQUEST_ALLOC))
    ioctl_fd = media_fd_.GetPlatformFile();
  else
    ioctl_fd = decode_fd_.GetPlatformFile();

  const int ret = ioctl(ioctl_fd, request_code, arg);

  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code, int arg) const {
  DCHECK(request_code == static_cast<int>(MEDIA_REQUEST_IOC_QUEUE) ||
         request_code == static_cast<int>(MEDIA_REQUEST_IOC_REINIT));

  const int ret = ioctl(arg, request_code);

  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct v4l2_ext_controls* ctrls) const {
  DCHECK(request_code == static_cast<int>(VIDIOC_S_EXT_CTRLS));
  LOG_ASSERT(ctrls != nullptr) << "|ctrls| check failed.";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, ctrls);
  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

bool V4L2IoctlShim::QueryCtrl(const uint32_t ctrl_id) const {
  struct v4l2_queryctrl query_ctrl;

  memset(&query_ctrl, 0, sizeof(query_ctrl));
  query_ctrl.id = ctrl_id;

  return Ioctl(VIDIOC_QUERYCTRL, &query_ctrl);
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

bool V4L2IoctlShim::ReqBufs(std::unique_ptr<V4L2Queue>& queue) const {
  struct v4l2_requestbuffers reqbuf;

  memset(&reqbuf, 0, sizeof(reqbuf));
  reqbuf.count = queue->num_buffers();
  reqbuf.type = queue->type();
  reqbuf.memory = queue->memory();

  const bool ret = Ioctl(VIDIOC_REQBUFS, &reqbuf);

  queue->set_num_buffers(reqbuf.count);

  LOG(INFO) << queue->num_buffers() << " buffers requested, " << reqbuf.count
            << " buffers returned for " << queue->type() << ".";

  return ret;
}

bool V4L2IoctlShim::ReqBufsWithCount(std::unique_ptr<V4L2Queue>& queue,
                                     uint32_t count) const {
  struct v4l2_requestbuffers reqbuf;

  memset(&reqbuf, 0, sizeof(reqbuf));
  reqbuf.count = count;
  reqbuf.type = queue->type();
  reqbuf.memory = queue->memory();

  const bool ret = Ioctl(VIDIOC_REQBUFS, &reqbuf);

  queue->set_num_buffers(reqbuf.count);

  if (count == 0) {
    LOG(INFO) << "Requested to free all buffers in " << queue->type()
              << "with a buffer count of 0.";
  } else {
    LOG(INFO) << queue->num_buffers() << " buffers requested, " << reqbuf.count
              << " buffers returned for " << queue->type() << ".";
  }

  return ret;
}

bool V4L2IoctlShim::QBuf(const std::unique_ptr<V4L2Queue>& queue,
                         const uint32_t index) const {
  LOG_ASSERT(queue->memory() == V4L2_MEMORY_MMAP)
      << "Only V4L2_MEMORY_MMAP is currently supported.";

  struct v4l2_buffer v4l2_buffer;
  std::vector<v4l2_plane> planes(VIDEO_MAX_PLANES);

  memset(&v4l2_buffer, 0, sizeof v4l2_buffer);
  v4l2_buffer.type = queue->type();
  v4l2_buffer.memory = queue->memory();
  v4l2_buffer.index = index;
  v4l2_buffer.m.planes = planes.data();
  v4l2_buffer.length = queue->num_planes();

  scoped_refptr<MmapedBuffer> buffer = queue->GetBuffer(index);

  for (uint32_t i = 0; i < queue->num_planes(); ++i) {
    v4l2_buffer.m.planes[i].length = buffer->mmaped_planes()[i].length;
    v4l2_buffer.m.planes[i].bytesused = buffer->mmaped_planes()[i].bytes_used;
    v4l2_buffer.m.planes[i].data_offset = 0;
  }

  // Request API related setting is needed only for OUTPUT queue.
  if (queue->type() == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    v4l2_buffer.flags |= V4L2_BUF_FLAG_REQUEST_FD;
    v4l2_buffer.request_fd = queue->media_request_fd();
    v4l2_buffer.timestamp.tv_usec =
        base::checked_cast<__suseconds_t>(buffer->frame_number());
  }

  return Ioctl(VIDIOC_QBUF, &v4l2_buffer);
}

bool V4L2IoctlShim::DQBuf(const std::unique_ptr<V4L2Queue>& queue,
                          uint32_t* index) const {
  LOG_ASSERT(queue->memory() == V4L2_MEMORY_MMAP)
      << "Only V4L2_MEMORY_MMAP is currently supported.";

  LOG_ASSERT(index != nullptr) << "|index| check failed.";

  struct v4l2_buffer v4l2_buffer;
  std::vector<v4l2_plane> planes(VIDEO_MAX_PLANES);

  memset(&v4l2_buffer, 0, sizeof v4l2_buffer);
  v4l2_buffer.type = queue->type();
  v4l2_buffer.memory = queue->memory();
  v4l2_buffer.m.planes = planes.data();
  v4l2_buffer.length = queue->num_planes();

  if (queue->type() == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
    // If no buffers have been dequeued for more than |kMaxRetryCount| retries,
    // we should exit the program. Something is wrong in the decoder or with
    // how we are controlling it.
    int num_tries = kMaxRetryCount;

    while ((num_tries != 0) && !Ioctl(VIDIOC_DQBUF, &v4l2_buffer)) {
      if (errno != EAGAIN)
        return false;

      num_tries--;
    }

    if (num_tries == 0) {
      LOG(ERROR)
          << "Decoder appeared to stall. VIDIOC_DQBUF ioctl call timed out.";
      return false;
    } else {
      // Successfully dequeued a buffer. Reset the |num_tries| counter.
      num_tries = kMaxRetryCount;
    }

    // We set |v4l2_buffer.timestamp.tv_usec| in the encoded chunk enqueued in
    // the OUTPUT queue, and the driver propagates it to the corresponding
    // decoded video frame (or at least is expected to). This gives us
    // information about which encoded frame corresponds to the current decoded
    // video frame.
    queue->GetBuffer(v4l2_buffer.index)->set_buffer_id(v4l2_buffer.index);
    queue->GetBuffer(v4l2_buffer.index)
        ->set_frame_number(v4l2_buffer.timestamp.tv_usec);

    *index = v4l2_buffer.index;

    return true;
  }

  DCHECK_EQ(queue->type(), V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

  // Currently, only 1 OUTPUT buffer is used.
  *index = 0;

  return Ioctl(VIDIOC_DQBUF, &v4l2_buffer);
}

bool V4L2IoctlShim::StreamOn(const enum v4l2_buf_type type) const {
  int arg = static_cast<int>(type);

  return Ioctl(VIDIOC_STREAMON, &arg);
}

bool V4L2IoctlShim::StreamOff(const enum v4l2_buf_type type) const {
  int arg = static_cast<int>(type);

  return Ioctl(VIDIOC_STREAMOFF, &arg);
}

bool V4L2IoctlShim::SetExtCtrls(const std::unique_ptr<V4L2Queue>& queue,
                                v4l2_ext_controls* ext_ctrls) const {
  // TODO(b/230021497): add compressed header probability related change
  // when V4L2_CID_STATELESS_VP9_COMPRESSED_HDR is supported

  // "If |request_fd| is set to a not-yet-queued request file descriptor
  // and |which| is set to V4L2_CTRL_WHICH_REQUEST_VAL, then the controls
  // are not applied immediately when calling VIDIOC_S_EXT_CTRLS, but
  // instead are applied by the driver for the buffer associated with
  // the same request.", see:
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/vidioc-g-ext-ctrls.html#description
  ext_ctrls->which = V4L2_CTRL_WHICH_REQUEST_VAL;
  ext_ctrls->request_fd = queue->media_request_fd();

  const bool ret = Ioctl(VIDIOC_S_EXT_CTRLS, ext_ctrls);

  return ret;
}

bool V4L2IoctlShim::MediaIocRequestAlloc(int* media_request_fd) const {
  LOG_ASSERT(media_request_fd != nullptr)
      << "|media_request_fd| check failed.\n";

  int allocated_req_fd;

  const bool ret = Ioctl(MEDIA_IOC_REQUEST_ALLOC, &allocated_req_fd);

  if (ret)
    *media_request_fd = allocated_req_fd;

  return ret;
}

bool V4L2IoctlShim::MediaRequestIocQueue(
    const std::unique_ptr<V4L2Queue>& queue) const {
  int req_fd = queue->media_request_fd();

  const bool ret = Ioctl(MEDIA_REQUEST_IOC_QUEUE, req_fd);

  return ret;
}

bool V4L2IoctlShim::MediaRequestIocReinit(
    const std::unique_ptr<V4L2Queue>& queue) const {
  int req_fd = queue->media_request_fd();

  const bool ret = Ioctl(MEDIA_REQUEST_IOC_REINIT, req_fd);

  return ret;
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

  const bool ret = Ioctl(VIDIOC_QUERYCAP, &cap);
  DCHECK(ret);

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
    std::unique_ptr<V4L2Queue>& queue) const {
  DCHECK_EQ(queue->memory(), V4L2_MEMORY_MMAP);

  MmapedBuffers buffers;

  for (uint32_t i = 0; i < queue->num_buffers(); ++i) {
    struct v4l2_buffer v4l_buffer;
    std::vector<v4l2_plane> planes(VIDEO_MAX_PLANES);

    memset(&v4l_buffer, 0, sizeof(v4l_buffer));
    v4l_buffer.type = queue->type();
    v4l_buffer.memory = queue->memory();
    v4l_buffer.index = i;
    v4l_buffer.length = queue->num_planes();
    v4l_buffer.m.planes = planes.data();

    const bool ret = Ioctl(VIDIOC_QUERYBUF, &v4l_buffer);
    DCHECK(ret);

    buffers.emplace_back(base::MakeRefCounted<MmapedBuffer>(
        decode_fd_.GetPlatformFile(), v4l_buffer));
  }

  queue->set_buffers(buffers);

  return true;
}

}  // namespace v4l2_test
}  // namespace media

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"

#include <fcntl.h>
#include <linux/media.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <string_view>
#include <unordered_map>

#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"

namespace media {

namespace v4l2_test {

constexpr int kIoctlOk = 0;

#define V4L2_REQUEST_CODE_AND_STRING(x) \
  { x, #x }

constexpr uint32_t kMaximumDeviceNumber = 150;

constexpr char kDecoderDevicePrefix[] = "/dev/video";
constexpr char kMediaDevicePrefix[] = "/dev/media";

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
        V4L2_REQUEST_CODE_AND_STRING(MEDIA_IOC_DEVICE_INFO),
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
  ostream << media::FourccToString(pix_mp.pixelformat) << ", " << pix_mp.width
          << " x " << pix_mp.height
          << ", num_planes = " << static_cast<size_t>(pix_mp.num_planes) << ".";

  return ostream;
}

// Logs whether given ioctl request |request_code| succeeded
// or failed given |ret|.
void LogIoctlResult(int ret, int request_code) {
  if (ret != kIoctlOk) {
    switch (errno) {
      case EAGAIN:
        LOG(INFO) << "Ioctl request failed for "
                  << V4L2RequestCodeToString(request_code)
                  << " with error code EAGAIN.";
        break;
      case EBUSY:
        LOG(WARNING) << "Ioctl request returned EBUSY for "
                     << V4L2RequestCodeToString(request_code)
                     << " and should be retried.";
        break;
      default:
        LOG(ERROR) << "Ioctl request failed for "
                   << V4L2RequestCodeToString(request_code) << ".";
    }
  }
  VLOG_IF(4, ret == kIoctlOk)
      << V4L2RequestCodeToString(request_code) << " succeeded.";
}

// Enumeration Ioctls are expected to return an error at the end of the list.
// Don't error on this message because this is the way that the client knows
// there are no more values to enumerate.
void LogIoctlResultForEnum(int ret, int request_code) {
  if (ret != kIoctlOk) {
    VLOG(1) << V4L2RequestCodeToString(request_code) << " failed(" << ret
            << ").";
  } else {
    VLOG(4) << V4L2RequestCodeToString(request_code) << " succeeded.";
  }
}

MmappedBuffer::MmappedBuffer(const base::PlatformFile ioctl_fd,
                             const struct v4l2_buffer& v4l2_buffer)
    : num_planes_(v4l2_buffer.length), buffer_id_(0) {
  for (uint32_t i = 0; i < num_planes_; ++i) {
    void* start_addr =
        mmap(NULL, v4l2_buffer.m.planes[i].length, PROT_READ | PROT_WRITE,
             MAP_SHARED, ioctl_fd, v4l2_buffer.m.planes[i].m.mem_offset);

    LOG_IF(FATAL, start_addr == MAP_FAILED)
        << "Failed to mmap buffer of length(" << v4l2_buffer.m.planes[i].length
        << ") and offset(" << std::hex << v4l2_buffer.m.planes[i].m.mem_offset
        << ").";

    mmapped_planes_.emplace_back(start_addr, v4l2_buffer.m.planes[i].length);
  }
}

MmappedBuffer::~MmappedBuffer() {
  for (const auto& [start_addr, length, bytes_used] : mmapped_planes_) {
    munmap(start_addr, length);
  }
}

V4L2Queue::V4L2Queue(enum v4l2_buf_type type,
                     const gfx::Size& resolution,
                     enum v4l2_memory memory)
    : type_(type),
      num_buffers_(0),
      resolution_(resolution),
      num_planes_(1),
      memory_(memory) {}

V4L2Queue::~V4L2Queue() = default;

scoped_refptr<MmappedBuffer> V4L2Queue::GetBuffer(const size_t index) const {
  DCHECK_LT(index, buffers_.size());

  return buffers_[index];
}

template <typename T>
bool V4L2IoctlShim::Ioctl(int request_code, T arg) const {
  NOTREACHED_IN_MIGRATION()
      << "Please add a specialized function for the given V4L2 ioctl "
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
  LogIoctlResultForEnum(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct v4l2_fmtdesc* fmtdesc) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_ENUM_FMT));
  LOG_ASSERT(fmtdesc != nullptr) << "|fmtdesc| check failed.";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, fmtdesc);
  LogIoctlResultForEnum(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct v4l2_frmsizeenum* frame_size) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_ENUM_FRAMESIZES));
  LOG_ASSERT(frame_size != nullptr) << "|frame_size| check failed.";

  const int ret = ioctl(decode_fd_.GetPlatformFile(), request_code, frame_size);
  LogIoctlResultForEnum(ret, request_code);

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

  if (request_code == static_cast<int>(MEDIA_IOC_REQUEST_ALLOC)) {
    ioctl_fd = media_fd_.GetPlatformFile();
  } else {
    ioctl_fd = decode_fd_.GetPlatformFile();
  }

  const int ret = ioctl(ioctl_fd, request_code, arg);

  LogIoctlResult(ret, request_code);

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct media_device_info* info) const {
  DCHECK_EQ(request_code, static_cast<int>(MEDIA_IOC_DEVICE_INFO));
  LOG_ASSERT(info != nullptr) << "|media_device_info| check failed.";

  const int ret = ioctl(media_fd_.GetPlatformFile(), request_code, info);
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

V4L2IoctlShim::V4L2IoctlShim(const uint32_t coded_fourcc) {
  // TODO(b/278748005): Remove |cur_val_is_supported_| when all drivers
  // fully support |V4L2_CTRL_WHICH_CUR_VAL|

  // On kernel version 5.4 the MTK driver for MT8192 does not correctly support
  // |V4L2_CTRL_WHICH_CUR_VAL|. This parameter is used when calling
  // VIDIOC_S_EXT_CTRLS to indicate that the call should be executed
  // immediately instead of putting it in a queue. Making sure the first
  // buffer is processed immediately is only necessary for codecs that
  // support 10 bit profiles. When processing a 10 bit profile the parameters
  // need to be processed before the format can be determined. There are no
  // chipsets that are on kernels older 5.10 and produce 10 bit output.
  constexpr char kKernelVersion5dot4[] = "Linux version 5.4*";
  std::string kernel_version;
  ReadFileToString(base::FilePath("/proc/version"), &kernel_version);

  cur_val_is_supported_ =
      !base::MatchPattern(kernel_version, kKernelVersion5dot4);

  for (uint32_t i = 0; i < kMaximumDeviceNumber; ++i) {
    std::string path =
        std::string(kDecoderDevicePrefix) + base::NumberToString(i);
    decode_fd_ = base::File(base::FilePath(path), base::File::FLAG_OPEN |
                                                      base::File::FLAG_READ |
                                                      base::File::FLAG_WRITE);

    // Check if the device supports the requested coded format
    if (QueryFormat(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, coded_fourcc)) {
      break;
    }

    // Close file descriptor on failure
    decode_fd_.Close();
  }

  PCHECK(decode_fd_.IsValid()) << "Failed to find available decode device.";

  struct v4l2_capability cap;
  memset(&cap, 0, sizeof(cap));

  const bool ret = Ioctl(VIDIOC_QUERYCAP, &cap);
  DCHECK(ret);

  LOG(INFO) << "Driver=\"" << cap.driver << "\" bus_info=\"" << cap.bus_info
            << "\" card=\"" << cap.card;

  if (!(cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)) {
    LOG(FATAL)
        << "Multi planar format required, but not supported by the driver.";
  }

  if (!FindMediaDevice(&cap)) {
    LOG(FATAL) << "Failed to find available media device.";
  }

  PCHECK(media_fd_.IsValid()) << "Media device fd is not valid.";
}

V4L2IoctlShim::~V4L2IoctlShim() = default;

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

void V4L2IoctlShim::SetFmt(const std::unique_ptr<V4L2Queue>& queue) const {
  struct v4l2_format fmt;

  if (queue->type() == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    // TODO(stevecho): remove VIDIOC_ENUM_FRAMESIZES ioctl call
    //   after b/193237015 is resolved.
    if (!EnumFrameSizes(queue->fourcc())) {
      LOG(INFO) << "EnumFrameSizes for OUTPUT queue failed.";
    }
  }

  memset(&fmt, 0, sizeof(fmt));
  fmt.type = queue->type();
  fmt.fmt.pix_mp.pixelformat = queue->fourcc();
  if (queue->type() == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    constexpr size_t kInputBufferMaxSize = 4 * 1024 * 1024;
    fmt.fmt.pix_mp.plane_fmt[0].sizeimage = kInputBufferMaxSize;
  }

  fmt.fmt.pix_mp.num_planes = queue->num_planes();
  fmt.fmt.pix_mp.width = queue->resolution().width();
  fmt.fmt.pix_mp.height = queue->resolution().height();

  const bool ret = Ioctl(VIDIOC_S_FMT, &fmt);

  LOGF(INFO) << queue->type() << " - VIDIOC_S_FMT: " << fmt.fmt.pix_mp;
  LOG_ASSERT(ret) << "VIDIOC_S_FMT for " << queue->type() << " queue failed.";
}

void V4L2IoctlShim::GetFmt(struct v4l2_format* fmt) const {
  const bool ret = Ioctl(VIDIOC_G_FMT, fmt);

  const enum v4l2_buf_type type = static_cast<enum v4l2_buf_type>(fmt->type);
  LOGF(INFO) << type << " - VIDIOC_G_FMT: " << fmt->fmt.pix_mp;
  LOG_ASSERT(ret) << "VIDIOC_G_FMT for " << type << " queue failed.";
}

void V4L2IoctlShim::TryFmt(struct v4l2_format* fmt) const {
  const bool ret = Ioctl(VIDIOC_TRY_FMT, fmt);

  const enum v4l2_buf_type type = static_cast<enum v4l2_buf_type>(fmt->type);
  LOGF(INFO) << type << " - VIDIOC_TRY_FMT: " << fmt->fmt.pix_mp;
  LOG_ASSERT(ret) << "VIDIOC_TRY_FMT for " << type << " queue failed.";
}

void V4L2IoctlShim::ReqBufs(std::unique_ptr<V4L2Queue>& queue,
                            uint32_t count) const {
  struct v4l2_requestbuffers reqbuf;

  memset(&reqbuf, 0, sizeof(reqbuf));
  reqbuf.count = count;
  reqbuf.type = queue->type();
  reqbuf.memory = queue->memory();

  const bool ret = Ioctl(VIDIOC_REQBUFS, &reqbuf);

  queue->set_num_buffers(reqbuf.count);

  if (count == 0) {
    LOGF(INFO) << "Requested to free all buffers in " << queue->type()
               << " with a buffer count of 0.";
  } else {
    LOGF(INFO) << queue->num_buffers() << " buffers requested, " << reqbuf.count
               << " buffers returned for " << queue->type() << ".";
  }

  LOG_ASSERT(ret) << "VIDIOC_REQBUFS for " << queue->type() << " queue failed.";
}

bool V4L2IoctlShim::QBuf(const std::unique_ptr<V4L2Queue>& queue,
                         const uint32_t buffer_id) const {
  LOG_ASSERT(queue->memory() == V4L2_MEMORY_MMAP)
      << "Only V4L2_MEMORY_MMAP is currently supported.";

  struct v4l2_buffer v4l2_buffer;
  std::vector<v4l2_plane> planes(VIDEO_MAX_PLANES);

  memset(&v4l2_buffer, 0, sizeof v4l2_buffer);
  v4l2_buffer.type = queue->type();
  v4l2_buffer.memory = queue->memory();
  v4l2_buffer.index = buffer_id;
  v4l2_buffer.m.planes = planes.data();
  v4l2_buffer.length = queue->num_planes();

  scoped_refptr<MmappedBuffer> buffer = queue->GetBuffer(buffer_id);

  for (uint32_t i = 0; i < queue->num_planes(); ++i) {
    v4l2_buffer.m.planes[i].length = buffer->mmapped_planes()[i].length;
    v4l2_buffer.m.planes[i].bytesused = buffer->mmapped_planes()[i].bytes_used;
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

void V4L2IoctlShim::DQBuf(const std::unique_ptr<V4L2Queue>& queue,
                          uint32_t* buffer_id) const {
  LOG_ASSERT(queue->memory() == V4L2_MEMORY_MMAP)
      << "Only V4L2_MEMORY_MMAP is currently supported.";

  LOG_ASSERT(buffer_id != nullptr) << "|buffer_id| check failed.";

  struct v4l2_buffer v4l2_buffer;
  std::vector<v4l2_plane> planes(VIDEO_MAX_PLANES);

  memset(&v4l2_buffer, 0, sizeof v4l2_buffer);
  v4l2_buffer.type = queue->type();
  v4l2_buffer.memory = queue->memory();
  v4l2_buffer.m.planes = planes.data();
  v4l2_buffer.length = queue->num_planes();

  const bool ret = Ioctl(VIDIOC_DQBUF, &v4l2_buffer);
  LOG_ASSERT(ret) << "VIDIOC_DQBUF failed for " << queue->type() << " queue.";

  // V4L2 explains |index| to be id number of the buffer. We are using
  // |buffer_id| (or |id|) instead of |index| consistently in the platform
  // decoding code to avoid confusion.
  const uint32_t id = v4l2_buffer.index;

  if (queue->type() == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
    // We set |v4l2_buffer.timestamp.tv_usec| in the encoded chunk enqueued in
    // the OUTPUT queue, and the driver propagates it to the corresponding
    // decoded video frame (or at least is expected to). This gives us
    // information about which encoded frame corresponds to the current
    // decoded video frame.
    queue->GetBuffer(id)->set_buffer_id(id);
    queue->GetBuffer(id)->set_frame_number(v4l2_buffer.timestamp.tv_usec);
  }

  *buffer_id = id;
}

void V4L2IoctlShim::StreamOn(const enum v4l2_buf_type type) const {
  int arg = static_cast<int>(type);

  const bool ret = Ioctl(VIDIOC_STREAMON, &arg);
  LOG_ASSERT(ret) << "VIDIOC_STREAMON for " << type << " queue failed.";
}

void V4L2IoctlShim::StreamOff(const enum v4l2_buf_type type) const {
  int arg = static_cast<int>(type);

  const bool ret = Ioctl(VIDIOC_STREAMOFF, &arg);
  LOG_ASSERT(ret) << "VIDIOC_STREAMOFF for " << type << " queue failed.";
}

void V4L2IoctlShim::SetExtCtrls(const std::unique_ptr<V4L2Queue>& queue,
                                v4l2_ext_controls* ext_ctrls,
                                bool immediate) const {
  // TODO(b/230021497): add compressed header probability related change
  // when V4L2_CID_STATELESS_VP9_COMPRESSED_HDR is supported

  // "If |request_fd| is set to a not-yet-queued request file descriptor
  // and |which| is set to V4L2_CTRL_WHICH_REQUEST_VAL, then the controls
  // are not applied immediately when calling VIDIOC_S_EXT_CTRLS, but
  // instead are applied by the driver for the buffer associated with
  // the same request.", see:
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/vidioc-g-ext-ctrls.html#description
  // Unmentioned in that documentation is that |V4L2_CTRL_WHICH_CUR_VAL| will
  // force the request to be processed immediately instead of being queue.
  if (immediate) {
    ext_ctrls->which = cur_val_is_supported_ ? V4L2_CTRL_WHICH_CUR_VAL
                                             : V4L2_CTRL_WHICH_REQUEST_VAL;
  } else {
    ext_ctrls->which = V4L2_CTRL_WHICH_REQUEST_VAL;
  }

  ext_ctrls->request_fd = queue->media_request_fd();

  const bool ret = Ioctl(VIDIOC_S_EXT_CTRLS, ext_ctrls);

  LOG_ASSERT(ret) << "VIDIOC_S_EXT_CTRLS failed.";
}

void V4L2IoctlShim::MediaIocRequestAlloc(int* media_request_fd) const {
  LOG_ASSERT(media_request_fd != nullptr)
      << "|media_request_fd| check failed.\n";

  int allocated_req_fd;

  const bool ret = Ioctl(MEDIA_IOC_REQUEST_ALLOC, &allocated_req_fd);

  if (ret)
    *media_request_fd = allocated_req_fd;

  LOG_ASSERT(ret) << "MEDIA_IOC_REQUEST_ALLOC failed";
}

void V4L2IoctlShim::MediaRequestIocQueue(
    const std::unique_ptr<V4L2Queue>& queue) const {
  int req_fd = queue->media_request_fd();

  const bool ret = Ioctl(MEDIA_REQUEST_IOC_QUEUE, req_fd);

  LOG_ASSERT(ret) << "MEDIA_REQUEST_IOC_QUEUE failed.";
}

void V4L2IoctlShim::MediaRequestIocReinit(
    const std::unique_ptr<V4L2Queue>& queue) const {
  int req_fd = queue->media_request_fd();

  const bool ret = Ioctl(MEDIA_REQUEST_IOC_REINIT, req_fd);

  LOG_ASSERT(ret) << "MEDIA_REQUEST_IOC_REINIT failed.";
}

void V4L2IoctlShim::WaitForRequestCompletion(
    const std::unique_ptr<V4L2Queue>& queue) const {
  struct pollfd pollfds[] = {
      {.fd = queue->media_request_fd(), .events = POLLPRI}};

  // There are some test vectors that are not expected to play back at real
  // time. 250ms corresponds to 4fps.
  constexpr int kPollTimeoutMS = 250;
  const int poll_result =
      HANDLE_EINTR(poll(pollfds, std::size(pollfds), kPollTimeoutMS));
  LOG_ASSERT(poll_result >= 0) << "Polling on request fd failed.";
  LOG_ASSERT(poll_result > 0) << "Polling on request fd timed out.";
  LOG_ASSERT(pollfds[0].revents & POLLPRI)
      << "Polling on request fd exited with incorrect revents.";
}

bool V4L2IoctlShim::FindMediaDevice(struct v4l2_capability* cap) {
  for (uint32_t i = 0; i < kMaximumDeviceNumber; ++i) {
    media_fd_ = base::File(
        base::FilePath(std::string(kMediaDevicePrefix) +
                       base::NumberToString(i)),
        base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);

    if (!media_fd_.IsValid()) {
      continue;
    }

    struct media_device_info media_info;
    const bool ret = Ioctl(MEDIA_IOC_DEVICE_INFO, &media_info);
    DCHECK(ret);

    // Match the video device and the media controller by the |bus_info|
    // field. This works better than |driver| field if there are multiple
    // instances of the same decoder driver in the system. However, old MediaTek
    // drivers didn't fill in the bus_info field for the media device.
    if (strlen(reinterpret_cast<const char*>(cap->bus_info)) > 0 &&
        strlen(reinterpret_cast<const char*>(media_info.bus_info)) > 0 &&
        !strcmp(reinterpret_cast<const char*>(cap->bus_info),
                reinterpret_cast<const char*>(media_info.bus_info))) {
      LOG(INFO) << "Using \"" << media_info.bus_info
                << "\" driver with /dev/media" << base::NumberToString(i)
                << ".";

      return true;
    }

    // Fall back to matching the video device and the media controller by the
    // |driver| field. This is needed because the mtk-vcodec driver does not
    // always fill the |card| and |bus_info| fields properly.
    if (!strcmp(reinterpret_cast<const char*>(cap->driver),
                reinterpret_cast<const char*>(media_info.driver))) {
      LOG(INFO) << "Using \"" << media_info.driver
                << "\" driver with /dev/media" << base::NumberToString(i)
                << ".";

      return true;
    }

    media_fd_.Close();
  }

  return false;
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

void V4L2IoctlShim::QueryAndMmapQueueBuffers(
    std::unique_ptr<V4L2Queue>& queue) const {
  DCHECK_EQ(queue->memory(), V4L2_MEMORY_MMAP);

  MmappedBuffers buffers;

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
    LOG_ASSERT(ret) << "VIDIOC_QUERYBUF for " << queue->type()
                    << " queue failed";

    buffers.emplace_back(base::MakeRefCounted<MmappedBuffer>(
        decode_fd_.GetPlatformFile(), v4l_buffer));
  }

  queue->set_buffers(buffers);
}

}  // namespace v4l2_test
}  // namespace media

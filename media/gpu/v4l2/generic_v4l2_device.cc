// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "media/gpu/v4l2/generic_v4l2_device.h"

#include <errno.h>
#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <algorithm>
#include <memory>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/video_types.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/generic_v4l2_device.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

#if BUILDFLAG(USE_LIBV4L2)
// Auto-generated for dlopen libv4l2 libraries
#include "media/gpu/v4l2/v4l2_stubs.h"
#include "third_party/v4l-utils/lib/include/libv4l2.h"

using media_gpu_v4l2::kModuleV4l2;
using media_gpu_v4l2::InitializeStubs;
using media_gpu_v4l2::StubPathMap;

static const base::FilePath::CharType kV4l2Lib[] =
    FILE_PATH_LITERAL("/usr/lib/libv4l2.so");
#endif

namespace media {

GenericV4L2Device::GenericV4L2Device() {
#if BUILDFLAG(USE_LIBV4L2)
  use_libv4l2_ = false;
#endif
}

GenericV4L2Device::~GenericV4L2Device() {
  CloseDevice();
}

int GenericV4L2Device::Ioctl(int request, void* arg) {
  DCHECK(device_fd_.is_valid());
#if BUILDFLAG(USE_LIBV4L2)
  if (use_libv4l2_)
    return HANDLE_EINTR(v4l2_ioctl(device_fd_.get(), request, arg));
#endif
  return HANDLE_EINTR(ioctl(device_fd_.get(), request, arg));
}

bool GenericV4L2Device::Poll(bool poll_device, bool* event_pending) {
  struct pollfd pollfds[2];
  nfds_t nfds;
  int pollfd = -1;

  pollfds[0].fd = device_poll_interrupt_fd_.get();
  pollfds[0].events = POLLIN | POLLERR;
  nfds = 1;

  if (poll_device) {
    DVLOGF(5) << "adding device fd to poll() set";
    pollfds[nfds].fd = device_fd_.get();
    pollfds[nfds].events = POLLIN | POLLOUT | POLLERR | POLLPRI;
    pollfd = nfds;
    nfds++;
  }

  if (HANDLE_EINTR(poll(pollfds, nfds, -1)) == -1) {
    VPLOGF(1) << "poll() failed";
    return false;
  }
  *event_pending = (pollfd != -1 && pollfds[pollfd].revents & POLLPRI);
  return true;
}

void* GenericV4L2Device::Mmap(void* addr,
                              unsigned int len,
                              int prot,
                              int flags,
                              unsigned int offset) {
  DCHECK(device_fd_.is_valid());
  return mmap(addr, len, prot, flags, device_fd_.get(), offset);
}

void GenericV4L2Device::Munmap(void* addr, unsigned int len) {
  munmap(addr, len);
}

bool GenericV4L2Device::SetDevicePollInterrupt() {
  DVLOGF(4);

  const uint64_t buf = 1;
  if (HANDLE_EINTR(write(device_poll_interrupt_fd_.get(), &buf, sizeof(buf))) ==
      -1) {
    VPLOGF(1) << "write() failed";
    return false;
  }
  return true;
}

bool GenericV4L2Device::ClearDevicePollInterrupt() {
  DVLOGF(5);

  uint64_t buf;
  if (HANDLE_EINTR(read(device_poll_interrupt_fd_.get(), &buf, sizeof(buf))) ==
      -1) {
    if (errno == EAGAIN) {
      // No interrupt flag set, and we're reading nonblocking.  Not an error.
      return true;
    } else {
      VPLOGF(1) << "read() failed";
      return false;
    }
  }
  return true;
}

bool GenericV4L2Device::Initialize() {
  DVLOGF(3);
  static bool v4l2_functions_initialized = PostSandboxInitialization();
  if (!v4l2_functions_initialized) {
    VLOGF(1) << "Failed to initialize LIBV4L2 libs";
    return false;
  }

  return true;
}

bool GenericV4L2Device::Open(Type type, uint32_t v4l2_pixfmt) {
  DVLOGF(3);
  std::string path = GetDevicePathFor(type, v4l2_pixfmt);

  if (path.empty()) {
    VLOGF(1) << "No devices supporting " << FourccToString(v4l2_pixfmt)
             << " for type: " << static_cast<int>(type);
    return false;
  }

  if (!OpenDevicePath(path, type)) {
    VLOGF(1) << "Failed opening " << path;
    return false;
  }

  device_poll_interrupt_fd_.reset(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
  if (!device_poll_interrupt_fd_.is_valid()) {
    VLOGF(1) << "Failed creating a poll interrupt fd";
    return false;
  }

  return true;
}

std::vector<base::ScopedFD> GenericV4L2Device::GetDmabufsForV4L2Buffer(
    int index,
    size_t num_planes,
    enum v4l2_buf_type buf_type) {
  DVLOGF(3);
  DCHECK(V4L2_TYPE_IS_MULTIPLANAR(buf_type));

  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < num_planes; ++i) {
    struct v4l2_exportbuffer expbuf;
    memset(&expbuf, 0, sizeof(expbuf));
    expbuf.type = buf_type;
    expbuf.index = index;
    expbuf.plane = i;
    expbuf.flags = O_CLOEXEC;
    if (Ioctl(VIDIOC_EXPBUF, &expbuf) != 0) {
      dmabuf_fds.clear();
      break;
    }

    dmabuf_fds.push_back(base::ScopedFD(expbuf.fd));
  }

  return dmabuf_fds;
}

bool GenericV4L2Device::CanCreateEGLImageFrom(uint32_t v4l2_pixfmt) {
  static uint32_t kEGLImageDrmFmtsSupported[] = {
    DRM_FORMAT_ARGB8888,
#if defined(ARCH_CPU_ARM_FAMILY)
    DRM_FORMAT_NV12,
    DRM_FORMAT_YVU420,
#endif
  };

  return std::find(
             kEGLImageDrmFmtsSupported,
             kEGLImageDrmFmtsSupported + base::size(kEGLImageDrmFmtsSupported),
             V4L2PixFmtToDrmFormat(v4l2_pixfmt)) !=
         kEGLImageDrmFmtsSupported + base::size(kEGLImageDrmFmtsSupported);
}

EGLImageKHR GenericV4L2Device::CreateEGLImage(
    EGLDisplay egl_display,
    EGLContext /* egl_context */,
    GLuint texture_id,
    const gfx::Size& size,
    unsigned int buffer_index,
    uint32_t v4l2_pixfmt,
    const std::vector<base::ScopedFD>& dmabuf_fds) {
  DVLOGF(3);
  if (!CanCreateEGLImageFrom(v4l2_pixfmt)) {
    VLOGF(1) << "Unsupported V4L2 pixel format";
    return EGL_NO_IMAGE_KHR;
  }

  VideoPixelFormat vf_format =
      Fourcc::FromV4L2PixFmt(v4l2_pixfmt).ToVideoPixelFormat();
  // Number of components, as opposed to the number of V4L2 planes, which is
  // just a buffer count.
  size_t num_planes = VideoFrame::NumPlanes(vf_format);
  DCHECK_LE(num_planes, 3u);
  if (num_planes < dmabuf_fds.size()) {
    // It's possible for more than one DRM plane to reside in one V4L2 plane,
    // but not the other way around. We must use all V4L2 planes.
    LOG(ERROR) << "Invalid plane count";
    return EGL_NO_IMAGE_KHR;
  }

  std::vector<EGLint> attrs;
  attrs.push_back(EGL_WIDTH);
  attrs.push_back(size.width());
  attrs.push_back(EGL_HEIGHT);
  attrs.push_back(size.height());
  attrs.push_back(EGL_LINUX_DRM_FOURCC_EXT);
  attrs.push_back(V4L2PixFmtToDrmFormat(v4l2_pixfmt));

  // For existing formats, if we have less buffers (V4L2 planes) than
  // components (planes), the remaining planes are stored in the last
  // V4L2 plane. Use one V4L2 plane per each component until we run out of V4L2
  // planes, and use the last V4L2 plane for all remaining components, each
  // with an offset equal to the size of the preceding planes in the same
  // V4L2 plane.
  size_t v4l2_plane = 0;
  size_t plane_offset = 0;
  for (size_t plane = 0; plane < num_planes; ++plane) {
    attrs.push_back(EGL_DMA_BUF_PLANE0_FD_EXT + plane * 3);
    attrs.push_back(dmabuf_fds[v4l2_plane].get());
    attrs.push_back(EGL_DMA_BUF_PLANE0_OFFSET_EXT + plane * 3);
    attrs.push_back(plane_offset);
    attrs.push_back(EGL_DMA_BUF_PLANE0_PITCH_EXT + plane * 3);
    attrs.push_back(VideoFrame::RowBytes(plane, vf_format, size.width()));

    if (v4l2_plane + 1 < dmabuf_fds.size()) {
      ++v4l2_plane;
      plane_offset = 0;
    } else {
      plane_offset += VideoFrame::PlaneSize(vf_format, plane, size).GetArea();
    }
  }

  attrs.push_back(EGL_NONE);

  EGLImageKHR egl_image = eglCreateImageKHR(
      egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, &attrs[0]);
  if (egl_image == EGL_NO_IMAGE_KHR) {
    VLOGF(1) << "Failed creating EGL image: " << ui::GetLastEGLErrorString();
    return egl_image;
  }
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image);

  return egl_image;
}

scoped_refptr<gl::GLImage> GenericV4L2Device::CreateGLImage(
    const gfx::Size& size,
    uint32_t fourcc,
    const std::vector<base::ScopedFD>& dmabuf_fds) {
  DVLOGF(3);
  DCHECK(CanCreateEGLImageFrom(fourcc));
  VideoPixelFormat vf_format =
      Fourcc::FromV4L2PixFmt(fourcc).ToVideoPixelFormat();
  size_t num_planes = VideoFrame::NumPlanes(vf_format);
  DCHECK_LE(num_planes, 3u);
  DCHECK_LE(dmabuf_fds.size(), num_planes);

  gfx::NativePixmapHandle native_pixmap_handle;

  std::vector<base::ScopedFD> duped_fds;
  // The number of file descriptors can be less than the number of planes when
  // v4l2 pix fmt, |fourcc|, is a single plane format. Duplicating the last
  // file descriptor should be safely used for the later planes, because they
  // are on the last buffer.
  for (size_t i = 0; i < num_planes; ++i) {
    int fd =
        i < dmabuf_fds.size() ? dmabuf_fds[i].get() : dmabuf_fds.back().get();
    duped_fds.emplace_back(HANDLE_EINTR(dup(fd)));
    if (!duped_fds.back().is_valid()) {
      VPLOGF(1) << "Failed duplicating a dmabuf fd";
      return nullptr;
    }
  }

  // For existing formats, if we have less buffers (V4L2 planes) than
  // components (planes), the remaining planes are stored in the last
  // V4L2 plane. Use one V4L2 plane per each component until we run out of V4L2
  // planes, and use the last V4L2 plane for all remaining components, each
  // with an offset equal to the size of the preceding planes in the same
  // V4L2 plane.
  size_t v4l2_plane = 0;
  size_t plane_offset = 0;
  for (size_t p = 0; p < num_planes; ++p) {
    native_pixmap_handle.planes.emplace_back(
        VideoFrame::RowBytes(p, vf_format, size.width()), plane_offset,
        VideoFrame::PlaneSize(vf_format, p, size).GetArea(),
        std::move(duped_fds[p]));

    if (v4l2_plane + 1 < dmabuf_fds.size()) {
      ++v4l2_plane;
      plane_offset = 0;
    } else {
      plane_offset += VideoFrame::PlaneSize(vf_format, p, size).GetArea();
    }
  }

  gfx::BufferFormat buffer_format = gfx::BufferFormat::BGRA_8888;
  switch (fourcc) {
    case DRM_FORMAT_ARGB8888:
      buffer_format = gfx::BufferFormat::BGRA_8888;
      break;
    case DRM_FORMAT_NV12:
      buffer_format = gfx::BufferFormat::YUV_420_BIPLANAR;
      break;
    case DRM_FORMAT_YVU420:
      buffer_format = gfx::BufferFormat::YVU_420;
      break;
    default:
      NOTREACHED();
  }

  scoped_refptr<gfx::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmapFromHandle(0, size, buffer_format,
                                         std::move(native_pixmap_handle));

  DCHECK(pixmap);

  auto image =
      base::MakeRefCounted<gl::GLImageNativePixmap>(size, buffer_format);
  bool ret = image->Initialize(std::move(pixmap));
  DCHECK(ret);
  return image;
}

EGLBoolean GenericV4L2Device::DestroyEGLImage(EGLDisplay egl_display,
                                              EGLImageKHR egl_image) {
  DVLOGF(3);
  EGLBoolean result = eglDestroyImageKHR(egl_display, egl_image);
  if (result != EGL_TRUE) {
    LOG(WARNING) << "Destroy EGLImage failed.";
  }
  return result;
}

GLenum GenericV4L2Device::GetTextureTarget() {
  return GL_TEXTURE_EXTERNAL_OES;
}

std::vector<uint32_t> GenericV4L2Device::PreferredInputFormat(Type type) {
  if (type == Type::kEncoder)
    return {V4L2_PIX_FMT_NV12M, V4L2_PIX_FMT_NV12};

  return {};
}

std::vector<uint32_t> GenericV4L2Device::GetSupportedImageProcessorPixelformats(
    v4l2_buf_type buf_type) {
  std::vector<uint32_t> supported_pixelformats;

  Type type = Type::kImageProcessor;
  const auto& devices = GetDevicesForType(type);
  for (const auto& device : devices) {
    if (!OpenDevicePath(device.first, type)) {
      VLOGF(1) << "Failed opening " << device.first;
      continue;
    }

    std::vector<uint32_t> pixelformats =
        EnumerateSupportedPixelformats(buf_type);

    supported_pixelformats.insert(supported_pixelformats.end(),
                                  pixelformats.begin(), pixelformats.end());
    CloseDevice();
  }

  return supported_pixelformats;
}

VideoDecodeAccelerator::SupportedProfiles
GenericV4L2Device::GetSupportedDecodeProfiles(const size_t num_formats,
                                              const uint32_t pixelformats[]) {
  VideoDecodeAccelerator::SupportedProfiles supported_profiles;

  Type type = Type::kDecoder;
  const auto& devices = GetDevicesForType(type);
  for (const auto& device : devices) {
    if (!OpenDevicePath(device.first, type)) {
      VLOGF(1) << "Failed opening " << device.first;
      continue;
    }

    const auto& profiles =
        EnumerateSupportedDecodeProfiles(num_formats, pixelformats);
    supported_profiles.insert(supported_profiles.end(), profiles.begin(),
                              profiles.end());
    CloseDevice();
  }

  return supported_profiles;
}

VideoEncodeAccelerator::SupportedProfiles
GenericV4L2Device::GetSupportedEncodeProfiles() {
  VideoEncodeAccelerator::SupportedProfiles supported_profiles;

  Type type = Type::kEncoder;
  const auto& devices = GetDevicesForType(type);
  for (const auto& device : devices) {
    if (!OpenDevicePath(device.first, type)) {
      VLOGF(1) << "Failed opening " << device.first;
      continue;
    }

    const auto& profiles = EnumerateSupportedEncodeProfiles();
    supported_profiles.insert(supported_profiles.end(), profiles.begin(),
                              profiles.end());
    CloseDevice();
  }

  return supported_profiles;
}

bool GenericV4L2Device::IsImageProcessingSupported() {
  const auto& devices = GetDevicesForType(Type::kImageProcessor);
  return !devices.empty();
}

bool GenericV4L2Device::IsJpegDecodingSupported() {
  const auto& devices = GetDevicesForType(Type::kJpegDecoder);
  return !devices.empty();
}

bool GenericV4L2Device::IsJpegEncodingSupported() {
  const auto& devices = GetDevicesForType(Type::kJpegEncoder);
  return !devices.empty();
}

bool GenericV4L2Device::OpenDevicePath(const std::string& path, Type type) {
  DCHECK(!device_fd_.is_valid());

  device_fd_.reset(
      HANDLE_EINTR(open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC)));
  if (!device_fd_.is_valid())
    return false;

#if BUILDFLAG(USE_LIBV4L2)
  if (type == Type::kEncoder &&
      HANDLE_EINTR(v4l2_fd_open(device_fd_.get(), V4L2_DISABLE_CONVERSION)) !=
          -1) {
    DVLOGF(3) << "Using libv4l2 for " << path;
    use_libv4l2_ = true;
  }
#endif
  return true;
}

void GenericV4L2Device::CloseDevice() {
  DVLOGF(3);
#if BUILDFLAG(USE_LIBV4L2)
  if (use_libv4l2_ && device_fd_.is_valid())
    v4l2_close(device_fd_.release());
#endif
  device_fd_.reset();
}

// static
bool GenericV4L2Device::PostSandboxInitialization() {
#if BUILDFLAG(USE_LIBV4L2)
  StubPathMap paths;
  paths[kModuleV4l2].push_back(kV4l2Lib);

  return InitializeStubs(paths);
#else
  return true;
#endif
}

void GenericV4L2Device::EnumerateDevicesForType(Type type) {
  static const std::string kDecoderDevicePattern = "/dev/video-dec";
  static const std::string kEncoderDevicePattern = "/dev/video-enc";
  static const std::string kImageProcessorDevicePattern = "/dev/image-proc";
  static const std::string kJpegDecoderDevicePattern = "/dev/jpeg-dec";
  static const std::string kJpegEncoderDevicePattern = "/dev/jpeg-enc";

  std::string device_pattern;
  v4l2_buf_type buf_type;
  switch (type) {
    case Type::kDecoder:
      device_pattern = kDecoderDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      break;
    case Type::kEncoder:
      device_pattern = kEncoderDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      break;
    case Type::kImageProcessor:
      device_pattern = kImageProcessorDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      break;
    case Type::kJpegDecoder:
      device_pattern = kJpegDecoderDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      break;
    case Type::kJpegEncoder:
      device_pattern = kJpegEncoderDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      break;
  }

  std::vector<std::string> candidate_paths;

  // TODO(posciak): Remove this legacy unnumbered device once
  // all platforms are updated to use numbered devices.
  candidate_paths.push_back(device_pattern);

  // We are sandboxed, so we can't query directory contents to check which
  // devices are actually available. Try to open the first 10; if not present,
  // we will just fail to open immediately.
  for (int i = 0; i < 10; ++i) {
    candidate_paths.push_back(
        base::StringPrintf("%s%d", device_pattern.c_str(), i));
  }

  Devices devices;
  for (const auto& path : candidate_paths) {
    if (!OpenDevicePath(path, type))
      continue;

    const auto& supported_pixelformats =
        EnumerateSupportedPixelformats(buf_type);
    if (!supported_pixelformats.empty()) {
      DVLOGF(3) << "Found device: " << path;
      devices.push_back(std::make_pair(path, supported_pixelformats));
    }

    CloseDevice();
  }

  DCHECK_EQ(devices_by_type_.count(type), 0u);
  devices_by_type_[type] = devices;
}

const GenericV4L2Device::Devices& GenericV4L2Device::GetDevicesForType(
    Type type) {
  if (devices_by_type_.count(type) == 0)
    EnumerateDevicesForType(type);

  DCHECK_NE(devices_by_type_.count(type), 0u);
  return devices_by_type_[type];
}

std::string GenericV4L2Device::GetDevicePathFor(Type type, uint32_t pixfmt) {
  const Devices& devices = GetDevicesForType(type);

  for (const auto& device : devices) {
    if (std::find(device.second.begin(), device.second.end(), pixfmt) !=
        device.second.end())
      return device.first;
  }

  return std::string();
}

}  //  namespace media

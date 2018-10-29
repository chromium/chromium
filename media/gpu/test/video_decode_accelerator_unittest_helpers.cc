// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/format_utils.h"
#include "media/gpu/test/rendering_helper.h"
#include "media/video/h264_parser.h"

#if defined(OS_CHROMEOS)
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace media {
namespace test {

namespace {
const size_t kMD5StringLength = 32;
}  // namespace

VideoDecodeAcceleratorTestEnvironment::VideoDecodeAcceleratorTestEnvironment(
    bool use_gl_renderer)
    : use_gl_renderer_(use_gl_renderer),
      rendering_thread_("GLRenderingVDAClientThread") {}

VideoDecodeAcceleratorTestEnvironment::
    ~VideoDecodeAcceleratorTestEnvironment() {}

void VideoDecodeAcceleratorTestEnvironment::SetUp() {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_UI;
  rendering_thread_.StartWithOptions(options);

  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  rendering_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::InitializeOneOff, use_gl_renderer_, &done));
  done.Wait();

#if defined(OS_CHROMEOS)
  gpu_helper_.reset(new ui::OzoneGpuTestHelper());
  // Need to initialize after the rendering side since the rendering side
  // initializes the "GPU" parts of Ozone.
  //
  // This also needs to be done in the test environment since this shouldn't
  // be initialized multiple times for the same Ozone platform.
  gpu_helper_->Initialize(base::ThreadTaskRunnerHandle::Get());
#endif
}

void VideoDecodeAcceleratorTestEnvironment::TearDown() {
#if defined(OS_CHROMEOS)
  gpu_helper_.reset();
#endif
  rendering_thread_.Stop();
}

scoped_refptr<base::SingleThreadTaskRunner>
VideoDecodeAcceleratorTestEnvironment::GetRenderingTaskRunner() const {
  return rendering_thread_.task_runner();
}

TextureRef::TextureRef(uint32_t texture_id,
                       base::OnceClosure no_longer_needed_cb)
    : texture_id_(texture_id),
      no_longer_needed_cb_(std::move(no_longer_needed_cb)) {}

TextureRef::~TextureRef() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(no_longer_needed_cb_).Run();
}

// static
scoped_refptr<TextureRef> TextureRef::Create(
    uint32_t texture_id,
    base::OnceClosure no_longer_needed_cb) {
  return base::WrapRefCounted(
      new TextureRef(texture_id, std::move(no_longer_needed_cb)));
}

// static
scoped_refptr<TextureRef> TextureRef::CreatePreallocated(
    uint32_t texture_id,
    base::OnceClosure no_longer_needed_cb,
    VideoPixelFormat pixel_format,
    const gfx::Size& size) {
  scoped_refptr<TextureRef> texture_ref;
#if defined(OS_CHROMEOS)
  texture_ref = TextureRef::Create(texture_id, std::move(no_longer_needed_cb));
  LOG_ASSERT(texture_ref);

  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  gfx::BufferFormat buffer_format =
      VideoPixelFormatToGfxBufferFormat(pixel_format);
  texture_ref->pixmap_ =
      factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size,
                                  buffer_format,
                                  gfx::BufferUsage::SCANOUT_VDA_WRITE);
  LOG_ASSERT(texture_ref->pixmap_);
  texture_ref->coded_size_ = size;
#endif

  return texture_ref;
}

gfx::GpuMemoryBufferHandle TextureRef::ExportGpuMemoryBufferHandle() const {
  gfx::GpuMemoryBufferHandle handle;
#if defined(OS_CHROMEOS)
  CHECK(pixmap_);
  handle.type = gfx::NATIVE_PIXMAP;

  size_t num_planes =
      gfx::NumberOfPlanesForBufferFormat(pixmap_->GetBufferFormat());
  for (size_t i = 0; i < num_planes; ++i) {
    handle.native_pixmap_handle.planes.emplace_back(
        pixmap_->GetDmaBufPitch(i), pixmap_->GetDmaBufOffset(i), i,
        pixmap_->GetDmaBufModifier(i));
  }

  size_t num_fds = pixmap_->GetDmaBufFdCount();
  LOG_ASSERT(num_fds == num_planes || num_fds == 1);
  for (size_t i = 0; i < num_fds; ++i) {
    int duped_fd = HANDLE_EINTR(dup(pixmap_->GetDmaBufFd(i)));
    LOG_ASSERT(duped_fd != -1) << "Failed duplicating dmabuf fd";
    handle.native_pixmap_handle.fds.emplace_back(
        base::FileDescriptor(duped_fd, true));
  }
#endif
  return handle;
}

scoped_refptr<VideoFrame> TextureRef::CreateVideoFrame(
    const gfx::Rect& visible_rect) const {
  scoped_refptr<VideoFrame> video_frame;
#if defined(OS_CHROMEOS)
  VideoPixelFormat pixel_format =
      GfxBufferFormatToVideoPixelFormat(pixmap_->GetBufferFormat());
  CHECK_NE(pixel_format, PIXEL_FORMAT_UNKNOWN);
  size_t num_planes = VideoFrame::NumPlanes(pixel_format);
  std::vector<VideoFrameLayout::Plane> planes(num_planes);
  std::vector<int> plane_height(num_planes, 0u);
  for (size_t i = 0; i < num_planes; ++i) {
    planes[i].stride = pixmap_->GetDmaBufPitch(i);
    planes[i].offset = pixmap_->GetDmaBufOffset(i);
    plane_height[i] = VideoFrame::Rows(i, pixel_format, coded_size_.height());
  }

  std::vector<base::ScopedFD> dmabuf_fds;
  size_t num_fds = pixmap_->GetDmaBufFdCount();
  LOG_ASSERT(num_fds <= num_planes);
  for (size_t i = 0; i < num_fds; ++i) {
    int duped_fd = HANDLE_EINTR(dup(pixmap_->GetDmaBufFd(i)));
    LOG_ASSERT(duped_fd != -1) << "Failed duplicating dmabuf fd";
    dmabuf_fds.emplace_back(duped_fd);
  }

  std::vector<size_t> buffer_sizes(num_fds, 0u);
  for (size_t plane = 0, i = 0; plane < num_planes; ++plane) {
    if (plane + 1 < buffer_sizes.size()) {
      buffer_sizes[i] =
          planes[plane].offset + planes[plane].stride * plane_height[plane];
      ++i;
    } else {
      buffer_sizes[i] = std::max(
          buffer_sizes[i],
          planes[plane].offset + planes[plane].stride * plane_height[plane]);
    }
  }
  auto layout = VideoFrameLayout::CreateWithPlanes(
      pixel_format, coded_size_, std::move(planes), std::move(buffer_sizes));
  LOG_ASSERT(layout.has_value() == true);
  video_frame = VideoFrame::WrapExternalDmabufs(
      *layout, visible_rect, visible_rect.size(), std::move(dmabuf_fds),
      base::TimeDelta());
#endif
  return video_frame;
}

EncodedDataHelper::EncodedDataHelper(const std::string& data,
                                     VideoCodecProfile profile)
    : data_(data), profile_(profile) {}

EncodedDataHelper::~EncodedDataHelper() {
  base::STLClearObject(&data_);
}

bool EncodedDataHelper::IsNALHeader(const std::string& data, size_t pos) {
  return data[pos] == 0 && data[pos + 1] == 0 && data[pos + 2] == 0 &&
         data[pos + 3] == 1;
}

std::string EncodedDataHelper::GetBytesForNextData() {
  switch (VideoCodecProfileToVideoCodec(profile_)) {
    case kCodecH264:
      return GetBytesForNextFragment();
    case kCodecVP8:
    case kCodecVP9:
      return GetBytesForNextFrame();
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string EncodedDataHelper::GetBytesForNextFragment() {
  if (next_pos_to_decode_ == 0) {
    size_t skipped_fragments_count = 0;
    if (!LookForSPS(&skipped_fragments_count)) {
      next_pos_to_decode_ = 0;
      return std::string();
    }
    num_skipped_fragments_ += skipped_fragments_count;
  }

  size_t start_pos = next_pos_to_decode_;
  size_t next_nalu_pos = GetBytesForNextNALU(start_pos);

  // Update next_pos_to_decode_.
  next_pos_to_decode_ = next_nalu_pos;
  return data_.substr(start_pos, next_nalu_pos - start_pos);
}

size_t EncodedDataHelper::GetBytesForNextNALU(size_t start_pos) {
  size_t pos = start_pos;
  if (pos + 4 > data_.size())
    return pos;
  LOG_ASSERT(IsNALHeader(data_, pos));
  pos += 4;
  while (pos + 4 <= data_.size() && !IsNALHeader(data_, pos)) {
    ++pos;
  }
  if (pos + 3 >= data_.size())
    pos = data_.size();
  return pos;
}

bool EncodedDataHelper::LookForSPS(size_t* skipped_fragments_count) {
  *skipped_fragments_count = 0;
  while (next_pos_to_decode_ + 4 < data_.size()) {
    if ((data_[next_pos_to_decode_ + 4] & 0x1f) == 0x7) {
      return true;
    }
    *skipped_fragments_count += 1;
    next_pos_to_decode_ = GetBytesForNextNALU(next_pos_to_decode_);
  }
  return false;
}

std::string EncodedDataHelper::GetBytesForNextFrame() {
  // Helpful description: http://wiki.multimedia.cx/index.php?title=IVF
  size_t pos = next_pos_to_decode_;
  std::string bytes;
  if (pos == 0)
    pos = 32;  // Skip IVF header.

  uint32_t frame_size = *reinterpret_cast<uint32_t*>(&data_[pos]);
  pos += 12;  // Skip frame header.
  bytes.append(data_.substr(pos, frame_size));

  // Update next_pos_to_decode_.
  next_pos_to_decode_ = pos + frame_size;
  return bytes;
}

// static
bool EncodedDataHelper::HasConfigInfo(const uint8_t* data,
                                      size_t size,
                                      VideoCodecProfile profile) {
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    H264Parser parser;
    parser.SetStream(data, size);
    H264NALU nalu;
    H264Parser::Result result = parser.AdvanceToNextNALU(&nalu);
    if (result != H264Parser::kOk) {
      // Let the VDA figure out there's something wrong with the stream.
      return false;
    }

    return nalu.nal_unit_type == H264NALU::kSPS;
  } else if (profile >= VP8PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    return (size > 0 && !(data[0] & 0x01));
  }
  // Shouldn't happen at this point.
  LOG(FATAL) << "Invalid profile: " << GetProfileName(profile);
  return false;
}

// Read in golden MD5s for the thumbnailed rendering of this video
std::vector<std::string> ReadGoldenThumbnailMD5s(
    const base::FilePath& md5_file_path) {
  std::vector<std::string> golden_md5s;
  std::vector<std::string> md5_strings;
  std::string all_md5s;
  base::ReadFileToString(md5_file_path, &all_md5s);
  md5_strings = base::SplitString(all_md5s, "\n", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_ALL);
  // Check these are legitimate MD5s.
  for (const std::string& md5_string : md5_strings) {
    // Ignore the empty string added by SplitString
    if (!md5_string.length())
      continue;
    // Ignore comments
    if (md5_string.at(0) == '#')
      continue;

    bool valid_length = md5_string.length() == kMD5StringLength;
    LOG_IF(ERROR, !valid_length) << "MD5 length error: " << md5_string;
    bool hex_only = std::count_if(md5_string.begin(), md5_string.end(),
                                  isxdigit) == kMD5StringLength;
    LOG_IF(ERROR, !hex_only) << "MD5 includes non-hex char: " << md5_string;
    if (valid_length && hex_only)
      golden_md5s.push_back(md5_string);
  }
  LOG_IF(ERROR, md5_strings.empty())
      << "  MD5 checksum file (" << md5_file_path.MaybeAsASCII()
      << ") missing or empty.";
  return golden_md5s;
}

bool ConvertRGBAToRGB(const std::vector<unsigned char>& rgba,
                      std::vector<unsigned char>* rgb) {
  size_t num_pixels = rgba.size() / 4;
  rgb->resize(num_pixels * 3);
  // Drop the alpha channel, but check as we go that it is all 0xff.
  bool solid = true;
  for (size_t i = 0; i < num_pixels; i++) {
    (*rgb)[3 * i] = rgba[4 * i];
    (*rgb)[3 * i + 1] = rgba[4 * i + 1];
    (*rgb)[3 * i + 2] = rgba[4 * i + 2];
    solid = solid && (rgba[4 * i + 3] == 0xff);
  }
  return solid;
}
}  // namespace test
}  // namespace media

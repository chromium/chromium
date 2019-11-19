// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/gl_manager.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_direct.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_ref_counted_memory.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_MACOSX)
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/gl_image_io_surface.h"
#endif

namespace gpu {
namespace {

void InitializeGpuPreferencesForTestingFromCommandLine(
    const base::CommandLine& command_line,
    GpuPreferences* preferences) {
  // Only initialize specific GpuPreferences members used for testing.
  preferences->use_passthrough_cmd_decoder =
      gles2::UsePassthroughCommandDecoder(&command_line);
}

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(base::RefCountedBytes* bytes,
                      const gfx::Size& size,
                      gfx::BufferFormat format)
      : mapped_(false), bytes_(bytes), size_(size), format_(format) {}

  static GpuMemoryBufferImpl* FromClientBuffer(ClientBuffer buffer) {
    return reinterpret_cast<GpuMemoryBufferImpl*>(buffer);
  }

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
    DCHECK(!mapped_);
    mapped_ = true;
    return true;
  }
  void* memory(size_t plane) override {
    DCHECK(mapped_);
    DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return reinterpret_cast<uint8_t*>(&bytes_->data().front()) +
           gfx::BufferOffsetForBufferFormat(size_, format_, plane);
  }
  void Unmap() override {
    DCHECK(mapped_);
    mapped_ = false;
  }
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetFormat() const override { return format_; }
  int stride(size_t plane) const override {
    DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return gfx::RowSizeForBufferFormat(size_.width(), format_, plane);
  }
  gfx::GpuMemoryBufferId GetId() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferId(0);
  }
  gfx::GpuMemoryBufferType GetType() const override {
    return gfx::NATIVE_PIXMAP;
  }
  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferHandle();
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {}

  base::RefCountedBytes* bytes() { return bytes_.get(); }

 private:
  bool mapped_;
  scoped_refptr<base::RefCountedBytes> bytes_;
  const gfx::Size size_;
  gfx::BufferFormat format_;
};

#if defined(OS_MACOSX)
class IOSurfaceGpuMemoryBuffer : public gfx::GpuMemoryBuffer {
 public:
  IOSurfaceGpuMemoryBuffer(const gfx::Size& size, gfx::BufferFormat format)
      : mapped_(false), size_(size), format_(format) {
    iosurface_ = gfx::CreateIOSurface(size, gfx::BufferFormat::BGRA_8888);
  }

  ~IOSurfaceGpuMemoryBuffer() override {
    CFRelease(iosurface_);
  }

  static IOSurfaceGpuMemoryBuffer* FromClientBuffer(ClientBuffer buffer) {
    return reinterpret_cast<IOSurfaceGpuMemoryBuffer*>(buffer);
  }

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
    DCHECK(!mapped_);
    mapped_ = true;
    return true;
  }
  void* memory(size_t plane) override {
    DCHECK(mapped_);
    DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return IOSurfaceGetBaseAddressOfPlane(iosurface_, plane);
  }
  void Unmap() override {
    DCHECK(mapped_);
    mapped_ = false;
  }
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetFormat() const override { return format_; }
  int stride(size_t plane) const override {
    DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return IOSurfaceGetWidthOfPlane(iosurface_, plane);
  }
  gfx::GpuMemoryBufferId GetId() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferId(0);
  }
  gfx::GpuMemoryBufferType GetType() const override {
    return gfx::IO_SURFACE_BUFFER;
  }
  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferHandle();
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {}

  IOSurfaceRef iosurface() { return iosurface_; }

 private:
  bool mapped_;
  IOSurfaceRef iosurface_;
  const gfx::Size size_;
  gfx::BufferFormat format_;
};
#endif  // defined(OS_MACOSX)

class CommandBufferCheckLostContext : public CommandBufferDirect {
 public:
  explicit CommandBufferCheckLostContext(bool context_lost_allowed)
      : context_lost_allowed_(context_lost_allowed) {}

  ~CommandBufferCheckLostContext() override = default;

  void Flush(int32_t put_offset) override {
    CommandBufferDirect::Flush(put_offset);

    ::gpu::CommandBuffer::State state = GetLastState();
    if (!context_lost_allowed_) {
      ASSERT_EQ(::gpu::error::kNoError, state.error);
    }
  }

 private:
  bool context_lost_allowed_;
};

}  // namespace

int GLManager::use_count_;
scoped_refptr<gl::GLShareGroup>* GLManager::base_share_group_;
scoped_refptr<gl::GLSurface>* GLManager::base_surface_;
scoped_refptr<gl::GLContext>* GLManager::base_context_;
// static
GpuFeatureInfo GLManager::g_gpu_feature_info;

GLManager::Options::Options() = default;

GLManager::GLManager()
    : gpu_memory_buffer_factory_(
          gpu::GpuMemoryBufferFactory::CreateNativeType(nullptr)) {
  SetupBaseContext();
}

GLManager::~GLManager() {
  --use_count_;
  if (!use_count_) {
    if (base_share_group_) {
      delete base_share_group_;
      base_share_group_ = nullptr;
    }
    if (base_surface_) {
      delete base_surface_;
      base_surface_ = nullptr;
    }
    if (base_context_) {
      delete base_context_;
      base_context_ = nullptr;
    }
  }
}

std::unique_ptr<gfx::GpuMemoryBuffer> GLManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format) {
#if defined(OS_MACOSX)
  if (use_iosurface_memory_buffers_) {
    return base::WrapUnique<gfx::GpuMemoryBuffer>(
        new IOSurfaceGpuMemoryBuffer(size, format));
  }
#endif  // defined(OS_MACOSX)
  std::vector<uint8_t> data(gfx::BufferSizeForBufferFormat(size, format), 0);
  auto bytes = base::RefCountedBytes::TakeVector(&data);
  return base::WrapUnique<gfx::GpuMemoryBuffer>(
      new GpuMemoryBufferImpl(bytes.get(), size, format));
}

void GLManager::Initialize(const GLManager::Options& options) {
  GpuDriverBugWorkarounds platform_workarounds(
      g_gpu_feature_info.enabled_gpu_driver_bug_workarounds);
  InitializeWithWorkaroundsImpl(options, platform_workarounds);
}

void GLManager::InitializeWithWorkarounds(
    const GLManager::Options& options,
    const GpuDriverBugWorkarounds& workarounds) {
  GpuDriverBugWorkarounds combined_workarounds(
      g_gpu_feature_info.enabled_gpu_driver_bug_workarounds);
  combined_workarounds.Append(workarounds);
  InitializeWithWorkaroundsImpl(options, combined_workarounds);
}

void GLManager::InitializeWithWorkaroundsImpl(
    const GLManager::Options& options,
    const GpuDriverBugWorkarounds& workarounds) {
  const SharedMemoryLimits limits = options.shared_memory_limits;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  DCHECK(!command_line.HasSwitch(switches::kDisableGLExtensions));
  InitializeGpuPreferencesForTestingFromCommandLine(command_line,
                                                    &gpu_preferences_);

  context_type_ = options.context_type;
  if (options.share_mailbox_manager) {
    mailbox_manager_ = options.share_mailbox_manager->mailbox_manager();
  } else if (options.share_group_manager) {
    mailbox_manager_ = options.share_group_manager->mailbox_manager();
  } else {
    mailbox_manager_ = &owned_mailbox_manager_;
  }

  gl::GLShareGroup* share_group = nullptr;
  if (options.share_group_manager) {
    share_group = options.share_group_manager->share_group();
  } else if (options.share_mailbox_manager) {
    share_group = options.share_mailbox_manager->share_group();
  }

  gles2::ContextGroup* context_group = nullptr;
  scoped_refptr<gles2::ShareGroup> client_share_group;
  if (options.share_group_manager) {
    context_group = options.share_group_manager->decoder_->GetContextGroup();
    client_share_group =
      options.share_group_manager->gles2_implementation()->share_group();
  }

  gl::GLContext* real_gl_context = nullptr;
  if (options.virtual_manager &&
      !gpu_preferences_.use_passthrough_cmd_decoder) {
    real_gl_context = options.virtual_manager->context();
  }

  share_group_ = share_group ? share_group : new gl::GLShareGroup;

  ContextCreationAttribs attribs;
  attribs.red_size = 8;
  attribs.green_size = 8;
  attribs.blue_size = 8;
  attribs.alpha_size = 8;
  attribs.depth_size = 16;
  attribs.stencil_size = 8;
  attribs.context_type = options.context_type;
  attribs.samples = options.multisampled ? 4 : 0;
  attribs.sample_buffers = options.multisampled ? 1 : 0;
  attribs.alpha_size = options.backbuffer_alpha ? 8 : 0;
  attribs.should_use_native_gmb_for_backbuffer =
      options.image_factory != nullptr;
  attribs.offscreen_framebuffer_size = options.size;
  attribs.buffer_preserved = options.preserve_backbuffer;
  attribs.bind_generates_resource = options.bind_generates_resource;
  translator_cache_ =
      std::make_unique<gles2::ShaderTranslatorCache>(gpu_preferences_);

  if (!context_group) {
    GpuFeatureInfo gpu_feature_info;
    scoped_refptr<gles2::FeatureInfo> feature_info =
        new gles2::FeatureInfo(workarounds, gpu_feature_info);
    // Always mark the passthrough command decoder as supported so that tests do
    // not unexpectedly use the wrong command decoder
    context_group = new gles2::ContextGroup(
        gpu_preferences_, true, mailbox_manager_, nullptr /* memory_tracker */,
        translator_cache_.get(), &completeness_cache_, feature_info,
        options.bind_generates_resource, &image_manager_, options.image_factory,
        nullptr /* progress_reporter */, gpu_feature_info,
        &discardable_manager_, &passthrough_discardable_manager_,
        &shared_image_manager_);
  }

  command_buffer_.reset(
      new CommandBufferCheckLostContext(options.context_lost_allowed));

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(
      command_buffer_.get(), command_buffer_->service(), &outputter_,
      context_group));
  if (options.force_shader_name_hashing) {
    decoder_->SetForceShaderNameHashingForTest(true);
  }

  command_buffer_->set_handler(decoder_.get());

  surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
  ASSERT_TRUE(surface_.get() != nullptr)
      << "could not create offscreen surface";

  if (base_context_) {
    context_ = scoped_refptr<gl::GLContext>(new gpu::GLContextVirtual(
        share_group_.get(), base_context_->get(), decoder_->AsWeakPtr()));
    ASSERT_TRUE(context_->Initialize(
        surface_.get(), GenerateGLContextAttribs(attribs, context_group)));
  } else {
    if (real_gl_context) {
      context_ = scoped_refptr<gl::GLContext>(new gpu::GLContextVirtual(
          share_group_.get(), real_gl_context, decoder_->AsWeakPtr()));
      ASSERT_TRUE(context_->Initialize(
          surface_.get(), GenerateGLContextAttribs(attribs, context_group)));
    } else {
      context_ = gl::init::CreateGLContext(
          share_group_.get(), surface_.get(),
          GenerateGLContextAttribs(attribs, context_group));
      g_gpu_feature_info.ApplyToGLContext(context_.get());
    }
  }
  ASSERT_TRUE(context_.get() != nullptr) << "could not create GL context";

  ASSERT_TRUE(context_->MakeCurrent(surface_.get()));

  auto result =
      decoder_->Initialize(surface_.get(), context_.get(), true,
                           ::gpu::gles2::DisallowedFeatures(), attribs);
  if (result != gpu::ContextResult::kSuccess)
    return;
  // Client side Capabilities queries return reference, service side return
  // value. Here two sides are joined together.
  capabilities_ = decoder_->GetCapabilities();

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_.reset(new gles2::GLES2CmdHelper(command_buffer_.get()));
  ASSERT_EQ(gles2_helper_->Initialize(limits.command_buffer_size),
            gpu::ContextResult::kSuccess);

  // Create a transfer buffer.
  transfer_buffer_.reset(new TransferBuffer(gles2_helper_.get()));

  // Create the object exposing the OpenGL API.
  const bool support_client_side_arrays = true;
  gles2_implementation_.reset(new gles2::GLES2Implementation(
      gles2_helper_.get(), std::move(client_share_group),
      transfer_buffer_.get(), options.bind_generates_resource,
      options.lose_context_when_out_of_memory, support_client_side_arrays,
      this));

  ASSERT_EQ(gles2_implementation_->Initialize(limits),
            gpu::ContextResult::kSuccess)
      << "Could not init GLES2Implementation";

  MakeCurrent();
}

size_t GLManager::GetSharedMemoryBytesAllocated() const {
  return command_buffer_->service()->GetSharedMemoryBytesAllocated();
}

void GLManager::SetupBaseContext() {
  if (use_count_) {
    #if defined(OS_ANDROID)
    base_share_group_ =
        new scoped_refptr<gl::GLShareGroup>(new gl::GLShareGroup);
    gfx::Size size(4, 4);
    base_surface_ = new scoped_refptr<gl::GLSurface>(
        gl::init::CreateOffscreenGLSurface(size));
    base_context_ = new scoped_refptr<gl::GLContext>(gl::init::CreateGLContext(
        base_share_group_->get(), base_surface_->get(),
        gl::GLContextAttribs()));
    g_gpu_feature_info.ApplyToGLContext(base_context_->get());
    #endif
  }
  ++use_count_;
}

void GLManager::MakeCurrent() {
  ::gles2::SetGLContext(gles2_implementation_.get());
  if (!decoder_->MakeCurrent())
    command_buffer_->service()->SetParseError(error::kLostContext);
}

void GLManager::SetSurface(gl::GLSurface* surface) {
  decoder_->SetSurface(surface);
  MakeCurrent();
}

void GLManager::PerformIdleWork() {
  decoder_->PerformIdleWork();
}

void GLManager::Destroy() {
  if (gles2_implementation_.get()) {
    MakeCurrent();
    EXPECT_TRUE(glGetError() == GL_NONE);
    gles2_implementation_->Flush();
    gles2_implementation_.reset();
  }
  transfer_buffer_.reset();
  gles2_helper_.reset();
  if (decoder_.get()) {
    bool have_context = decoder_->GetGLContext() &&
                        decoder_->GetGLContext()->MakeCurrent(surface_.get());
    decoder_->Destroy(have_context);
    decoder_.reset();
  }
  command_buffer_.reset();
  context_ = nullptr;
}

const GpuDriverBugWorkarounds& GLManager::workarounds() const {
  return decoder_->GetContextGroup()->feature_info()->workarounds();
}

void GLManager::SetGpuControlClient(GpuControlClient*) {
  // The client is not currently called, so don't store it.
}

const Capabilities& GLManager::GetCapabilities() const {
  return capabilities_;
}

int32_t GLManager::CreateImage(ClientBuffer buffer,
                               size_t width,
                               size_t height) {
  gfx::Size size(width, height);
  scoped_refptr<gl::GLImage> gl_image;

#if defined(OS_MACOSX)
  if (use_iosurface_memory_buffers_) {
    IOSurfaceGpuMemoryBuffer* gpu_memory_buffer =
        IOSurfaceGpuMemoryBuffer::FromClientBuffer(buffer);
    unsigned internalformat =
        gl::BufferFormatToGLInternalFormat(gpu_memory_buffer->GetFormat());
    scoped_refptr<gl::GLImageIOSurface> image(
        gl::GLImageIOSurface::Create(size, internalformat));
    if (!image->Initialize(gpu_memory_buffer->iosurface(),
                           gfx::GenericSharedMemoryId(1),
                           gfx::BufferFormat::BGRA_8888)) {
      return -1;
    }
    gl_image = image;
  }
#endif  // defined(OS_MACOSX)

  if (use_native_pixmap_memory_buffers_) {
    gfx::GpuMemoryBuffer* gpu_memory_buffer =
        reinterpret_cast<gfx::GpuMemoryBuffer*>(buffer);
    DCHECK(gpu_memory_buffer);
    if (gpu_memory_buffer->GetType() == gfx::NATIVE_PIXMAP) {
      gfx::GpuMemoryBufferHandle handle = gpu_memory_buffer->CloneHandle();
      gfx::BufferFormat format = gpu_memory_buffer->GetFormat();
      gl_image = gpu_memory_buffer_factory_->AsImageFactory()
                     ->CreateImageForGpuMemoryBuffer(
                         std::move(handle), size, format,
                         gpu::kInProcessCommandBufferClientId,
                         gpu::kNullSurfaceHandle);
      if (!gl_image)
        return -1;
    }
  }

  if (!gl_image) {
    GpuMemoryBufferImpl* gpu_memory_buffer =
        GpuMemoryBufferImpl::FromClientBuffer(buffer);

    gfx::BufferFormat format = gpu_memory_buffer->GetFormat();
    auto image = base::MakeRefCounted<gl::GLImageRefCountedMemory>(size);
    if (!image->Initialize(gpu_memory_buffer->bytes(), format)) {
      return -1;
    }
    gl_image = image;
  }

  static int32_t next_id = 1;
  int32_t new_id = next_id++;
  image_manager_.AddImage(gl_image.get(), new_id);
  return new_id;
}

void GLManager::DestroyImage(int32_t id) {
  image_manager_.RemoveImage(id);
}

void GLManager::SignalQuery(uint32_t query, base::OnceClosure callback) {
  NOTREACHED();
}

void GLManager::CreateGpuFence(uint32_t gpu_fence_id, ClientGpuFence source) {
  NOTREACHED();
}

void GLManager::GetGpuFence(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  NOTREACHED();
}

void GLManager::SetLock(base::Lock*) {
  NOTREACHED();
}

void GLManager::EnsureWorkVisible() {
  NOTREACHED();
}

gpu::CommandBufferNamespace GLManager::GetNamespaceID() const {
  return CommandBufferNamespace::INVALID;
}

CommandBufferId GLManager::GetCommandBufferID() const {
  return CommandBufferId();
}

void GLManager::FlushPendingWork() {
  NOTREACHED();
}

uint64_t GLManager::GenerateFenceSyncRelease() {
  NOTREACHED();
  return 0;
}

bool GLManager::IsFenceSyncReleased(uint64_t release) {
  NOTREACHED();
  return false;
}

void GLManager::SignalSyncToken(const gpu::SyncToken& sync_token,
                                base::OnceClosure callback) {
  NOTREACHED();
}

void GLManager::WaitSyncToken(const gpu::SyncToken& sync_token) {
  NOTREACHED();
}

bool GLManager::CanWaitUnverifiedSyncToken(const gpu::SyncToken& sync_token) {
  NOTREACHED();
  return false;
}

void GLManager::SetDisplayTransform(gfx::OverlayTransform transform) {
  NOTREACHED();
}

ContextType GLManager::GetContextType() const {
  return context_type_;
}

void GLManager::Reset() {
  Destroy();
  SetupBaseContext();
}
}  // namespace gpu

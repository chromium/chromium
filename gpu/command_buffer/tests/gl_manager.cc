// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/tests/gl_manager.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_direct.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_MAC)
#include "ui/gfx/mac/io_surface.h"
#endif

namespace gpu {
namespace {

void InitializeGpuPreferencesForTestingFromCommandLine(
    const base::CommandLine& command_line,
    GpuPreferences* preferences) {
  // Only initialize specific GpuPreferences members used for testing.
  preferences->use_passthrough_cmd_decoder =
      gles2::UsePassthroughCommandDecoder(&command_line);
  preferences->enable_gpu_service_logging_gpu =
      command_line.HasSwitch(switches::kEnableGPUServiceLoggingGPU);
}

class GpuMemoryBufferImplTest : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImplTest(base::RefCountedBytes* bytes,
                          const gfx::Size& size,
                          gfx::BufferFormat format)
      : mapped_(false), bytes_(bytes), size_(size), format_(format) {}

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
    DCHECK(!mapped_);
    mapped_ = true;
    return true;
  }
  void* memory(size_t plane) override {
    DCHECK(mapped_);
    DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return bytes_->as_vector().data() +
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
    NOTREACHED_IN_MIGRATION();
    return gfx::GpuMemoryBufferId(0);
  }
  gfx::GpuMemoryBufferType GetType() const override {
    return gfx::NATIVE_PIXMAP;
  }
  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    NOTREACHED_IN_MIGRATION();
    return gfx::GpuMemoryBufferHandle();
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

#if BUILDFLAG(IS_MAC)
class IOSurfaceGpuMemoryBuffer : public gfx::GpuMemoryBuffer {
 public:
  IOSurfaceGpuMemoryBuffer(const gfx::Size& size, gfx::BufferFormat format)
      : mapped_(false), size_(size), format_(format) {
    iosurface_ = gfx::CreateIOSurface(size, gfx::BufferFormat::BGRA_8888);
  }

  ~IOSurfaceGpuMemoryBuffer() override = default;

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
    DCHECK(!mapped_);
    mapped_ = true;
    return true;
  }
  void* memory(size_t plane) override {
    DCHECK(mapped_);
    DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return IOSurfaceGetBaseAddressOfPlane(iosurface_.get(), plane);
  }
  void Unmap() override {
    DCHECK(mapped_);
    mapped_ = false;
  }
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetFormat() const override { return format_; }
  int stride(size_t plane) const override {
    DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return IOSurfaceGetWidthOfPlane(iosurface_.get(), plane);
  }
  gfx::GpuMemoryBufferId GetId() const override {
    NOTREACHED_IN_MIGRATION();
    return gfx::GpuMemoryBufferId(0);
  }
  gfx::GpuMemoryBufferType GetType() const override {
    return gfx::IO_SURFACE_BUFFER;
  }
  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    NOTREACHED_IN_MIGRATION();
    return gfx::GpuMemoryBufferHandle();
  }
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {}

  IOSurfaceRef iosurface() { return iosurface_.get(); }

 private:
  bool mapped_;
  base::apple::ScopedCFTypeRef<IOSurfaceRef> iosurface_;
  const gfx::Size size_;
  gfx::BufferFormat format_;
};
#endif  // BUILDFLAG(IS_MAC)

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
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  InitializeGpuPreferencesForTestingFromCommandLine(command_line,
                                                    &gpu_preferences_);
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
#if BUILDFLAG(IS_MAC)
  if (use_iosurface_memory_buffers_) {
    return base::WrapUnique<gfx::GpuMemoryBuffer>(
        new IOSurfaceGpuMemoryBuffer(size, format));
  }
#endif  // BUILDFLAG(IS_MAC)
  std::vector<uint8_t> data(gfx::BufferSizeForBufferFormat(size, format), 0);
  auto bytes = base::RefCountedBytes::TakeVector(&data);
  return base::WrapUnique<gfx::GpuMemoryBuffer>(
      new GpuMemoryBufferImplTest(bytes.get(), size, format));
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

  context_type_ = options.context_type;

  gl::GLShareGroup* share_group = nullptr;
  if (options.share_group_manager) {
    share_group = options.share_group_manager->share_group();
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
  attribs.context_type = options.context_type;
  attribs.bind_generates_resource = options.bind_generates_resource;

  translator_cache_ =
      std::make_unique<gles2::ShaderTranslatorCache>(gpu_preferences_);
  discardable_manager_ =
      std::make_unique<ServiceDiscardableManager>(gpu_preferences_);
  passthrough_discardable_manager_ =
      std::make_unique<PassthroughDiscardableManager>(gpu_preferences_);

  if (!context_group) {
    GpuFeatureInfo gpu_feature_info;
    scoped_refptr<gles2::FeatureInfo> feature_info =
        new gles2::FeatureInfo(workarounds, gpu_feature_info);
    // Always mark the passthrough command decoder as supported so that tests do
    // not unexpectedly use the wrong command decoder
    context_group = new gles2::ContextGroup(
        gpu_preferences_, true, nullptr /* memory_tracker */,
        translator_cache_.get(), &completeness_cache_, feature_info,
        options.bind_generates_resource, nullptr /* progress_reporter */,
        gpu_feature_info, discardable_manager_.get(),
        passthrough_discardable_manager_.get(), &shared_image_manager_);
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

  auto surface = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                                    gfx::Size());
  ASSERT_TRUE(surface.get() != nullptr) << "could not create offscreen surface";

  if (base_context_) {
    context_ = scoped_refptr<gl::GLContext>(new gpu::GLContextVirtual(
        share_group_.get(), base_context_->get(), decoder_->AsWeakPtr()));
    ASSERT_TRUE(context_->Initialize(
        surface.get(),
        GenerateGLContextAttribsForDecoder(attribs, context_group)));
  } else {
    if (real_gl_context) {
      context_ = scoped_refptr<gl::GLContext>(new gpu::GLContextVirtual(
          share_group_.get(), real_gl_context, decoder_->AsWeakPtr()));
      ASSERT_TRUE(context_->Initialize(
          surface.get(),
          GenerateGLContextAttribsForDecoder(attribs, context_group)));
    } else {
      context_ = gl::init::CreateGLContext(
          share_group_.get(), surface.get(),
          GenerateGLContextAttribsForDecoder(attribs, context_group));
      g_gpu_feature_info.ApplyToGLContext(context_.get());
    }
  }
  ASSERT_TRUE(context_.get() != nullptr) << "could not create GL context";
  ASSERT_TRUE(context_->default_surface() == surface.get());
  ASSERT_TRUE(context_->MakeCurrentDefault());

  // if (gpu_preferences_.use_passthrough_cmd_decoder) {
  //   auto* apit = g_current_gl_context;
  //   api->glRequestExtensionANGLEFn("GL_EXT_read_format_bgra");
  //   api->glRequestExtensionANGLEFn("GL_EXT_texture_format_BGRA8888");
  // }

  auto result =
      decoder_->Initialize(context_->default_surface(), context_.get(), true,
                           ::gpu::gles2::DisallowedFeatures(), attribs);
  if (result != gpu::ContextResult::kSuccess)
    return;
  // Client side Capabilities queries return reference, service side return
  // value. Here two sides are joined together.
  capabilities_ = decoder_->GetCapabilities();
  gl_capabilities_ = decoder_->GetGLCapabilities();

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

  // Initialize FBO for drawing
  if (!options.size.IsEmpty()) {
    GLuint color, depth_stencil;
    gles2_implementation_->GenTextures(1, &color);
    gles2_implementation_->BindTexture(GL_TEXTURE_2D, color);
    gles2_implementation_->TexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, options.size.width(), options.size.height(),
        0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    gles2_implementation_->BindTexture(GL_TEXTURE_2D, 0);

    gles2_implementation_->GenRenderbuffers(1, &depth_stencil);
    gles2_implementation_->BindRenderbuffer(GL_RENDERBUFFER, depth_stencil);
    gles2_implementation_->RenderbufferStorage(
        GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, options.size.width(),
        options.size.height());
    gles2_implementation_->BindRenderbuffer(GL_RENDERBUFFER, 0);

    gles2_implementation_->GenFramebuffers(1, &fbo_);
    gles2_implementation_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gles2_implementation_->FramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    EXPECT_TRUE(glGetError() == GL_NONE);

    // WebGL requires GL_DEPTH_STENCIL_ATTACHMENT
    if (context_type_ == CONTEXT_TYPE_WEBGL1 ||
        context_type_ == CONTEXT_TYPE_WEBGL2) {
      gles2_implementation_->FramebufferRenderbuffer(
          GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
          depth_stencil);
    } else {
      gles2_implementation_->FramebufferRenderbuffer(
          GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_stencil);
      gles2_implementation_->FramebufferRenderbuffer(
          GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
          depth_stencil);
    }

    gles2_implementation_->Viewport(0, 0, options.size.width(),
                                    options.size.height());

    gles2_implementation_->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                                 GL_STENCIL_BUFFER_BIT);
  }

  EXPECT_TRUE(glGetError() == GL_NONE);
}

void GLManager::BindOffscreenFramebuffer(GLenum target) {
  gles2_implementation_->BindFramebuffer(target, fbo_);
}

size_t GLManager::GetSharedMemoryBytesAllocated() const {
  return command_buffer_->service()->GetSharedMemoryBytesAllocated();
}

void GLManager::SetupBaseContext() {
  if (!use_count_) {
#if BUILDFLAG(IS_ANDROID)
    // Virtual contexts is not necessary with passthrough.
    if (!gpu_preferences_.use_passthrough_cmd_decoder) {
      base_share_group_ =
          new scoped_refptr<gl::GLShareGroup>(new gl::GLShareGroup);
      gfx::Size size(4, 4);
      base_surface_ = new scoped_refptr<gl::GLSurface>(
          gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), size));
      base_context_ =
          new scoped_refptr<gl::GLContext>(gl::init::CreateGLContext(
              base_share_group_->get(), base_surface_->get(),
              gl::GLContextAttribs()));
      g_gpu_feature_info.ApplyToGLContext(base_context_->get());
    }
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
    bool have_context =
        decoder_->GetGLContext() &&
        decoder_->GetGLContext()->MakeCurrent(context_->default_surface());
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

const GLCapabilities& GLManager::GetGLCapabilities() const {
  return gl_capabilities_;
}

void GLManager::SignalQuery(uint32_t query, base::OnceClosure callback) {
  NOTREACHED_IN_MIGRATION();
}

void GLManager::CancelAllQueries() {
  NOTREACHED_IN_MIGRATION();
}

void GLManager::CreateGpuFence(uint32_t gpu_fence_id, ClientGpuFence source) {
  NOTREACHED_IN_MIGRATION();
}

void GLManager::GetGpuFence(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  NOTREACHED_IN_MIGRATION();
}

void GLManager::SetLock(base::Lock*) {
  NOTREACHED_IN_MIGRATION();
}

void GLManager::EnsureWorkVisible() {
  NOTREACHED_IN_MIGRATION();
}

gpu::CommandBufferNamespace GLManager::GetNamespaceID() const {
  return CommandBufferNamespace::INVALID;
}

CommandBufferId GLManager::GetCommandBufferID() const {
  return CommandBufferId();
}

void GLManager::FlushPendingWork() {
  NOTREACHED_IN_MIGRATION();
}

uint64_t GLManager::GenerateFenceSyncRelease() {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

bool GLManager::IsFenceSyncReleased(uint64_t release) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void GLManager::SignalSyncToken(const gpu::SyncToken& sync_token,
                                base::OnceClosure callback) {
  NOTREACHED_IN_MIGRATION();
}

void GLManager::WaitSyncToken(const gpu::SyncToken& sync_token) {
  NOTREACHED_IN_MIGRATION();
}

bool GLManager::CanWaitUnverifiedSyncToken(const gpu::SyncToken& sync_token) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

ContextType GLManager::GetContextType() const {
  return context_type_;
}

void GLManager::Reset() {
  Destroy();
  SetupBaseContext();
}
}  // namespace gpu

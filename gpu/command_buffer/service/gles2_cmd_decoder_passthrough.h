// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GLES2DecoderPassthroughImpl class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_H_

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/client_service_map.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_switching_observer.h"

namespace gl {
class GLFence;
class ProgressReporter;
}

namespace gpu {
class GLTexturePassthroughImageRepresentation;

namespace gles2 {

class ContextGroup;
class GPUTracer;
class MultiDrawManager;
class GLES2DecoderPassthroughImpl;
class GLES2ExternalFramebuffer;

struct MappedBuffer {
  GLsizeiptr size;
  GLbitfield original_access;
  GLbitfield filtered_access;
  raw_ptr<uint8_t, DanglingUntriaged | AllowPtrArithmetic> map_ptr;
  int32_t data_shm_id;
  uint32_t data_shm_offset;
};

struct PassthroughResources {
  PassthroughResources();
  ~PassthroughResources();

  // api is null if we don't have a context (e.g. lost).
  void Destroy(gl::GLApi* api, gl::ProgressReporter* progress_reporter);

  void SuspendSharedImageAccessIfNeeded();
  bool ResumeSharedImageAccessIfNeeded(gl::GLApi* api);

  // Mappings from client side IDs to service side IDs.
  ClientServiceMap<GLuint, GLuint> texture_id_map;
  ClientServiceMap<GLuint, GLuint> buffer_id_map;
  ClientServiceMap<GLuint, GLuint> renderbuffer_id_map;
  ClientServiceMap<GLuint, GLuint> sampler_id_map;
  ClientServiceMap<GLuint, GLuint> program_id_map;
  ClientServiceMap<GLuint, GLuint> shader_id_map;

  static_assert(sizeof(uintptr_t) == sizeof(GLsync),
                "GLsync not the same size as uintptr_t");
  ClientServiceMap<GLuint, uintptr_t> sync_id_map;

  // Mapping of client texture IDs to TexturePassthrough objects used to make
  // sure all textures used by mailboxes are not deleted until all textures
  // using the mailbox are deleted
  ClientServiceMap<GLuint, scoped_refptr<TexturePassthrough>>
      texture_object_map;

  class SharedImageData {
   public:
    SharedImageData();
    explicit SharedImageData(
        const GLES2DecoderPassthroughImpl*,
        std::unique_ptr<GLTexturePassthroughImageRepresentation>);
    SharedImageData(SharedImageData&& other);

    SharedImageData(const SharedImageData&) = delete;
    SharedImageData& operator=(const SharedImageData&) = delete;

    ~SharedImageData();
    SharedImageData& operator=(SharedImageData&& other);

    GLTexturePassthroughImageRepresentation* representation() const {
      return representation_.get();
    }

    // Returns true between a successful BeginAccess and the following EndAccess
    // even if access is currently suspended.
    bool is_being_accessed() const { return access_mode_.has_value(); }

    void EnsureClear(const GLES2DecoderPassthroughImpl*);

    bool BeginAccess(GLenum mode, gl::GLApi* api);
    void EndAccess();

    bool ResumeAccessIfNeeded(gl::GLApi* api);
    void SuspendAccessIfNeeded();

   private:
    std::unique_ptr<GLTexturePassthroughImageRepresentation> representation_;
    std::unique_ptr<GLTexturePassthroughImageRepresentation::ScopedAccess>
        scoped_access_;
    std::optional<GLenum> access_mode_;
  };
  // Mapping of client texture IDs to GLTexturePassthroughImageRepresentations.
  // TODO(ericrk): Remove this once TexturePassthrough holds a reference to
  // the GLTexturePassthroughImageRepresentation itself.
  base::flat_map<GLuint, SharedImageData> texture_shared_image_map;

  // Mapping of client buffer IDs that are mapped to the shared memory used to
  // back the mapping so that it can be flushed when the buffer is unmapped
  base::flat_map<GLuint, MappedBuffer> mapped_buffer_map;
};

// Impose an upper bound on the number ANGLE_shader_pixel_local_storage planes
// so we can stack-allocate load/store ops.
static constexpr GLsizei kPassthroughMaxPLSPlanes = 8;

class GPU_GLES2_EXPORT GLES2DecoderPassthroughImpl
    : public GLES2Decoder,
      public ui::GpuSwitchingObserver {
 public:
  GLES2DecoderPassthroughImpl(DecoderClient* client,
                              CommandBufferServiceBase* command_buffer_service,
                              Outputter* outputter,
                              ContextGroup* group);
  ~GLES2DecoderPassthroughImpl() override;

  Error DoCommands(unsigned int num_commands,
                   const volatile void* buffer,
                   int num_entries,
                   int* entries_processed) override;

  template <bool DebugImpl>
  Error DoCommandsImpl(unsigned int num_commands,
                       const volatile void* buffer,
                       int num_entries,
                       int* entries_processed);

  base::WeakPtr<DecoderContext> AsWeakPtr() override;

  gpu::ContextResult Initialize(
      const scoped_refptr<gl::GLSurface>& surface,
      const scoped_refptr<gl::GLContext>& context,
      bool offscreen,
      const DisallowedFeatures& disallowed_features,
      const ContextCreationAttribs& attrib_helper) override;

  // Destroys the graphics context.
  void Destroy(bool have_context) override;

  // Set the surface associated with the default FBO.
  void SetSurface(const scoped_refptr<gl::GLSurface>& surface) override;

  // Releases the surface associated with the GL context.
  // The decoder should not be used until a new surface is set.
  void ReleaseSurface() override;

  void SetDefaultFramebufferSharedImage(const Mailbox& mailbox,
                                        int samples,
                                        bool preserve,
                                        bool needs_depth,
                                        bool needs_stencil) override;

  // Make this decoder's GL context current.
  bool MakeCurrent() override;

  gl::GLApi* api() const { return api_; }

  // Gets the GLES2 Util which holds info.
  GLES2Util* GetGLES2Util() override;

  // Gets the associated GLContext and GLSurface.
  gl::GLContext* GetGLContext() override;
  gl::GLSurface* GetGLSurface() override;

  // Gets the associated ContextGroup
  ContextGroup* GetContextGroup() override;

  const FeatureInfo* GetFeatureInfo() const override;

  Capabilities GetCapabilities() override;

  GLCapabilities GetGLCapabilities() override;

  // Restores all of the decoder GL state.
  void RestoreState(const ContextState* prev_state) override;

  // Restore States.
  void RestoreActiveTexture() const override;
  void RestoreAllTextureUnitAndSamplerBindings(
      const ContextState* prev_state) const override;
  void RestoreActiveTextureUnitBinding(unsigned int target) const override;
  void RestoreBufferBinding(unsigned int target) override;
  void RestoreBufferBindings() const override;
  void RestoreFramebufferBindings() const override;
  void RestoreRenderbufferBindings() override;
  void RestoreGlobalState() const override;
  void RestoreProgramBindings() const override;
  void RestoreTextureState(unsigned service_id) override;
  void RestoreTextureUnitBindings(unsigned unit) const override;
  void RestoreVertexAttribArray(unsigned index) override;
  void RestoreAllExternalTextureBindingsIfNeeded() override;
  void RestoreDeviceWindowRectangles() const override;

  void ClearAllAttributes() const override;
  void RestoreAllAttributes() const override;

  void SetIgnoreCachedStateForTest(bool ignore) override;
  void SetForceShaderNameHashingForTest(bool force) override;

  // Gets the QueryManager for this context.
  QueryManager* GetQueryManager() override;

  // Set a callback to be called when a query is complete.
  void SetQueryCallback(unsigned int query_client_id,
                        base::OnceClosure callback) override;

  void CancelAllQueries() override;

  // Gets the GpuFenceManager for this context.
  GpuFenceManager* GetGpuFenceManager() override;

  // Gets the FramebufferManager for this context.
  FramebufferManager* GetFramebufferManager() override;

  // Gets the TransformFeedbackManager for this context.
  TransformFeedbackManager* GetTransformFeedbackManager() override;

  // Gets the VertexArrayManager for this context.
  VertexArrayManager* GetVertexArrayManager() override;

  // Returns false if there are no pending queries.
  bool HasPendingQueries() const override;

  // Process any pending queries.
  void ProcessPendingQueries(bool did_finish) override;

  // Returns false if there is no idle work to be made.
  bool HasMoreIdleWork() const override;

  // Perform any idle work that needs to be made.
  void PerformIdleWork() override;

  bool HasPollingWork() const override;
  void PerformPollingWork() override;

  bool GetServiceTextureId(uint32_t client_texture_id,
                           uint32_t* service_texture_id) override;
  TextureBase* GetTextureBase(uint32_t client_id) override;

  // Provides detail about a lost context if one occurred.
  // Clears a level sub area of a texture
  // Returns false if a GL error should be generated.
  bool ClearLevel(Texture* texture,
                  unsigned target,
                  int level,
                  unsigned format,
                  unsigned type,
                  int xoffset,
                  int yoffset,
                  int width,
                  int height) override;

  // Clears a level sub area of a compressed 2D texture.
  // Returns false if a GL error should be generated.
  bool ClearCompressedTextureLevel(Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   int width,
                                   int height) override;

  // Clears a level sub area of a compressed 3D texture.
  // Returns false if a GL error should be generated.
  bool ClearCompressedTextureLevel3D(Texture* texture,
                                     unsigned target,
                                     int level,
                                     unsigned format,
                                     int width,
                                     int height,
                                     int depth) override;

  // Indicates whether a given internal format is one for a compressed
  // texture.
  bool IsCompressedTextureFormat(unsigned format) override;

  // Clears a level of a 3D texture.
  // Returns false if a GL error should be generated.
  bool ClearLevel3D(Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override;

  ErrorState* GetErrorState() override;

  void WaitForReadPixels(base::OnceClosure callback) override;

  // Returns true if the context was lost either by GL_ARB_robustness, forced
  // context loss or command buffer parse error.
  bool WasContextLost() const override;

  // Returns true if the context was lost specifically by GL_ARB_robustness.
  bool WasContextLostByRobustnessExtension() const override;

  // Lose this context.
  void MarkContextLost(error::ContextLostReason reason) override;

  // Update lost context state for use when making calls to the GL context
  // directly, and needing to know if they failed due to loss.
  bool CheckResetStatus() override;

  // Implement GpuSwitchingObserver.
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) override;

  Logger* GetLogger() override;

  void BeginDecoding() override;
  void EndDecoding() override;

  const ContextState* GetContextState() override;
  scoped_refptr<ShaderTranslatorInterface> GetTranslator(GLenum type) override;

  void OnDebugMessage(GLenum source,
                      GLenum type,
                      GLuint id,
                      GLenum severity,
                      GLsizei length,
                      const GLchar* message);

  void SetCopyTextureResourceManagerForTest(
      CopyTextureCHROMIUMResourceManager* copy_texture_resource_manager)
      override;
  void SetCopyTexImageBlitterForTest(
      CopyTexImageResourceManager* copy_tex_image_blit) override;

  const FeatureInfo::FeatureFlags& features() const {
    return feature_info_->feature_flags();
  }

  class ScopedPixelLocalStorageInterrupt {
   public:
    ScopedPixelLocalStorageInterrupt(const GLES2DecoderPassthroughImpl*);
    ~ScopedPixelLocalStorageInterrupt();

   private:
    raw_ptr<const GLES2DecoderPassthroughImpl> impl_;
  };

 private:
  // Allow unittests to inspect internal state tracking
  friend class GLES2DecoderPassthroughTestBase;

  const char* GetCommandName(unsigned int command_id) const;

  void SetOptionalExtensionsRequestedForTesting(bool request_extensions);

  void InitializeFeatureInfo(ContextType context_type,
                             const DisallowedFeatures& disallowed_features,
                             bool force_reinitialize);

  template <typename T, typename GLGetFunction>
  error::Error GetNumericHelper(GLenum pname,
                                GLsizei bufsize,
                                GLsizei* length,
                                T* params,
                                GLGetFunction get_call) {
    get_call(pname, bufsize, length, params);

    // Update the results of the query, if needed
    const error::Error error = PatchGetNumericResults(pname, *length, params);
    if (error != error::kNoError) {
      *length = 0;
      return error;
    }

    return error::kNoError;
  }

  template <typename T>
  error::Error PatchGetNumericResults(GLenum pname, GLsizei length, T* params);
  error::Error PatchGetFramebufferAttachmentParameter(GLenum target,
                                                      GLenum attachment,
                                                      GLenum pname,
                                                      GLsizei length,
                                                      GLint* params);

  template <typename T>
  error::Error PatchGetBufferResults(GLenum target,
                                     GLenum pname,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     T* params);

  error::Error PatchGetFramebufferPixelLocalStorageParameterivANGLE(
      GLint plane,
      GLenum pname,
      GLsizei length,
      GLint* params);

  void InsertError(GLenum error, const std::string& message);
  GLenum PopError();
  bool FlushErrors();

  bool IsIgnoredCap(GLenum cap) const;

  bool IsEmulatedQueryTarget(GLenum target) const;
  error::Error ProcessQueries(bool did_finish);
  void RemovePendingQuery(GLuint service_id);

  struct BufferShadowUpdate;
  // BufferShadowUpdateMap's key is a buffer client id.
  using BufferShadowUpdateMap = base::flat_map<GLuint, BufferShadowUpdate>;
  void ReadBackBuffersIntoShadowCopies(const BufferShadowUpdateMap& updates);

  error::Error ProcessReadPixels(bool did_finish);

  // Checks to see if the inserted fence has completed.
  void ProcessDescheduleUntilFinished();

  void UpdateTextureBinding(GLenum target,
                            GLuint client_id,
                            TexturePassthrough* texture);
  void RebindTexture(TexturePassthrough* texture);

  void UpdateTextureSizeFromTexturePassthrough(TexturePassthrough* texture,
                                               GLuint client_id);
  void UpdateTextureSizeFromTarget(GLenum target);
  void UpdateTextureSizeFromClientID(GLuint client_id);

  // Some operations like binding a VAO will update the element array buffer
  // binding without an explicit glBindBuffer. This function is extremely
  // expensive, and it is crucial that it be called only when the command
  // decoder's notion of the element array buffer absolutely has to be
  // up-to-date.
  void LazilyUpdateCurrentlyBoundElementArrayBuffer();

  void VerifyServiceTextureObjectsExist();

  bool IsEmulatedFramebufferBound(GLenum target) const;

  void ExitCommandProcessingEarly() override;

  void CheckSwapBuffersAsyncResult(const char* function_name,
                                   uint64_t swap_id,
                                   gfx::SwapCompletionResult result);
  error::Error CheckSwapBuffersResult(gfx::SwapResult result,
                                      const char* function_name);

  bool OnlyHasPendingProgramCompletionQueries();

  int commands_to_process_;

  DebugMarkerManager debug_marker_manager_;
  Logger logger_;

#define GLES2_CMD_OP(name) \
  Error Handle##name(uint32_t immediate_data_size, const volatile void* data);

  GLES2_COMMAND_LIST(GLES2_CMD_OP)
#undef GLES2_CMD_OP

  using CmdHandler =
      Error (GLES2DecoderPassthroughImpl::*)(uint32_t immediate_data_size,
                                             const volatile void* data);

  // A struct to hold info about each command.
  struct CommandInfo {
    CmdHandler cmd_handler;
    uint8_t arg_flags;   // How to handle the arguments for this scommand
    uint8_t cmd_flags;   // How to handle this command
    uint16_t arg_count;  // How many arguments are expected for this command.
  };

  // A table of CommandInfo for all the commands.
  static const CommandInfo command_info[kNumCommands - kFirstGLES2Command];

  // The GLApi to make the gl calls on.
  raw_ptr<gl::GLApi> api_ = nullptr;

  // The GL context this decoder renders to on behalf of the client.
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  bool offscreen_;

  // The ContextGroup for this decoder uses to track resources.
  scoped_refptr<ContextGroup> group_;
  scoped_refptr<FeatureInfo> feature_info_;

  // By default, all requestable extensions should be loaded at initialization
  // time. Can be disabled for testing with only specific extensions enabled.
  bool request_optional_extensions_ = true;

  // Some objects may generate resources when they are bound even if they were
  // not generated yet: texture, buffer, renderbuffer, framebuffer, transform
  // feedback, vertex array
  bool bind_generates_resource_;

  // Mappings from client side IDs to service side IDs for shared objects
  raw_ptr<PassthroughResources> resources_ = nullptr;

  // Mappings from client side IDs to service side IDs for per-context objects
  ClientServiceMap<GLuint, GLuint> framebuffer_id_map_;
  ClientServiceMap<GLuint, GLuint> transform_feedback_id_map_;
  ClientServiceMap<GLuint, GLuint> query_id_map_;
  ClientServiceMap<GLuint, GLuint> vertex_array_id_map_;

  std::unique_ptr<GpuFenceManager> gpu_fence_manager_;

  std::unique_ptr<MultiDrawManager> multi_draw_manager_;

  // State tracking of currently bound 2D textures (client IDs)
  size_t active_texture_unit_;

  enum class TextureTarget : uint8_t {
    k2D = 0,
    kCubeMap = 1,
    k2DArray = 2,
    k3D = 3,
    k2DMultisample = 4,
    kExternal = 5,
    kRectangle = 6,
    kBuffer = 7,
    kCubeMapArray = 8,
    k2DMultisampleArray = 9,

    kUnkown = 10,
    kCount = kUnkown,
  };
  static TextureTarget GLenumToTextureTarget(GLenum target);

  struct BoundTexture {
    BoundTexture();
    ~BoundTexture();
    BoundTexture(const BoundTexture&);
    BoundTexture(BoundTexture&&);
    BoundTexture& operator=(const BoundTexture&);
    BoundTexture& operator=(BoundTexture&&);

    GLuint client_id = 0;
    scoped_refptr<TexturePassthrough> texture;
  };

  // Use a limit that is at least ANGLE's IMPLEMENTATION_MAX_ACTIVE_TEXTURES
  // constant
  static constexpr size_t kMaxTextureUnits = 64;
  static constexpr size_t kNumTextureTypes =
      static_cast<size_t>(TextureTarget::kCount);
  std::array<std::array<BoundTexture, kMaxTextureUnits>, kNumTextureTypes>
      bound_textures_;

  // State tracking of currently bound buffers
  base::flat_map<GLenum, GLuint> bound_buffers_;
  // Lazy tracking of the bound element array buffer when changing VAOs.
  bool bound_element_array_buffer_dirty_;

  // Track the service-id to type of all queries for validation
  struct QueryInfo {
    GLenum type = GL_NONE;
  };
  base::flat_map<GLuint, QueryInfo> query_info_map_;

  // All queries that are waiting for their results to be ready
  struct PendingQuery {
    PendingQuery();
    ~PendingQuery();
    PendingQuery(const PendingQuery&) = delete;
    PendingQuery(PendingQuery&&);
    PendingQuery& operator=(const PendingQuery&) = delete;
    PendingQuery& operator=(PendingQuery&&);

    GLenum target = GL_NONE;
    GLuint service_id = 0;

    scoped_refptr<gpu::Buffer> shm;
    raw_ptr<QuerySync> sync = nullptr;
    base::subtle::Atomic32 submit_count = 0;

    std::unique_ptr<gl::GLFence> commands_completed_fence;
    base::TimeDelta commands_issued_time;
    base::TimeTicks commands_issued_timestamp;

    std::vector<base::OnceClosure> callbacks;
    std::unique_ptr<gl::GLFence> buffer_shadow_update_fence;
    BufferShadowUpdateMap buffer_shadow_updates;
    GLuint program_service_id = 0u;
  };
  base::circular_deque<PendingQuery> pending_queries_;

  // Currently active queries
  struct ActiveQuery {
    ActiveQuery();
    ~ActiveQuery();
    ActiveQuery(const ActiveQuery&) = delete;
    ActiveQuery(ActiveQuery&&);
    ActiveQuery& operator=(const ActiveQuery&) = delete;
    ActiveQuery& operator=(ActiveQuery&&);

    GLuint service_id = 0;
    scoped_refptr<gpu::Buffer> shm;
    raw_ptr<QuerySync> sync = nullptr;

    // Time at which the commands for this query started processing. This is
    // used to ensure we only include the time when the decoder is scheduled in
    // the |active_time|. Used for GL_COMMANDS_ISSUED_CHROMIUM type query.
    base::TimeTicks command_processing_start_time;
    base::TimeDelta active_time;
  };
  base::flat_map<GLenum, ActiveQuery> active_queries_;

  // Pending async ReadPixels calls
  struct PendingReadPixels {
    PendingReadPixels();

    PendingReadPixels(const PendingReadPixels&) = delete;
    PendingReadPixels& operator=(const PendingReadPixels&) = delete;

    PendingReadPixels(PendingReadPixels&&);
    PendingReadPixels& operator=(PendingReadPixels&&);

    ~PendingReadPixels();

    std::unique_ptr<gl::GLFence> fence;
    GLuint buffer_service_id = 0;
    uint32_t pixels_size = 0;
    uint32_t pixels_shm_id = 0;
    uint32_t pixels_shm_offset = 0;
    uint32_t result_shm_id = 0;
    uint32_t result_shm_offset = 0;

    // Service IDs of GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM queries waiting for
    // this read pixels operation to complete
    base::flat_set<GLuint> waiting_async_pack_queries;
  };
  base::circular_deque<PendingReadPixels> pending_read_pixels_;

  struct BufferShadowUpdate {
    BufferShadowUpdate();

    BufferShadowUpdate(const BufferShadowUpdate&) = delete;
    BufferShadowUpdate& operator=(const BufferShadowUpdate&) = delete;

    BufferShadowUpdate(BufferShadowUpdate&&);
    BufferShadowUpdate& operator=(BufferShadowUpdate&&);

    ~BufferShadowUpdate();

    scoped_refptr<gpu::Buffer> shm;
    GLuint shm_offset = 0;
    GLuint size = 0;
  };
  BufferShadowUpdateMap buffer_shadow_updates_;

  // Error state
  base::flat_set<GLenum> errors_;

  // Checks if an error has been generated since the last call to
  // CheckErrorCallbackState
  bool CheckErrorCallbackState();
  bool had_error_callback_ = false;

  struct EmulatedDefaultFramebuffer {
    EmulatedDefaultFramebuffer(const GLES2DecoderPassthroughImpl*);

    EmulatedDefaultFramebuffer(const EmulatedDefaultFramebuffer&) = delete;
    EmulatedDefaultFramebuffer& operator=(const EmulatedDefaultFramebuffer&) =
        delete;

    ~EmulatedDefaultFramebuffer();

    gl::GLApi* api() const { return impl_->api(); }

    bool Initialize(const gfx::Size& size);
    void Destroy(bool have_context);

    raw_ptr<const GLES2DecoderPassthroughImpl> impl_;

    // Service ID of the framebuffer
    GLuint framebuffer_service_id = 0;

    // Color buffer texture
    scoped_refptr<TexturePassthrough> texture;
  };

  GLenum emulated_default_framebuffer_format_;
  std::unique_ptr<EmulatedDefaultFramebuffer> emulated_back_buffer_;
  std::unique_ptr<GLES2ExternalFramebuffer> external_default_framebuffer_;

  // Maximum 2D resource sizes for limiting offscreen framebuffer sizes
  GLint max_renderbuffer_size_ = 0;
  GLint max_offscreen_framebuffer_size_ = 0;

  // State tracking of currently bound draw and read framebuffers (client IDs)
  GLuint bound_draw_framebuffer_;
  GLuint bound_read_framebuffer_;

  // If this context supports both read and draw framebuffer bindings
  bool supports_separate_fbo_bindings_ = false;

  // Tracks if the context has ever called glBeginPixelLocalStorageANGLE. If it
  // has, we need to start using the pixel local storage interrupt mechanism.
  bool has_activated_pixel_local_storage_ = false;

  // Creates lazily and holds a SharedContextState on a GLContext that is in the
  // same share group as the command decoder's context. This is done so that
  // skia operations can be performed on textures from the context and not worry
  // about state tracking.
  class LazySharedContextState {
   public:
    static std::unique_ptr<LazySharedContextState> Create(
        GLES2DecoderPassthroughImpl* impl);

    explicit LazySharedContextState(GLES2DecoderPassthroughImpl* impl);
    ~LazySharedContextState();

    SharedContextState* shared_context_state() {
      return shared_context_state_.get();
    }

   private:
    bool Initialize();

    raw_ptr<GLES2DecoderPassthroughImpl> impl_ = nullptr;
    scoped_refptr<SharedContextState> shared_context_state_;
  };

  std::unique_ptr<LazySharedContextState> lazy_context_;
  // Tracing
  std::unique_ptr<GPUTracer> gpu_tracer_;
  raw_ptr<const unsigned char> gpu_decoder_category_ = nullptr;
  int gpu_trace_level_;
  bool gpu_trace_commands_;
  bool gpu_debug_commands_;

  // Context lost state
  bool context_lost_;
  bool reset_by_robustness_extension_;
  bool lose_context_when_out_of_memory_;

  // After a second fence is inserted, both the GpuChannelMessageQueue and
  // CommandExecutor are descheduled. Once the first fence has completed, both
  // get rescheduled.
  base::circular_deque<std::unique_ptr<gl::GLFence>>
      deschedule_until_finished_fences_;

  GLuint linking_program_service_id_ = 0u;

  base::WeakPtrFactory<GLES2DecoderPassthroughImpl> weak_ptr_factory_{this};

  class ScopedEnableTextureRectangleInShaderCompiler;

// Include the prototypes of all the doer functions from a separate header to
// keep this file clean.
#include "base/time/time.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough_doer_prototypes.h"
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_H_

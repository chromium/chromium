// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/client/buffer_tracker.h"
#include "gpu/command_buffer/client/client_context_state.h"
#include "gpu/command_buffer/client/client_transfer_cache.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_impl_export.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/implementation_base.h"
#include "gpu/command_buffer/client/logging.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/client/query_tracker.h"
#include "gpu/command_buffer/client/readback_buffer_shadow_tracker.h"
#include "gpu/command_buffer/client/ref_counted.h"
#include "gpu/command_buffer/client/share_group.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"

namespace gpu {

class IdAllocator;

namespace gles2 {

class GLES2CmdHelper;
class VertexArrayObjectManager;
class ReadbackBufferShadowTracker;

// This class emulates GLES2 over command buffers. It can be used by a client
// program so that the program does not need deal with shared memory and command
// buffer management. See gl2_lib.h.  Note that there is a performance gain to
// be had by changing your code to use command buffers directly by using the
// GLES2CmdHelper but that entails changing your code to use and deal with
// shared memory and synchronization issues.
class GLES2_IMPL_EXPORT GLES2Implementation : public GLES2Interface,
                                              public ImplementationBase,
                                              public QueryTrackerClient {
 public:
  // Stores GL state that never changes.
  struct GLES2_IMPL_EXPORT GLStaticState {
    GLStaticState();
    ~GLStaticState();

    typedef std::pair<GLenum, GLenum> ShaderPrecisionKey;
    typedef std::map<ShaderPrecisionKey,
                     cmds::GetShaderPrecisionFormat::Result>
        ShaderPrecisionMap;
    ShaderPrecisionMap shader_precisions;
  };

  // GL names for the buffers used to emulate client side buffers.
  static const GLuint kClientSideArrayId = 0xFEDCBA98u;
  static const GLuint kClientSideElementArrayId = 0xFEDCBA99u;

  // Number of swap buffers allowed before waiting.
  static const size_t kMaxSwapBuffers = 2;

  GLES2Implementation(GLES2CmdHelper* helper,
                      scoped_refptr<ShareGroup> share_group,
                      TransferBufferInterface* transfer_buffer,
                      bool bind_generates_resource,
                      bool lose_context_when_out_of_memory,
                      bool support_client_side_arrays,
                      GpuControl* gpu_control);

  ~GLES2Implementation() override;

  gpu::ContextResult Initialize(const SharedMemoryLimits& limits);

  // The GLES2CmdHelper being used by this GLES2Implementation. You can use
  // this to issue cmds at a lower level for certain kinds of optimization.
  GLES2CmdHelper* helper() const;

  // GLES2Interface implementation
  void FreeSharedMemory(void*) override;
  GLboolean DidGpuSwitch(gl::GpuPreference* active_gpu) final;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gpu/command_buffer/client/gles2_implementation_autogen.h"

  // ContextSupport implementation.
  void SetAggressivelyFreeResources(bool aggressively_free_resources) override;
  void Swap(uint32_t flags,
            SwapCompletedCallback complete_callback,
            PresentationCallback presentation_callback) override;
  void SwapWithBounds(const std::vector<gfx::Rect>& rects,
                      uint32_t flags,
                      SwapCompletedCallback swap_completed,
                      PresentationCallback presentation_callback) override;
  void PartialSwapBuffers(const gfx::Rect& sub_buffer,
                          uint32_t flags,
                          SwapCompletedCallback swap_completed,
                          PresentationCallback presentation_callback) override;
  void CommitOverlayPlanes(uint32_t flags,
                           SwapCompletedCallback swap_completed,
                           PresentationCallback presentation_callback) override;
  void ScheduleOverlayPlane(int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            unsigned overlay_texture_id,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& uv_rect,
                            bool enable_blend,
                            unsigned gpu_fence_id) override;
  uint64_t ShareGroupTracingGUID() const override;
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)> callback) override;
  bool ThreadSafeShallowLockDiscardableTexture(uint32_t texture_id) override;
  void CompleteLockDiscardableTexureOnContextThread(
      uint32_t texture_id) override;
  bool ThreadsafeDiscardableTextureIsDeletedForTracing(
      uint32_t texture_id) override;
  void* MapTransferCacheEntry(uint32_t serialized_size) override;
  void UnmapAndCreateTransferCacheEntry(uint32_t type, uint32_t id) override;
  bool ThreadsafeLockTransferCacheEntry(uint32_t type, uint32_t id) override;
  void UnlockTransferCacheEntries(
      const std::vector<std::pair<uint32_t, uint32_t>>& entries) override;
  void DeleteTransferCacheEntry(uint32_t type, uint32_t id) override;
  unsigned int GetTransferBufferFreeSize() const override;
  bool IsJpegDecodeAccelerationSupported() const override;
  bool IsWebPDecodeAccelerationSupported() const override;
  bool CanDecodeWithHardwareAcceleration(
      const cc::ImageHeaderMetadata* image_metadata) const override;

  // InterfaceBase implementation.
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;

  void GetProgramInfoCHROMIUMHelper(GLuint program,
                                    std::vector<int8_t>* result);
  GLint GetAttribLocationHelper(GLuint program, const char* name);
  GLint GetUniformLocationHelper(GLuint program, const char* name);
  GLint GetFragDataIndexEXTHelper(GLuint program, const char* name);
  GLint GetFragDataLocationHelper(GLuint program, const char* name);

  // Writes the result bucket into a buffer pointed by name and of maximum size
  // buffsize. If length is !null, it receives the number of characters written
  // (excluding the final \0). This is a helper function for GetActive*Helper
  // functions that return names.
  void GetResultNameHelper(GLsizei bufsize, GLsizei* length, char* name);
  bool GetActiveAttribHelper(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name);
  bool GetActiveUniformHelper(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name);
  void GetUniformBlocksCHROMIUMHelper(GLuint program,
                                      std::vector<int8_t>* result);
  void GetUniformsES3CHROMIUMHelper(GLuint program,
                                    std::vector<int8_t>* result);
  GLuint GetUniformBlockIndexHelper(GLuint program, const char* name);
  bool GetActiveUniformBlockNameHelper(
      GLuint program, GLuint index, GLsizei bufsize,
      GLsizei* length, char* name);
  bool GetActiveUniformBlockivHelper(
      GLuint program, GLuint index, GLenum pname, GLint* params);
  void GetTransformFeedbackVaryingsCHROMIUMHelper(GLuint program,
                                                  std::vector<int8_t>* result);
  bool GetTransformFeedbackVaryingHelper(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name);
  bool GetUniformIndicesHelper(
      GLuint program, GLsizei count, const char* const* names, GLuint* indices);
  bool GetActiveUniformsivHelper(
      GLuint program, GLsizei count, const GLuint* indices,
      GLenum pname, GLint* params);
  bool GetSyncivHelper(
      GLsync sync, GLenum pname, GLsizei bufsize, GLsizei* length,
      GLint* values);
  bool GetQueryObjectValueHelper(
      const char* function_name, GLuint id, GLenum pname, GLuint64* params);
  bool GetProgramInterfaceivHelper(
      GLuint program, GLenum program_interface, GLenum pname, GLint* params);
  GLuint GetProgramResourceIndexHelper(
      GLuint program, GLenum program_interface, const char* name);
  bool GetProgramResourceNameHelper(
      GLuint program, GLenum program_interface, GLuint index, GLsizei bufsize,
      GLsizei* length, char* name);
  bool GetProgramResourceivHelper(
      GLuint program, GLenum program_interface, GLuint index,
      GLsizei prop_count, const GLenum* props, GLsizei bufsize, GLsizei* length,
      GLint* params);
  GLint GetProgramResourceLocationHelper(
      GLuint program, GLenum program_interface, const char* name);

  const scoped_refptr<ShareGroup>& share_group() const { return share_group_; }

  GpuControl* gpu_control() {
    return gpu_control_;
  }

  ShareGroupContextData* share_group_context_data() {
    return &share_group_context_data_;
  }

  // QueryTrackerClient implementation.
  void IssueBeginQuery(GLenum target,
                       GLuint id,
                       uint32_t sync_data_shm_id,
                       uint32_t sync_data_shm_offset) override;
  void IssueEndQuery(GLenum target, GLuint submit_count) override;
  void IssueQueryCounter(GLuint id,
                         GLenum target,
                         uint32_t sync_data_shm_id,
                         uint32_t sync_data_shm_offset,
                         GLuint submit_count) override;
  void IssueSetDisjointValueSync(uint32_t sync_data_shm_id,
                                 uint32_t sync_data_shm_offset) override;
  GLenum GetClientSideGLError() override;
  CommandBufferHelper* cmd_buffer_helper() override;
  void SetGLError(GLenum error,
                  const char* function_name,
                  const char* msg) override;

  CommandBuffer* command_buffer() const;

 private:
  friend class GLES2ImplementationTest;
  friend class VertexArrayObjectManager;
  friend class QueryTracker;

  using IdNamespaces = id_namespaces::IdNamespaces;

  // Used to track whether an extension is available
  enum ExtensionStatus {
    kAvailableExtensionStatus,
    kUnavailableExtensionStatus,
    kUnknownExtensionStatus
  };

  enum Dimension {
    k2D,
    k3D,
  };

  // Base class for mapped resources.
  struct MappedResource {
    MappedResource(GLenum _access, int _shm_id, void* mem, unsigned int offset)
        : access(_access),
          shm_id(_shm_id),
          shm_memory(mem),
          shm_offset(offset) {}

    // access mode. Currently only GL_WRITE_ONLY is valid
    GLenum access;

    // Shared memory ID for buffer.
    int shm_id;

    // Address of shared memory
    void* shm_memory;

    // Offset of shared memory
    unsigned int shm_offset;
  };

  // Used to track mapped textures.
  struct MappedTexture : public MappedResource {
    MappedTexture(
        GLenum access,
        int shm_id,
        void* shm_mem,
        unsigned int shm_offset,
        GLenum _target,
        GLint _level,
        GLint _xoffset,
        GLint _yoffset,
        GLsizei _width,
        GLsizei _height,
        GLenum _format,
        GLenum _type)
        : MappedResource(access, shm_id, shm_mem, shm_offset),
          target(_target),
          level(_level),
          xoffset(_xoffset),
          yoffset(_yoffset),
          width(_width),
          height(_height),
          format(_format),
          type(_type) {
    }

    // These match the arguments to TexSubImage2D.
    GLenum target;
    GLint level;
    GLint xoffset;
    GLint yoffset;
    GLsizei width;
    GLsizei height;
    GLenum format;
    GLenum type;
  };

  // Used to track mapped buffers.
  struct MappedBuffer : public MappedResource {
    MappedBuffer(
        GLenum access,
        int shm_id,
        void* shm_mem,
        unsigned int shm_offset,
        GLenum _target,
        GLintptr _offset,
        GLsizeiptr _size)
        : MappedResource(access, shm_id, shm_mem, shm_offset),
          target(_target),
          offset(_offset),
          size(_size) {
    }

    // These match the arguments to BufferSubData.
    GLenum target;
    GLintptr offset;
    GLsizeiptr size;
  };

  struct TextureUnit {
    TextureUnit() {}

    // texture currently bound to this unit's GL_TEXTURE_2D with glBindTexture
    GLuint bound_texture_2d = 0;

    // texture currently bound to this unit's GL_TEXTURE_CUBE_MAP with
    // glBindTexture
    GLuint bound_texture_cube_map = 0;

    // texture currently bound to this unit's GL_TEXTURE_EXTERNAL_OES with
    // glBindTexture
    GLuint bound_texture_external_oes = 0;

    // texture currently bound to this unit's GL_TEXTURE_RECTANGLE_ARB with
    // glBindTexture
    GLuint bound_texture_rectangle_arb = 0;
  };

  // Prevents problematic reentrancy during error callbacks.
  class DeferErrorCallbacks {
   public:
    explicit DeferErrorCallbacks(GLES2Implementation* gles2_implementation);
    ~DeferErrorCallbacks();

   private:
    GLES2Implementation* gles2_implementation_;
  };

  struct DeferredErrorCallback {
    // This takes std::string by value and uses std::move in the
    // implementation, allowing the compiler to achieve zero copies
    // when passing in a temporary.
    DeferredErrorCallback(std::string message, int32_t id);

    std::string message;
    int32_t id = 0;
  };

  // Checks for single threaded access.
  class SingleThreadChecker {
   public:
    explicit SingleThreadChecker(GLES2Implementation* gles2_implementation);
    ~SingleThreadChecker();

   private:
    GLES2Implementation* gles2_implementation_;
  };

  // ImplementationBase implementation.
  void IssueShallowFlush() override;

  // GpuControlClient implementation.
  void OnGpuControlLostContext() final;
  void OnGpuControlLostContextMaybeReentrant() final;
  void OnGpuControlErrorMessage(const char* message, int32_t id) final;
  void OnGpuControlSwapBuffersCompleted(
      const SwapBuffersCompleteParams& params) final;
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) final;
  void OnSwapBufferPresented(uint64_t swap_id,
                             const gfx::PresentationFeedback& feedback) final;
  void OnGpuControlReturnData(base::span<const uint8_t> data) final;

  void SendErrorMessage(std::string message, int32_t id);
  void CallDeferredErrorCallbacks();

  bool IsChromiumFramebufferMultisampleAvailable();

  bool IsExtensionAvailableHelper(
      const char* extension, ExtensionStatus* status);

  // Gets the GLError through our wrapper.
  GLenum GetGLError();

  // Sets our wrapper for the GLError.
  void SetGLErrorInvalidEnum(
      const char* function_name, GLenum value, const char* label);

  // Returns the last error and clears it. Useful for debugging.
  const std::string& GetLastError() {
    return last_error_;
  }

  void AllocateShadowCopiesForReadback();
  static void BufferShadowWrittenCallback(
      const ReadbackBufferShadowTracker::BufferList& buffers,
      uint64_t serial);

  // Returns true if id is reserved.
  bool IsBufferReservedId(GLuint id);
  bool IsFramebufferReservedId(GLuint id) { return false; }
  bool IsRenderbufferReservedId(GLuint id) { return false; }
  bool IsTextureReservedId(GLuint id) { return false; }
  bool IsVertexArrayReservedId(GLuint id) { return false; }
  bool IsProgramReservedId(GLuint id) { return false; }
  bool IsSamplerReservedId(GLuint id) { return false; }
  bool IsTransformFeedbackReservedId(GLuint id) { return false; }

  bool UpdateIndexedBufferState(GLenum target,
                                GLuint index,
                                GLuint buffer_id,
                                const char* function_name);
  void BindBufferHelper(GLenum target, GLuint buffer);
  void BindBufferBaseHelper(GLenum target, GLuint index, GLuint buffer);
  void BindBufferRangeHelper(GLenum target, GLuint index, GLuint buffer,
                             GLintptr offset, GLsizeiptr size);
  void BindFramebufferHelper(GLenum target, GLuint framebuffer);
  void BindRenderbufferHelper(GLenum target, GLuint renderbuffer);
  void BindSamplerHelper(GLuint unit, GLuint sampler);
  void BindTextureHelper(GLenum target, GLuint texture);
  void BindTransformFeedbackHelper(GLenum target, GLuint transformfeedback);
  void BindVertexArrayOESHelper(GLuint array);
  void UseProgramHelper(GLuint program);

  void BindBufferStub(GLenum target, GLuint buffer);
  void BindBufferBaseStub(GLenum target, GLuint index, GLuint buffer);
  void BindBufferRangeStub(GLenum target, GLuint index, GLuint buffer,
                           GLintptr offset, GLsizeiptr size);
  void BindRenderbufferStub(GLenum target, GLuint renderbuffer);
  void BindTextureStub(GLenum target, GLuint texture);

  void GenBuffersHelper(GLsizei n, const GLuint* buffers);
  void GenFramebuffersHelper(GLsizei n, const GLuint* framebuffers);
  void GenRenderbuffersHelper(GLsizei n, const GLuint* renderbuffers);
  void GenTexturesHelper(GLsizei n, const GLuint* textures);
  void GenVertexArraysOESHelper(GLsizei n, const GLuint* arrays);
  void GenQueriesEXTHelper(GLsizei n, const GLuint* queries);
  void GenSamplersHelper(GLsizei n, const GLuint* samplers);
  void GenTransformFeedbacksHelper(GLsizei n, const GLuint* transformfeedbacks);

  void DeleteBuffersHelper(GLsizei n, const GLuint* buffers);
  void DeleteFramebuffersHelper(GLsizei n, const GLuint* framebuffers);
  void DeleteRenderbuffersHelper(GLsizei n, const GLuint* renderbuffers);
  void DeleteTexturesHelper(GLsizei n, const GLuint* textures);
  void UnbindTexturesHelper(GLsizei n, const GLuint* textures);
  bool DeleteProgramHelper(GLuint program);
  bool DeleteShaderHelper(GLuint shader);
  void DeleteQueriesEXTHelper(GLsizei n, const GLuint* queries);
  void DeleteVertexArraysOESHelper(GLsizei n, const GLuint* arrays);
  void DeleteSamplersHelper(GLsizei n, const GLuint* samplers);
  void DeleteTransformFeedbacksHelper(
      GLsizei n, const GLuint* transformfeedbacks);
  void DeleteSyncHelper(GLsync sync);

  void DeleteBuffersStub(GLsizei n, const GLuint* buffers);
  void DeleteRenderbuffersStub(GLsizei n, const GLuint* renderbuffers);
  void DeleteTexturesStub(GLsizei n, const GLuint* textures);
  void DeletePathsCHROMIUMStub(GLuint first_client_id, GLsizei range);
  void DeleteProgramStub(GLsizei n, const GLuint* programs);
  void DeleteShaderStub(GLsizei n, const GLuint* shaders);
  void DeleteSamplersStub(GLsizei n, const GLuint* samplers);
  void DeleteSyncStub(GLsizei n, const GLuint* syncs);
  void DestroyGpuFenceCHROMIUMHelper(GLuint client_id);

  void BufferDataHelper(
      GLenum target, GLsizeiptr size, const void* data, GLenum usage);
  void BufferSubDataHelper(
      GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
  void BufferSubDataHelperImpl(
      GLenum target, GLintptr offset, GLsizeiptr size, const void* data,
      ScopedTransferBufferPtr* buffer);

  void MultiDrawArraysWEBGLHelper(GLenum mode,
                                  const GLint* firsts,
                                  const GLsizei* counts,
                                  GLsizei drawcount);

  void MultiDrawArraysInstancedWEBGLHelper(GLenum mode,
                                           const GLint* firsts,
                                           const GLsizei* counts,
                                           const GLsizei* instanceCounts,
                                           GLsizei drawcount);

  void MultiDrawArraysInstancedBaseInstanceWEBGLHelper(
      GLenum mode,
      const GLint* firsts,
      const GLsizei* counts,
      const GLsizei* instanceCounts,
      const GLuint* baseInstances,
      GLsizei drawcount);

  void MultiDrawElementsWEBGLHelper(GLenum mode,
                                    const GLsizei* counts,
                                    GLenum type,
                                    const GLsizei* offsets,
                                    GLsizei drawcount);

  void MultiDrawElementsInstancedWEBGLHelper(GLenum mode,
                                             const GLsizei* counts,
                                             GLenum type,
                                             const GLsizei* offsets,
                                             const GLsizei* instanceCounts,
                                             GLsizei drawcount);

  void MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGLHelper(
      GLenum mode,
      const GLsizei* counts,
      GLenum type,
      const GLsizei* offsets,
      const GLsizei* instanceCounts,
      const GLint* baseVertices,
      const GLuint* baseInstances,
      GLsizei drawcount);

  GLuint CreateImageCHROMIUMHelper(ClientBuffer buffer,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum internalformat);
  void DestroyImageCHROMIUMHelper(GLuint image_id);

  // Helper for GetVertexAttrib
  bool GetVertexAttribHelper(GLuint index, GLenum pname, uint32_t* param);

  GLuint GetMaxValueInBufferCHROMIUMHelper(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset);

  void WaitAllAsyncTexImage2DCHROMIUMHelper();

  void RestoreElementAndArrayBuffers(bool restore);
  void RestoreArrayBuffer(bool restrore);

  // The pixels pointer should already account for unpack skip
  // images/rows/pixels.
  void TexSubImage2DImpl(GLenum target,
                         GLint level,
                         GLint xoffset,
                         GLint yoffset,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         uint32_t unpadded_row_size,
                         const void* pixels,
                         uint32_t pixels_padded_row_size,
                         GLboolean internal,
                         ScopedTransferBufferPtr* buffer,
                         uint32_t buffer_padded_row_size);
  void TexSubImage3DImpl(GLenum target,
                         GLint level,
                         GLint xoffset,
                         GLint yoffset,
                         GLint zoffset,
                         GLsizei width,
                         GLsizei height,
                         GLsizei depth,
                         GLenum format,
                         GLenum type,
                         uint32_t unpadded_row_size,
                         const void* pixels,
                         uint32_t pixels_padded_row_size,
                         GLboolean internal,
                         ScopedTransferBufferPtr* buffer,
                         uint32_t buffer_padded_row_size);

  // Helpers for query functions.
  bool GetHelper(GLenum pname, GLint* params);
  GLuint GetBoundBufferHelper(GLenum target);
  bool GetBooleanvHelper(GLenum pname, GLboolean* params);
  bool GetBufferParameteri64vHelper(
      GLenum target, GLenum pname, GLint64* params);
  bool GetBufferParameterivHelper(GLenum target, GLenum pname, GLint* params);
  bool GetFloatvHelper(GLenum pname, GLfloat* params);
  bool GetFramebufferAttachmentParameterivHelper(
      GLenum target, GLenum attachment, GLenum pname, GLint* params);
  bool GetInteger64vHelper(GLenum pname, GLint64* params);
  bool GetIntegervHelper(GLenum pname, GLint* params);
  bool GetIntegeri_vHelper(GLenum pname, GLuint index, GLint* data);
  bool GetInteger64i_vHelper(GLenum pname, GLuint index, GLint64* data);
  bool GetInternalformativHelper(
      GLenum target, GLenum format, GLenum pname, GLsizei bufSize,
      GLint* params);
  bool GetProgramivHelper(GLuint program, GLenum pname, GLint* params);
  bool GetSamplerParameterfvHelper(
      GLuint sampler, GLenum pname, GLfloat* params);
  bool GetSamplerParameterivHelper(
      GLuint sampler, GLenum pname, GLint* params);
  bool GetRenderbufferParameterivHelper(
      GLenum target, GLenum pname, GLint* params);
  bool GetShaderivHelper(GLuint shader, GLenum pname, GLint* params);
  bool GetTexParameterfvHelper(GLenum target, GLenum pname, GLfloat* params);
  bool GetTexParameterivHelper(GLenum target, GLenum pname, GLint* params);
  const GLubyte* GetStringHelper(GLenum name);

  bool IsExtensionAvailable(const char* ext);

  // Caches certain capabilties state. Return true if cached.
  bool SetCapabilityState(GLenum cap, bool enabled);

  IdHandlerInterface* GetIdHandler(SharedIdNamespaces id_namespace) const;
  RangeIdHandlerInterface* GetRangeIdHandler(int id_namespace) const;
  // IdAllocators for objects that can't be shared among contexts.
  IdAllocator* GetIdAllocator(IdNamespaces id_namespace) const;

  void FinishHelper();
  void FlushHelper();

  void RunIfContextNotLost(base::OnceClosure callback);

  // Validate if an offset is valid, i.e., non-negative and fit into 32-bit.
  // If not, generate an approriate error, and return false.
  bool ValidateOffset(const char* func, GLintptr offset);

  // Validate if a size is valid, i.e., non-negative and fit into 32-bit.
  // If not, generate an approriate error, and return false.
  bool ValidateSize(const char* func, GLsizeiptr offset);

  // Remove the transfer buffer from the buffer tracker. For buffers used
  // asynchronously the memory is free:ed if the upload has completed. For
  // other buffers, the memory is either free:ed immediately or free:ed pending
  // a token.
  void RemoveTransferBuffer(BufferTracker::Buffer* buffer);

  bool GetBoundPixelTransferBuffer(
      GLenum target, const char* function_name, GLuint* buffer_id);
  BufferTracker::Buffer* GetBoundPixelTransferBufferIfValid(
      GLuint buffer_id,
      const char* function_name, GLuint offset, GLsizei size);

  void OnBufferWrite(GLenum target);

  // Pack 2D arrays of char into a bucket.
  // Helper function for ShaderSource(), TransformFeedbackVaryings(), etc.
  bool PackStringsToBucket(GLsizei count,
                           const char* const* str,
                           const GLint* length,
                           const char* func_name);

  const std::string& GetLogPrefix() const;

  bool PrepareInstancedPathCommand(const char* function_name,
                                   GLsizei num_paths,
                                   GLenum path_name_type,
                                   const void* paths,
                                   GLenum transform_type,
                                   const GLfloat* transform_values,
                                   ScopedTransferBufferPtr* buffer,
                                   uint32_t* out_paths_shm_id,
                                   uint32_t* out_paths_offset,
                                   uint32_t* out_transforms_shm_id,
                                   uint32_t* out_transforms_offset);

// Set to 1 to have the client fail when a GL error is generated.
// This helps find bugs in the renderer since the debugger stops on the error.
#if DCHECK_IS_ON()
#if 0
#define GL_CLIENT_FAIL_GL_ERRORS
#endif
#endif

#if defined(GL_CLIENT_FAIL_GL_ERRORS)
  void CheckGLError();
  void FailGLError(GLenum error);
#else
  void CheckGLError() { }
  void FailGLError(GLenum /* error */) { }
#endif

  void RemoveMappedBufferRangeByTarget(GLenum target);
  void RemoveMappedBufferRangeById(GLuint buffer);
  void ClearMappedBufferRangeMap();

  void DrawElementsImpl(GLenum mode, GLsizei count, GLenum type,
                        const void* indices, const char* func_name);
  void UpdateCachedExtensionsIfNeeded();
  void InvalidateCachedExtensions();

  PixelStoreParams GetUnpackParameters(Dimension dimension);

  uint64_t PrepareNextSwapId(SwapCompletedCallback complete_callback,
                             PresentationCallback present_callback);

  GLES2Util util_;
  GLES2CmdHelper* helper_;
  std::string last_error_;
  DebugMarkerManager debug_marker_manager_;
  std::string this_in_hex_;

  base::queue<int32_t> swap_buffers_tokens_;

  ExtensionStatus chromium_framebuffer_multisample_;

  GLStaticState static_state_;
  ClientContextState state_;

  // pack alignment as last set by glPixelStorei
  GLint pack_alignment_;

  // pack row length as last set by glPixelStorei
  GLint pack_row_length_;

  // pack skip pixels as last set by glPixelStorei
  GLint pack_skip_pixels_;

  // pack skip rows as last set by glPixelStorei
  GLint pack_skip_rows_;

  // unpack alignment as last set by glPixelStorei
  GLint unpack_alignment_;

  // unpack row length as last set by glPixelStorei
  GLint unpack_row_length_;

  // unpack image height as last set by glPixelStorei
  GLint unpack_image_height_;

  // unpack skip rows as last set by glPixelStorei
  GLint unpack_skip_rows_;

  // unpack skip pixels as last set by glPixelStorei
  GLint unpack_skip_pixels_;

  // unpack skip images as last set by glPixelStorei
  GLint unpack_skip_images_;

  std::unique_ptr<TextureUnit[]> texture_units_;

  // 0 to gl_state_.max_combined_texture_image_units.
  GLuint active_texture_unit_;

  GLuint bound_framebuffer_;
  GLuint bound_read_framebuffer_;
  GLuint bound_renderbuffer_;

  // The program in use by glUseProgram
  GLuint current_program_;

  GLuint bound_array_buffer_;
  GLuint bound_atomic_counter_buffer_;
  GLuint bound_copy_read_buffer_;
  GLuint bound_copy_write_buffer_;
  GLuint bound_dispatch_indirect_buffer_;
  GLuint bound_draw_indirect_buffer_;
  GLuint bound_pixel_pack_buffer_;
  GLuint bound_pixel_unpack_buffer_;
  GLuint bound_shader_storage_buffer_;
  GLuint bound_transform_feedback_buffer_;
  GLuint bound_uniform_buffer_;
  // We don't cache the currently bound transform feedback buffer, because
  // it is part of the current transform feedback object. Caching the transform
  // feedback object state correctly requires predicting if a call to
  // glBeginTransformFeedback will succeed or fail, which in turn requires
  // caching a whole bunch of other states such as the transform feedback
  // varyings of the current program.

  // The currently bound pixel transfer buffers.
  GLuint bound_pixel_pack_transfer_buffer_id_;
  GLuint bound_pixel_unpack_transfer_buffer_id_;

  // Client side management for vertex array objects. Needed to correctly
  // track client side arrays.
  std::unique_ptr<VertexArrayObjectManager> vertex_array_object_manager_;

  GLuint reserved_ids_[2];

  // Current GL error bits.
  uint32_t error_bits_;

  LogSettings log_settings_;

  // When true, the context is lost when a GL_OUT_OF_MEMORY error occurs.
  const bool lose_context_when_out_of_memory_;

  // Whether or not to support client side arrays.
  const bool support_client_side_arrays_;

  // Used to check for single threaded access.
  int use_count_;

  // Changed every time a flush or finish occurs.
  uint32_t flush_id_;

  // Avoid recycling of client-allocated GpuFence IDs by saving the
  // last-allocated one and requesting the next one to be higher than that.
  // This will wrap around as needed, but the space should be plenty big enough
  // to avoid collisions.
  uint32_t last_gpu_fence_id_ = 0;

  // Maximum amount of extra memory from the mapped memory pool to use when
  // needing to transfer something exceeding the default transfer buffer.
  uint32_t max_extra_transfer_buffer_size_;

  // Set of strings returned from glGetString.  We need to cache these because
  // the pointer passed back to the client has to remain valid for eternity.
  std::set<std::string> gl_strings_;

  typedef std::map<const void*, MappedBuffer> MappedBufferMap;
  MappedBufferMap mapped_buffers_;

  // TODO(zmo): Consolidate |mapped_buffers_| and |mapped_buffer_range_map_|.
  typedef std::unordered_map<GLuint, MappedBuffer> MappedBufferRangeMap;
  MappedBufferRangeMap mapped_buffer_range_map_;

  typedef std::map<const void*, MappedTexture> MappedTextureMap;
  MappedTextureMap mapped_textures_;

  scoped_refptr<ShareGroup> share_group_;
  ShareGroupContextData share_group_context_data_;

  std::unique_ptr<IdAllocator>
      id_allocators_[static_cast<int>(IdNamespaces::kNumIdNamespaces)];

  std::unique_ptr<BufferTracker> buffer_tracker_;
  std::unique_ptr<ReadbackBufferShadowTracker> readback_buffer_shadow_tracker_;

  base::Optional<ScopedMappedMemoryPtr> font_mapped_buffer_;
  base::Optional<ScopedTransferBufferPtr> raster_mapped_buffer_;

  base::RepeatingCallback<void(const char*, int32_t)> error_message_callback_;
  bool deferring_error_callbacks_ = false;
  std::deque<DeferredErrorCallback> deferred_error_callbacks_;

  int current_trace_stack_;

  // Flag to indicate whether the implementation can retain resources, or
  // whether it should aggressively free them.
  bool aggressively_free_resources_;

  // Result of last GetString(GL_EXTENSIONS), used to keep
  // GetString(GL_EXTENSIONS), GetStringi(GL_EXTENSIONS, index) and
  // GetIntegerv(GL_NUM_EXTENSIONS) in sync. This points to gl_strings, valid
  // forever.
  const char* cached_extension_string_;

  // Populated if cached_extension_string_ != nullptr. These point to
  // gl_strings, valid forever.
  std::vector<const char*> cached_extensions_;

  // The next swap ID to send.
  uint64_t swap_id_ = 0;
  // A map of swap IDs to callbacks to run when that ID completes.
  base::flat_map<uint64_t, SwapCompletedCallback> pending_swap_callbacks_;
  base::flat_map<uint64_t, PresentationCallback>
      pending_presentation_callbacks_;

  std::string last_active_url_;

  bool gpu_switched_ = false;
  gl::GpuPreference active_gpu_heuristic_ = gl::GpuPreference::kDefault;

  base::WeakPtrFactory<GLES2Implementation> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GLES2Implementation);
};

inline bool GLES2Implementation::GetBufferParameteri64vHelper(
    GLenum /* target */, GLenum /* pname */, GLint64* /* params */) {
  return false;
}

inline bool GLES2Implementation::GetBufferParameterivHelper(
    GLenum /* target */, GLenum /* pname */, GLint* /* params */) {
  return false;
}

inline bool GLES2Implementation::GetFramebufferAttachmentParameterivHelper(
    GLenum /* target */,
    GLenum /* attachment */,
    GLenum /* pname */,
    GLint* /* params */) {
  return false;
}

inline bool GLES2Implementation::GetRenderbufferParameterivHelper(
    GLenum /* target */, GLenum /* pname */, GLint* /* params */) {
  return false;
}

inline bool GLES2Implementation::GetShaderivHelper(
    GLuint /* shader */, GLenum /* pname */, GLint* /* params */) {
  return false;
}

inline bool GLES2Implementation::GetTexParameterfvHelper(
    GLenum /* target */, GLenum /* pname */, GLfloat* /* params */) {
  return false;
}

inline bool GLES2Implementation::GetTexParameterivHelper(
    GLenum /* target */, GLenum /* pname */, GLint* /* params */) {
  return false;
}

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_

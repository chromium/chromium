// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_H_

#include "base/macros.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class DecoderClient;
struct GpuFeatureInfo;
struct GpuPreferences;
class MemoryTracker;
class ServiceTransferCache;
class SharedContextState;
class SharedImageManager;

namespace gles2 {
class CopyTextureCHROMIUMResourceManager;
class GLES2Util;
class ImageManager;
class Logger;
class Outputter;
}  // namespace gles2

namespace raster {

// This class implements the AsyncAPIInterface interface, decoding
// RasterInterface commands and calling GL.
class GPU_GLES2_EXPORT RasterDecoder : public DecoderContext,
                                       public CommonDecoder {
 public:
  static RasterDecoder* Create(
      DecoderClient* client,
      CommandBufferServiceBase* command_buffer_service,
      gles2::Outputter* outputter,
      const GpuFeatureInfo& gpu_feature_info,
      const GpuPreferences& gpu_preferences,
      MemoryTracker* memory_tracker,
      SharedImageManager* shared_image_manager,
      scoped_refptr<SharedContextState> shared_context_state);

  ~RasterDecoder() override;

  // DecoderContext implementation.
  bool initialized() const override;
  TextureBase* GetTextureBase(uint32_t client_id) override;
  void SetLevelInfo(uint32_t client_id,
                    int level,
                    unsigned internal_format,
                    unsigned width,
                    unsigned height,
                    unsigned depth,
                    unsigned format,
                    unsigned type,
                    const gfx::Rect& cleared_rect) override;
  void BeginDecoding() override;
  void EndDecoding() override;
  base::StringPiece GetLogPrefix() override;

  virtual gles2::GLES2Util* GetGLES2Util() = 0;
  virtual gles2::Logger* GetLogger() = 0;
  virtual void SetIgnoreCachedStateForTest(bool ignore) = 0;

  // Gets the ImageManager for this context.
  virtual gles2::ImageManager* GetImageManagerForTest() = 0;

  void set_initialized() { initialized_ = true; }

  // Set to true to call glGetError after every command.
  void set_debug(bool debug) { debug_ = debug; }
  bool debug() const { return debug_; }

  // Set to true to LOG every command.
  void SetLogCommands(bool log_commands) override;
  gles2::Outputter* outputter() const override;
  bool log_commands() const { return log_commands_; }

  virtual void SetCopyTextureResourceManagerForTest(
      gles2::CopyTextureCHROMIUMResourceManager*
          copy_texture_resource_manager) = 0;

  virtual int DecoderIdForTest() = 0;
  virtual ServiceTransferCache* GetTransferCacheForTest() = 0;

  virtual void SetUpForRasterCHROMIUMForTest() = 0;
  virtual void SetOOMErrorForTest() = 0;
  virtual void DisableFlushWorkaroundForTest() = 0;

 protected:
  RasterDecoder(DecoderClient* client,
                CommandBufferServiceBase* command_buffer_service,
                gles2::Outputter* outputter);

 private:
  bool initialized_ = false;
  bool debug_ = false;
  bool log_commands_ = false;
  gles2::Outputter* outputter_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RasterDecoder);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_QUERY_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_QUERY_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/gpu_gles2_export.h"

namespace gl {
class GPUTimer;
class GPUTimingClient;
}  // namespace gl

namespace gpu {
namespace gles2 {

class FeatureInfo;
class GLES2Decoder;

// Similar to QueryManager with extensions to support GLES2 specific queries.
class GPU_GLES2_EXPORT GLES2QueryManager : public QueryManager {
 public:
  class GPU_GLES2_EXPORT GLES2Query : public QueryManager::Query {
   public:
    GLES2Query(GLES2QueryManager* manager,
               GLenum target,
               scoped_refptr<gpu::Buffer> buffer,
               QuerySync* sync);

    void SafelyResetDisjointValue() {
      gles2_query_manager_->SafelyResetDisjointValue();
    }

    void UpdateDisjointValue() { gles2_query_manager_->UpdateDisjointValue(); }

    void BeginContinualDisjointUpdate() {
      gles2_query_manager_->update_disjoints_continually_ = true;
    }

   protected:
    ~GLES2Query() override;

    GLES2QueryManager* gles2_query_manager() { return gles2_query_manager_; }

   private:
    // |this| is owned by |gles2_query_manager_|.
    raw_ptr<GLES2QueryManager> gles2_query_manager_;
  };

  GLES2QueryManager(GLES2Decoder* decoder, FeatureInfo* feature_info);

  GLES2QueryManager(const GLES2QueryManager&) = delete;
  GLES2QueryManager& operator=(const GLES2QueryManager&) = delete;

  ~GLES2QueryManager() override;

  // Creates a Query for the given query.
  Query* CreateQuery(GLenum target,
                     GLuint client_id,
                     scoped_refptr<gpu::Buffer> buffer,
                     QuerySync* sync) override;

  // Do any updates we need to do when the frame has begun.
  void ProcessFrameBeginUpdates();

  // Sets up a location to be incremented whenever a disjoint is detected.
  error::Error SetDisjointSync(int32_t shm_id, uint32_t shm_offset);

  std::unique_ptr<gl::GPUTimer> CreateGPUTimer(bool elapsed_time);
  bool GPUTimingAvailable();

  GLES2Decoder* decoder() const { return decoder_; }

 private:
  // Checks and notifies if a disjoint occurred.
  void UpdateDisjointValue();

  // Safely resets the disjoint value if no queries are active.
  void SafelyResetDisjointValue();

  raw_ptr<GLES2Decoder> decoder_;

  // Whether we are tracking disjoint values every frame.
  bool update_disjoints_continually_;

  // The shared memory used for disjoint notifications.
  int32_t disjoint_notify_shm_id_;
  uint32_t disjoint_notify_shm_offset_;

  // Current number of disjoints notified.
  uint32_t disjoints_notified_;

  scoped_refptr<gl::GPUTimingClient> gpu_timing_client_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_QUERY_MANAGER_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BATCH_ACCESS_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BATCH_ACCESS_MANAGER_H_

#include <set>

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

class SharedImageBackingEglImage;
class SharedImageManager;

class GPU_GLES2_EXPORT SharedImageBatchAccessManager {
 public:
  SharedImageBatchAccessManager();

  SharedImageBatchAccessManager(const SharedImageBatchAccessManager&) = delete;
  SharedImageBatchAccessManager& operator=(
      const SharedImageBatchAccessManager&) = delete;

  ~SharedImageBatchAccessManager();

  bool IsDoingBatchReads();

 private:
  friend class SharedImageManager;
  friend class SharedImageBackingEglImage;

  using SetOfBackings = std::set<SharedImageBackingEglImage*>;

  void RegisterEglBackingForEndReadFence(
      SharedImageBackingEglImage* egl_backing);
  void UnregisterEglBacking(SharedImageBackingEglImage* egl_backing);
  bool BeginBatchReadAccess();
  bool EndBatchReadAccess();

  base::Lock lock_;
  base::flat_map<gl::GLApi*, SetOfBackings> backings_ GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BATCH_ACCESS_MANAGER_H_

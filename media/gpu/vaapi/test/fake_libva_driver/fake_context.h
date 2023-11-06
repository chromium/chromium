// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_CONTEXT_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_CONTEXT_H_

#include <va/va.h>

#include <vector>

#include "base/memory/raw_ptr.h"

namespace media::internal {

class ContextDelegate;
class FakeSurface;
class FakeBuffer;

// Class used for tracking a VAContext and all information relevant to it.
// All objects of this class are immutable, but three of the methods must be
// synchronized externally: BeginPicture(), RenderPicture(), and EndPicture().
// The other methods are thread-safe and may be called concurrently with any of
// those three methods.
class FakeContext {
 public:
  using IdType = VAContextID;

  FakeContext(IdType id,
              VAConfigID config_id,
              int picture_width,
              int picture_height,
              int flag,
              std::vector<VASurfaceID> render_targets);
  FakeContext(const FakeContext&) = delete;
  FakeContext& operator=(const FakeContext&) = delete;
  ~FakeContext();

  IdType GetID() const;
  VAConfigID GetConfigID() const;
  int GetPictureWidth() const;
  int GetPictureHeight() const;
  int GetFlag() const;
  const std::vector<VASurfaceID>& GetRenderTargets() const;

  void BeginPicture(const FakeSurface& surface) const;
  void RenderPicture(
      const std::vector<raw_ptr<const FakeBuffer>>& buffers) const;
  void EndPicture() const;

 private:
  const IdType id_;
  const VAConfigID config_id_;
  const int picture_width_;
  const int picture_height_;
  const int flag_;
  const std::vector<VASurfaceID> render_targets_;
  const std::unique_ptr<ContextDelegate> delegate_;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_CONTEXT_H_

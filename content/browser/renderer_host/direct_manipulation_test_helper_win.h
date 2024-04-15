// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_TEST_HELPER_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_TEST_HELPER_WIN_H_

#include <windows.h>

#include <directmanipulation.h>
#include <wrl.h>

#include <array>

namespace content {
class PrecisionTouchpadBrowserTest;

// Size of the |transforms_| array. The DirectManipulationContent API specifies
// that the size is always 6 for direct manipulation transforms.
static constexpr int kTransformMatrixSize = 6;

// This class is used for setting up mock content to be used for testing direct
// manipulation and precision touchpad code paths. Most of its methods aren't
// used, and its only real purpose is to store, update, and provide the
// transforms scale, scroll_x, and scroll_y.
class MockDirectManipulationContent
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<
                  Microsoft::WRL::RuntimeClassType::ClassicCom>,
              Microsoft::WRL::FtmBase,
              IDirectManipulationContent>> {
 public:
  MockDirectManipulationContent();

  MockDirectManipulationContent(const MockDirectManipulationContent&) = delete;
  MockDirectManipulationContent& operator=(
      const MockDirectManipulationContent&) = delete;

  // IDirectManipulationContent:
  ~MockDirectManipulationContent() override;

  void SetContentTransform(float scale, float scroll_x, float scroll_y);

  // IDirectManipulationContent:
  HRESULT STDMETHODCALLTYPE GetContentTransform(float* transforms,
                                                DWORD point_count) override;

  // Other Overrides, also from IDirectManipulationContent
  HRESULT STDMETHODCALLTYPE GetContentRect(RECT* contentSize) override;

  HRESULT STDMETHODCALLTYPE SetContentRect(const RECT* contentSize) override;

  HRESULT STDMETHODCALLTYPE GetViewport(REFIID riid, void** object) override;

  HRESULT STDMETHODCALLTYPE GetTag(REFIID riid,
                                   void** object,
                                   UINT32* id) override;

  HRESULT STDMETHODCALLTYPE SetTag(IUnknown* object, UINT32 id) override;

  HRESULT STDMETHODCALLTYPE GetOutputTransform(float* matrix,
                                               DWORD point_count) override;

  HRESULT STDMETHODCALLTYPE SyncContentTransform(const float* matrix,
                                                 DWORD point_count) override;

 private:
  friend class PrecisionTouchpadBrowserTest;

  // IDirectManipulationContent provides a 3x2 transform matrix, written out
  // flatly as M = [(1,1), (1,2), (2,1), (2,2), (3,1), (3,2)].
  // Each element stores the following information:
  // (1,1) - x scale
  // (1,2) - y rotation (0 when rotation is not allowed, such as in Chromium)
  // (2,1) - x rotation (0 when rotation is not allowed)
  // (2,2) - y scale
  // (3,1) - x offset
  // (3,2) - y offset.
  std::array<float, kTransformMatrixSize> transforms_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_TEST_HELPER_WIN_H_

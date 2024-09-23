// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"

#include <windows.h>

#include <d3d11_1.h>

#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_angle_util_win.h"

namespace gpu {
namespace {

class DXGISharedHandleManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    d3d11_device_ = gl::QueryD3D11DeviceObjectFromANGLE();
    dxgi_shared_handle_manager_ =
        base::MakeRefCounted<DXGISharedHandleManager>();
  }

  bool ShouldSkipTest() const { return !d3d11_device_; }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateTexture() {
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags =
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
    HRESULT hr = d3d11_device_->CreateTexture2D(&desc, nullptr, &d3d11_texture);
    EXPECT_EQ(hr, S_OK);
    return d3d11_texture;
  }

  base::win::ScopedHandle CreateSharedHandle(
      const Microsoft::WRL::ComPtr<ID3D11Texture2D>& d3d11_texture) {
    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    HRESULT hr = d3d11_texture.As(&dxgi_resource);
    EXPECT_EQ(hr, S_OK);

    HANDLE shared_handle;
    hr = dxgi_resource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr, &shared_handle);
    EXPECT_EQ(hr, S_OK);

    return base::win::ScopedHandle(shared_handle);
  }

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager_;
};

TEST_F(DXGISharedHandleManagerTest, LookupByToken) {
  if (ShouldSkipTest())
    return;

  auto d3d11_texture = CreateTexture();
  ASSERT_TRUE(d3d11_texture);

  constexpr int kNumHandles = 5;

  auto orig_token = gfx::DXGIHandleToken();
  base::win::ScopedHandle orig_handle = CreateSharedHandle(d3d11_texture);
  ASSERT_TRUE(orig_handle.IsValid());

  scoped_refptr<DXGISharedHandleState> orig_state =
      dxgi_shared_handle_manager_->GetOrCreateSharedHandleState(
          orig_token, std::move(orig_handle), d3d11_device_);
  ASSERT_NE(orig_state, nullptr);

  EXPECT_EQ(dxgi_shared_handle_manager_->GetSharedHandleMapSizeForTesting(),
            1u);

  for (int i = 0; i < kNumHandles - 1; i++) {
    HANDLE handle;
    ::DuplicateHandle(::GetCurrentProcess(), orig_state->GetSharedHandle(),
                      ::GetCurrentProcess(), &handle,
                      /*dwDesiredAccess=*/0,
                      /*bInerhitHandle=*/FALSE, DUPLICATE_SAME_ACCESS);
    base::win::ScopedHandle new_handle(handle);
    ASSERT_TRUE(new_handle.IsValid());

    scoped_refptr<DXGISharedHandleState> state =
        dxgi_shared_handle_manager_->GetOrCreateSharedHandleState(
            orig_token, std::move(new_handle), d3d11_device_);
    EXPECT_EQ(state, orig_state);

    EXPECT_EQ(dxgi_shared_handle_manager_->GetSharedHandleMapSizeForTesting(),
              1u);
  }

  orig_state = nullptr;

  EXPECT_EQ(dxgi_shared_handle_manager_->GetSharedHandleMapSizeForTesting(),
            0u);
}

TEST_F(DXGISharedHandleManagerTest, LookupByTokenMultiThread) {
  if (ShouldSkipTest())
    return;

  auto d3d11_texture = CreateTexture();
  ASSERT_TRUE(d3d11_texture);

  constexpr int kNumHandles = 101;

  auto orig_token = gfx::DXGIHandleToken();
  base::win::ScopedHandle orig_handle = CreateSharedHandle(d3d11_texture);
  ASSERT_TRUE(orig_handle.IsValid());

  scoped_refptr<DXGISharedHandleState> orig_state =
      dxgi_shared_handle_manager_->GetOrCreateSharedHandleState(
          orig_token, std::move(orig_handle), d3d11_device_);
  ASSERT_NE(orig_state, nullptr);

  EXPECT_EQ(dxgi_shared_handle_manager_->GetSharedHandleMapSizeForTesting(),
            1u);

  base::Lock lock;
  base::ConditionVariable cv(&lock);

  base::AutoLock auto_lock(lock);
  int remaining_handles = kNumHandles - 1;

  for (int i = 0; i < remaining_handles; i++) {
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          HANDLE handle;
          ::DuplicateHandle(::GetCurrentProcess(),
                            orig_state->GetSharedHandle(),
                            ::GetCurrentProcess(), &handle,
                            /*dwDesiredAccess=*/0,
                            /*bInerhitHandle=*/FALSE, DUPLICATE_SAME_ACCESS);
          base::win::ScopedHandle new_handle(handle);
          ASSERT_TRUE(new_handle.IsValid());

          scoped_refptr<DXGISharedHandleState> state =
              dxgi_shared_handle_manager_->GetOrCreateSharedHandleState(
                  orig_token, std::move(new_handle), d3d11_device_);
          EXPECT_EQ(state, orig_state);

          EXPECT_EQ(
              dxgi_shared_handle_manager_->GetSharedHandleMapSizeForTesting(),
              1u);

          state = nullptr;

          EXPECT_EQ(
              dxgi_shared_handle_manager_->GetSharedHandleMapSizeForTesting(),
              1u);

          base::AutoLock auto_lock(lock);
          remaining_handles--;
          cv.Signal();
        }));
  }

  while (remaining_handles > 0)
    cv.Wait();

  EXPECT_EQ(dxgi_shared_handle_manager_->GetSharedHandleMapSizeForTesting(),
            1u);

  orig_state = nullptr;

  EXPECT_EQ(dxgi_shared_handle_manager_->GetSharedHandleMapSizeForTesting(),
            0u);
}

}  // anonymous namespace
}  // namespace gpu

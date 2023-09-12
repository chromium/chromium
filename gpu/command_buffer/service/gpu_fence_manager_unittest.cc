// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_fence_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/error_state_mock.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/switches.h"
#include "ui/gl/egl_mock.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_utils.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace gpu {
namespace gles2 {

class GpuFenceManagerTest : public GpuServiceTest {
 public:
  GpuFenceManagerTest() {
    GpuDriverBugWorkarounds gpu_driver_bug_workaround;
    feature_info_ =
        new FeatureInfo(gpu_driver_bug_workaround, GpuFeatureInfo());
  }

  ~GpuFenceManagerTest() override {}

 protected:
  void SetUp() override {
    GpuServiceTest::SetUp();
    SetupMockEGL("EGL_ANDROID_native_fence_sync EGL_KHR_wait_sync");
    SetupFeatureInfo("", "OpenGL ES 2.0", CONTEXT_TYPE_OPENGLES2);
    error_state_ = std::make_unique<::testing::StrictMock<MockErrorState>>();
    manager_ = std::make_unique<GpuFenceManager>();
  }

  void TearDown() override {
    manager_->Destroy(false);
    manager_.reset();
    GpuServiceTest::TearDown();
    TeardownMockEGL();
  }

  void SetupMockEGL(const char* extensions) {
    gl::SetGLGetProcAddressProc(gl::MockEGLInterface::GetGLProcAddress);
    egl_ = std::make_unique<::testing::NiceMock<::gl::MockEGLInterface>>();
    gl::MockEGLInterface::SetEGLInterface(egl_.get());

    const EGLDisplay kDummyDisplay = reinterpret_cast<EGLDisplay>(0x1001);
    ON_CALL(*egl_, QueryString(_, EGL_EXTENSIONS))
        .WillByDefault(Return(extensions));
    ON_CALL(*egl_, GetCurrentDisplay()).WillByDefault(Return(kDummyDisplay));
    ON_CALL(*egl_, GetDisplay(_)).WillByDefault(Return(kDummyDisplay));
    ON_CALL(*egl_, Initialize(_, _, _)).WillByDefault(Return(true));
    ON_CALL(*egl_, Terminate(_)).WillByDefault(Return(true));

    gl::ClearBindingsEGL();
    gl::InitializeStaticGLBindingsEGL();
    display_ = gl::GetDefaultDisplayEGL();
    display_->InitializeForTesting();
  }

  void TeardownMockEGL() {
    if (display_)
      display_->Shutdown();
    egl_.reset();
    gl::ClearBindingsEGL();
  }

  void SetupFeatureInfo(const char* gl_extensions,
                        const char* gl_version,
                        ContextType context_type) {
    TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
        gl_.get(), gl_extensions, "", gl_version, context_type);
    feature_info_->InitializeForTesting(context_type);
    ASSERT_TRUE(feature_info_->context_type() == context_type);
  }

  scoped_refptr<FeatureInfo> feature_info_;
  std::unique_ptr<GpuFenceManager> manager_;
  std::unique_ptr<MockErrorState> error_state_;
  std::unique_ptr<::testing::NiceMock<::gl::MockEGLInterface>> egl_;
  raw_ptr<gl::GLDisplayEGL> display_ = nullptr;
};

TEST_F(GpuFenceManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kClient2Id = 2;
  const EGLSyncKHR kDummySync = reinterpret_cast<EGLSyncKHR>(0x2001);

  // Sanity check that our client IDs are invalid at start.
  EXPECT_FALSE(manager_->IsValidGpuFence(kClient1Id));
  EXPECT_FALSE(manager_->IsValidGpuFence(kClient2Id));

  // Creating a new fence creates an underlying native sync object.
  EXPECT_CALL(*egl_, CreateSyncKHR(_, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr))
      .Times(1)
      .WillOnce(Return(kDummySync))
      .RetiresOnSaturation();
  EXPECT_CALL(*egl_, GetSyncAttribKHR(_, kDummySync, EGL_SYNC_STATUS_KHR, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<3>(EGL_UNSIGNALED_KHR), Return(EGL_TRUE)));
  EXPECT_CALL(*gl_, Flush()).Times(1).RetiresOnSaturation();
  EXPECT_TRUE(manager_->CreateGpuFence(kClient1Id));
  EXPECT_TRUE(manager_->IsValidGpuFence(kClient1Id));

  // Try a server wait on it.
  EXPECT_CALL(*egl_, WaitSyncKHR(_, kDummySync, _))
      .Times(1)
      .WillOnce(Return(EGL_TRUE))
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->GpuFenceServerWait(kClient1Id));

  // Removing the fence marks it invalid.
  EXPECT_CALL(*egl_, DestroySyncKHR(_, kDummySync))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->RemoveGpuFence(kClient1Id));
  EXPECT_FALSE(manager_->IsValidGpuFence(kClient1Id));

  // Removing a non-existent fence does not crash.
  EXPECT_FALSE(manager_->RemoveGpuFence(kClient2Id));
}

TEST_F(GpuFenceManagerTest, Destruction) {
  const GLuint kClient1Id = 1;
  const EGLSyncKHR kDummySync = reinterpret_cast<EGLSyncKHR>(0x2001);

  // Sanity check that our client IDs are invalid at start.
  EXPECT_FALSE(manager_->IsValidGpuFence(kClient1Id));

  // Create a fence object.
  EXPECT_CALL(*egl_, CreateSyncKHR(_, _, _)).WillOnce(Return(kDummySync));
  EXPECT_CALL(*gl_, Flush()).Times(1).RetiresOnSaturation();
  EXPECT_TRUE(manager_->CreateGpuFence(kClient1Id));
  EXPECT_TRUE(manager_->IsValidGpuFence(kClient1Id));

  // Destroying the manager destroys any pending sync objects.
  EXPECT_CALL(*egl_, DestroySyncKHR(_, kDummySync))
      .Times(1)
      .RetiresOnSaturation();
  manager_->Destroy(true);
}

#if BUILDFLAG(IS_POSIX)

TEST_F(GpuFenceManagerTest, SmartHandleImplmentation) {
  if (!base::FeatureList::IsEnabled(features::kUseSmartRefForGPUFenceHandle)) {
    GTEST_SKIP();
  }
  const GLuint kClient1Id = 1;
  const EGLSyncKHR kDummySync = reinterpret_cast<EGLSyncKHR>(0x2001);
  const EGLint kFenceFD = dup(1);

  // Creating a new fence creates an underlying native sync object.
  EXPECT_CALL(*egl_, CreateSyncKHR(_, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr))
      .Times(1)
      .WillOnce(Return(kDummySync))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, Flush()).Times(1).RetiresOnSaturation();
  EXPECT_TRUE(manager_->CreateGpuFence(kClient1Id));
  EXPECT_TRUE(manager_->IsValidGpuFence(kClient1Id));

  // Get a GpuFence and its GpuFenceHandle.

  // We use a dup'ed STDOUT as the file descriptor for testing. Since
  // it will be closed on GpuFence destruction, only return it one time.
  EXPECT_CALL(*egl_, DupNativeFenceFDANDROID(_, kDummySync))
      .Times(1)
      .WillOnce(Return(kFenceFD))
      .RetiresOnSaturation();
  gfx::GpuFenceHandle handle;
  {
    std::unique_ptr<gfx::GpuFence> gpu_fence =
        manager_->GetGpuFence(kClient1Id);
    EXPECT_TRUE(gpu_fence);
    handle = gpu_fence->GetGpuFenceHandle().Clone();
  }

  EXPECT_EQ(handle.Peek(), kFenceFD);
  {
    gfx::GpuFenceHandle handle2 = handle.Clone();

    EXPECT_EQ(handle2.Peek(), kFenceFD);
    auto scoped_fence = handle2.Release();
    // There are multiple references to the same underlying fence so when we
    // 'Release' the fence will have a different fd number corresponding to a
    // 'dup'.
    EXPECT_NE(scoped_fence.get(), kFenceFD);
  }
  EXPECT_EQ(handle.Peek(), kFenceFD);
}

TEST_F(GpuFenceManagerTest, SmartHandleOptimization) {
  if (!base::FeatureList::IsEnabled(features::kUseSmartRefForGPUFenceHandle)) {
    GTEST_SKIP();
  }
  const GLuint kClient1Id = 1;
  const EGLSyncKHR kDummySync = reinterpret_cast<EGLSyncKHR>(0x2001);
  const EGLint kFenceFD = dup(1);

  // Creating a new fence creates an underlying native sync object.
  EXPECT_CALL(*egl_, CreateSyncKHR(_, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr))
      .Times(1)
      .WillOnce(Return(kDummySync))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, Flush()).Times(1).RetiresOnSaturation();
  EXPECT_TRUE(manager_->CreateGpuFence(kClient1Id));
  EXPECT_TRUE(manager_->IsValidGpuFence(kClient1Id));

  // Get a GpuFence and its GpuFenceHandle.

  // We use a dup'ed STDOUT as the file descriptor for testing. Since
  // it will be closed on GpuFence destruction, only return it one time.
  EXPECT_CALL(*egl_, DupNativeFenceFDANDROID(_, kDummySync))
      .Times(1)
      .WillOnce(Return(kFenceFD))
      .RetiresOnSaturation();
  gfx::GpuFenceHandle handle;
  {
    std::unique_ptr<gfx::GpuFence> gpu_fence =
        manager_->GetGpuFence(kClient1Id);
    EXPECT_TRUE(gpu_fence);
    handle = gpu_fence->GetGpuFenceHandle().Clone();
  }

  EXPECT_EQ(handle.Peek(), kFenceFD);
  gfx::GpuFenceHandle handle2 = handle.Clone();
  EXPECT_EQ(handle2.Peek(), kFenceFD);
  // Drop reference in original 'handle'.
  handle.Reset();
  EXPECT_TRUE(handle.is_null());
  EXPECT_EQ(handle.Peek(), -1);
  // Single reference release optimization returns the original underlying FD.
  auto scoped_fence = handle2.Release();
  EXPECT_EQ(scoped_fence.get(), kFenceFD);
}

TEST_F(GpuFenceManagerTest, GetGpuFence) {
  const GLuint kClient1Id = 1;
  const EGLSyncKHR kDummySync = reinterpret_cast<EGLSyncKHR>(0x2001);
  const EGLint kFenceFD = dup(1);

  // Creating a new fence creates an underlying native sync object.
  EXPECT_CALL(*egl_, CreateSyncKHR(_, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr))
      .Times(1)
      .WillOnce(Return(kDummySync))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, Flush()).Times(1).RetiresOnSaturation();
  EXPECT_TRUE(manager_->CreateGpuFence(kClient1Id));
  EXPECT_TRUE(manager_->IsValidGpuFence(kClient1Id));

  // Get a GpuFence and its GpuFenceHandle.

  // We use a dup'ed STDOUT as the file descriptor for testing. Since
  // it will be closed on GpuFence destruction, only return it one time.
  EXPECT_CALL(*egl_, DupNativeFenceFDANDROID(_, kDummySync))
      .Times(1)
      .WillOnce(Return(kFenceFD))
      .RetiresOnSaturation();
  std::unique_ptr<gfx::GpuFence> gpu_fence = manager_->GetGpuFence(kClient1Id);
  EXPECT_TRUE(gpu_fence);
  const gfx::GpuFenceHandle& handle = gpu_fence->GetGpuFenceHandle();

  EXPECT_EQ(handle.Peek(), kFenceFD);

  // Removing the fence marks it invalid.
  EXPECT_CALL(*egl_, DestroySyncKHR(_, kDummySync))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->RemoveGpuFence(kClient1Id));
}

TEST_F(GpuFenceManagerTest, Duplication) {
  const GLuint kClient1Id = 1;
  const EGLSyncKHR kDummySync = reinterpret_cast<EGLSyncKHR>(0x2001);
  const EGLint kFenceFD = dup(1);

  // Sanity check that our client IDs are invalid at start.
  EXPECT_FALSE(manager_->IsValidGpuFence(kClient1Id));

  // Create a handle.
  gfx::GpuFenceHandle handle;
  handle.Adopt(base::ScopedFD(kFenceFD));

  // Create a duplicate fence object from it.
  EXPECT_CALL(*egl_, CreateSyncKHR(_, EGL_SYNC_NATIVE_FENCE_ANDROID, _))
      .Times(1)
      .WillOnce(Return(kDummySync))
      .RetiresOnSaturation();
  EXPECT_CALL(*egl_, GetSyncAttribKHR(_, kDummySync, EGL_SYNC_STATUS_KHR, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<3>(EGL_UNSIGNALED_KHR), Return(EGL_TRUE)));
  EXPECT_CALL(*gl_, Flush()).Times(1).RetiresOnSaturation();
  EXPECT_TRUE(
      manager_->CreateGpuFenceFromHandle(kClient1Id, std::move(handle)));
  EXPECT_TRUE(manager_->IsValidGpuFence(kClient1Id));

  // Try a server wait on it.
  EXPECT_CALL(*egl_, WaitSyncKHR(_, kDummySync, _))
      .Times(1)
      .WillOnce(Return(EGL_TRUE))
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->GpuFenceServerWait(kClient1Id));

  // Cleanup.
  EXPECT_TRUE(manager_->RemoveGpuFence(kClient1Id));
  EXPECT_FALSE(manager_->IsValidGpuFence(kClient1Id));
}

#endif  // BUILDFLAG(IS_POSIX)

}  // namespace gles2
}  // namespace gpu

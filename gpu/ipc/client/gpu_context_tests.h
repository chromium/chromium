// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_GPU_CONTEXT_TESTS_H_
#define GPU_IPC_CLIENT_GPU_CONTEXT_TESTS_H_

// These tests are run twice:
// Once in a gpu test with an in-process command buffer.
// Once in a browsertest with an out-of-process command buffer and gpu-process.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_fence.h"

namespace {

class SignalTest : public ContextTestBase {
 public:
  // These tests should time out if the callback doesn't get called.
  void TestSignalSyncToken(const gpu::SyncToken& sync_token) {
    base::RunLoop run_loop;
    context_support_->SignalSyncToken(sync_token, run_loop.QuitClosure());
    run_loop.Run();
  }

  // These tests should time out if the callback doesn't get called.
  void TestSignalQuery(GLuint query) {
    base::RunLoop run_loop;
    context_support_->SignalQuery(query, run_loop.QuitClosure());
    run_loop.Run();
  }
};

CONTEXT_TEST_F(SignalTest, BasicSignalSyncTokenTest) {
#if BUILDFLAG(IS_WIN)
  // The IPC version of ContextTestBase::SetUpOnMainThread does not succeed on
  // some platforms.
  if (!gl_)
    return;
#endif

  gpu::SyncToken sync_token;
  gl_->GenSyncTokenCHROMIUM(sync_token.GetData());

  TestSignalSyncToken(sync_token);
}

CONTEXT_TEST_F(SignalTest, EmptySignalSyncTokenTest) {
#if BUILDFLAG(IS_WIN)
  // The IPC version of ContextTestBase::SetUpOnMainThread does not succeed on
  // some platforms.
  if (!gl_)
    return;
#endif

  // Signalling something that doesn't exist should run the callback
  // immediately.
  gpu::SyncToken sync_token;
  TestSignalSyncToken(sync_token);
}

CONTEXT_TEST_F(SignalTest, InvalidSignalSyncTokenTest) {
#if BUILDFLAG(IS_WIN)
  // The IPC version of ContextTestBase::SetUpOnMainThread does not succeed on
  // some platforms.
  if (!gl_)
    return;
#endif

  // Signalling something that doesn't exist should run the callback
  // immediately.
  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(1297824234),
                            9123743439);
  TestSignalSyncToken(sync_token);
}

CONTEXT_TEST_F(SignalTest, BasicSignalQueryTest) {
#if BUILDFLAG(IS_WIN)
  // The IPC version of ContextTestBase::SetUpOnMainThread does not succeed on
  // some platforms.
  if (!gl_)
    return;
#endif

  unsigned query;
  gl_->GenQueriesEXT(1, &query);
  gl_->BeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, query);
  gl_->Finish();
  gl_->EndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
  TestSignalQuery(query);
  gl_->DeleteQueriesEXT(1, &query);
}

CONTEXT_TEST_F(SignalTest, SignalQueryUnboundTest) {
#if BUILDFLAG(IS_WIN)
  // The IPC version of ContextTestBase::SetUpOnMainThread does not succeed on
  // some platforms.
  if (!gl_)
    return;
#endif

  GLuint query;
  gl_->GenQueriesEXT(1, &query);
  TestSignalQuery(query);
  gl_->DeleteQueriesEXT(1, &query);
}

CONTEXT_TEST_F(SignalTest, InvalidSignalQueryUnboundTest) {
#if BUILDFLAG(IS_WIN)
  // The IPC version of ContextTestBase::SetUpOnMainThread does not succeed on
  // some platforms.
  if (!gl_)
    return;
#endif

  // Signalling something that doesn't exist should run the callback
  // immediately.
  TestSignalQuery(928729087);
  TestSignalQuery(928729086);
  TestSignalQuery(928729085);
  TestSignalQuery(928729083);
  TestSignalQuery(928729082);
  TestSignalQuery(928729081);
}

// The GpuFenceTest doesn't currently work on ChromeOS, apparently
// due to inconsistent initialization of InProcessCommandBuffer which
// isn't used on that platform. Restrict it to Android for now.

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)

class GpuFenceTest : public ContextTestBase {
 public:
  GpuFenceTest();
  ~GpuFenceTest() override;

  void TestReceive(base::OnceClosure quit_cb,
                   std::unique_ptr<gfx::GpuFence> gpu_fence) {
    EXPECT_TRUE(gpu_fence);
    received_gpu_fence_ = std::move(gpu_fence);
    std::move(quit_cb).Run();
  }

  std::unique_ptr<gfx::GpuFence> received_gpu_fence_;
};

GpuFenceTest::GpuFenceTest() = default;
GpuFenceTest::~GpuFenceTest() = default;

CONTEXT_TEST_F(GpuFenceTest, BasicGpuFenceTest) {
  // Skip test if GpuFence is not supported.
  if (!gl::GLFence::IsGpuFenceSupported())
    return;

  GLuint id1 = gl_->CreateGpuFenceCHROMIUM();
  EXPECT_NE(id1, 0U);

  // These tests should time out if the callback doesn't get called.
  base::RunLoop run_loop;

  base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback =
      base::BindOnce(&GpuFenceTest::TestReceive, base::Unretained(this),
                     run_loop.QuitClosure());

  // Receive and store a local GpuFence.
  context_support_->GetGpuFence(id1, std::move(callback));
  run_loop.Run();

  gl_->DestroyGpuFenceCHROMIUM(id1);

  // GPU fence should be valid.
  EXPECT_TRUE(received_gpu_fence_);

  // Send a copy of this fence back via command buffer GL.
  GLuint id2 = gl_->CreateClientGpuFenceCHROMIUM(
      received_gpu_fence_->AsClientGpuFence());
  gl_->ShallowFlushCHROMIUM();
  EXPECT_NE(id2, 0U);
  EXPECT_NE(id2, id1);
  gl_->WaitGpuFenceCHROMIUM(id2);
  gl_->DestroyGpuFenceCHROMIUM(id2);
}

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)

}  // namespace

#endif  // GPU_IPC_CLIENT_GPU_CONTEXT_TESTS_H_

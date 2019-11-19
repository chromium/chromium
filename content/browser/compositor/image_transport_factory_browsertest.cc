// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/image_transport_factory.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/test/content_browser_test.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/compositor.h"

namespace content {
namespace {

using ImageTransportFactoryBrowserTest = ContentBrowserTest;

class MockContextFactoryObserver : public ui::ContextFactoryObserver {
 public:
  MOCK_METHOD0(OnLostSharedContext, void());
};

// Flaky on ChromeOS: crbug.com/394083
#if defined(OS_CHROMEOS)
#define MAYBE_TestLostContext DISABLED_TestLostContext
#else
#define MAYBE_TestLostContext TestLostContext
#endif
// Checks that upon context loss the observer is notified.
IN_PROC_BROWSER_TEST_F(ImageTransportFactoryBrowserTest,
                       MAYBE_TestLostContext) {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();

  // This test doesn't make sense in software compositing mode.
  if (GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled())
    return;

  MockContextFactoryObserver observer;
  factory->GetContextFactory()->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLostSharedContext())
      .WillOnce(testing::Invoke(&run_loop, &base::RunLoop::Quit));

  scoped_refptr<viz::ContextProvider> context_provider =
      factory->GetContextFactory()->SharedMainThreadContextProvider();
  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  gl->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                          GL_INNOCENT_CONTEXT_RESET_ARB);

  // We have to flush to make sure that the client side gets a chance to notice
  // the context is gone.
  gl->Flush();

  run_loop.Run();

  factory->GetContextFactory()->RemoveObserver(&observer);
}

}  // anonymous namespace
}  // namespace content

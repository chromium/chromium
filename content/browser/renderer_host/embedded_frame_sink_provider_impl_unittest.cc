// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/embedded_frame_sink_provider_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_host_frame_sink_client.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/embedded_frame_sink_impl.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom.h"
#include "ui/compositor/compositor.h"

#if !defined(OS_ANDROID)
#include "content/browser/compositor/image_transport_factory.h"
#endif

using testing::ElementsAre;
using testing::IsEmpty;

namespace content {
namespace {

constexpr uint32_t kRendererClientId = 3;
constexpr viz::FrameSinkId kFrameSinkParent(kRendererClientId, 1);
constexpr viz::FrameSinkId kFrameSinkA(kRendererClientId, 3);
constexpr viz::FrameSinkId kFrameSinkB(kRendererClientId, 4);

// Runs RunLoop until |endpoint| encounters a connection error.
template <class T>
void WaitForConnectionError(T* endpoint) {
  base::RunLoop run_loop;
  endpoint->set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();
}

// Stub EmbeddedFrameSinkClient that stores the latest SurfaceInfo.
class StubEmbeddedFrameSinkClient
    : public blink::mojom::EmbeddedFrameSinkClient,
      public blink::mojom::SurfaceEmbedder {
 public:
  StubEmbeddedFrameSinkClient() = default;
  ~StubEmbeddedFrameSinkClient() override = default;

  mojo::PendingRemote<blink::mojom::EmbeddedFrameSinkClient>
  GetInterfaceRemote() {
    mojo::PendingRemote<blink::mojom::EmbeddedFrameSinkClient> client;
    receiver_.Bind(client.InitWithNewPipeAndPassReceiver());
    receiver_.set_disconnect_handler(
        base::BindOnce([](bool* error_variable) { *error_variable = true; },
                       &connection_error_));
    return client;
  }

  void Close() { receiver_.reset(); }

  const viz::LocalSurfaceId& last_received_local_surface_id() const {
    return last_received_local_surface_id_;
  }

  bool connection_error() const { return connection_error_; }

 private:
  // blink::mojom::EmbeddedFrameSinkClient:
  void BindSurfaceEmbedder(
      mojo::PendingReceiver<blink::mojom::SurfaceEmbedder> receiver) override {
    surface_embedder_receiver_.Bind(std::move(receiver));
  }

  // blink::mojom::SurfaceEmbedder implementation:
  void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id) override {
    last_received_local_surface_id_ = local_surface_id;
  }

  mojo::Receiver<blink::mojom::SurfaceEmbedder> surface_embedder_receiver_{
      this};
  mojo::Receiver<blink::mojom::EmbeddedFrameSinkClient> receiver_{this};
  viz::LocalSurfaceId last_received_local_surface_id_;
  bool connection_error_ = false;

  DISALLOW_COPY_AND_ASSIGN(StubEmbeddedFrameSinkClient);
};

}  // namespace

class EmbeddedFrameSinkProviderImplTest : public testing::Test {
 public:
  EmbeddedFrameSinkProviderImpl* provider() { return provider_.get(); }

  // Gets the EmbeddedFrameSinkImpl for |frame_sink_id| or null if it doesn't
  // exist.
  EmbeddedFrameSinkImpl* GetEmbeddedFrameSink(
      const viz::FrameSinkId& frame_sink_id) {
    auto iter = provider_->frame_sink_map_.find(frame_sink_id);
    if (iter == provider_->frame_sink_map_.end())
      return nullptr;
    return iter->second.get();
  }

  // Gets list of FrameSinkId for all EmbeddedFrameSinkImpls.
  std::vector<viz::FrameSinkId> GetAllCanvases() {
    std::vector<viz::FrameSinkId> frame_sink_ids;
    for (auto& map_entry : provider_->frame_sink_map_)
      frame_sink_ids.push_back(map_entry.second->frame_sink_id());
    std::sort(frame_sink_ids.begin(), frame_sink_ids.end());
    return frame_sink_ids;
  }

  void DeleteEmbeddedFrameSinkProviderImpl() { provider_.reset(); }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

 protected:
  void SetUp() override {
    host_frame_sink_manager_ = std::make_unique<viz::HostFrameSinkManager>();

    // The FrameSinkManagerImpl implementation is in-process here for tests.
    frame_sink_manager_ =
        std::make_unique<viz::FrameSinkManagerImpl>(&shared_bitmap_manager_);
    surface_utils::ConnectWithLocalFrameSinkManager(
        host_frame_sink_manager_.get(), frame_sink_manager_.get());

    provider_ = std::make_unique<EmbeddedFrameSinkProviderImpl>(
        host_frame_sink_manager_.get(), kRendererClientId);

    host_frame_sink_manager_->RegisterFrameSinkId(
        kFrameSinkParent, &host_frame_sink_client_,
        viz::ReportFirstSurfaceActivation::kYes);
  }
  void TearDown() override {
    host_frame_sink_manager_->InvalidateFrameSinkId(kFrameSinkParent);
    provider_.reset();
    host_frame_sink_manager_.reset();
    frame_sink_manager_.reset();
  }

 private:
  // A MessageLoop is required for mojo bindings which are used to
  // connect to graphics services.
  base::test::SingleThreadTaskEnvironment task_environment_;
  viz::ServerSharedBitmapManager shared_bitmap_manager_;
  viz::FakeHostFrameSinkClient host_frame_sink_client_;
  std::unique_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<EmbeddedFrameSinkProviderImpl> provider_;
};

// Mimics the workflow of OffscreenCanvas.commit() on renderer process.
TEST_F(EmbeddedFrameSinkProviderImplTest,
       SingleHTMLCanvasElementTransferToOffscreen) {
  // Mimic connection from the renderer main thread to browser.
  StubEmbeddedFrameSinkClient efs_client;
  provider()->RegisterEmbeddedFrameSink(kFrameSinkParent, kFrameSinkA,
                                        efs_client.GetInterfaceRemote());

  EmbeddedFrameSinkImpl* efs_impl = GetEmbeddedFrameSink(kFrameSinkA);

  // There should be a single EmbeddedFrameSinkImpl and it should have the
  // provided FrameSinkId.
  EXPECT_EQ(kFrameSinkA, efs_impl->frame_sink_id());
  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA));

  // Mimic connection from the renderer main or worker thread to browser.
  mojo::Remote<viz::mojom::CompositorFrameSink> compositor_frame_sink;
  viz::MockCompositorFrameSinkClient compositor_frame_sink_client;
  mojo::Remote<blink::mojom::SurfaceEmbedder> surface_embedder;
  provider()->CreateCompositorFrameSink(
      kFrameSinkA, compositor_frame_sink_client.BindInterfaceRemote(),
      compositor_frame_sink.BindNewPipeAndPassReceiver());
  provider()->ConnectToEmbedder(kFrameSinkA,
                                surface_embedder.BindNewPipeAndPassReceiver());

  // Renderer submits a CompositorFrame with |local_id|.
  const viz::LocalSurfaceId local_id(1, base::UnguessableToken::Create());
  compositor_frame_sink->SubmitCompositorFrame(
      local_id, viz::MakeDefaultCompositorFrame(), base::nullopt, 0);

  RunUntilIdle();

  // EmbeddedFrameSinkImpl in browser should not be obversing surface
  // activations so it should not know about |local_id|.
  EXPECT_NE(local_id, efs_impl->local_surface_id());
  EXPECT_FALSE(efs_impl->local_surface_id().is_valid());

  // Notify the embedder of the new LocalSurfaceId.
  surface_embedder->SetLocalSurfaceId(local_id);
  RunUntilIdle();

  // EmbeddedFrameSinkClient in the renderer should now know about |local_id|.
  EXPECT_EQ(local_id, efs_client.last_received_local_surface_id());
}

// Check that renderer closing the mojom::EmbeddedFrameSinkClient connection
// destroys the EmbeddedFrameSinkImpl in browser.
TEST_F(EmbeddedFrameSinkProviderImplTest, ClientClosesConnection) {
  StubEmbeddedFrameSinkClient efs_client;
  provider()->RegisterEmbeddedFrameSink(kFrameSinkParent, kFrameSinkA,
                                        efs_client.GetInterfaceRemote());

  RunUntilIdle();

  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA));

  // Mimic closing the connection from the renderer.
  efs_client.Close();

  RunUntilIdle();

  // The renderer closing the connection should destroy the
  // EmbeddedFrameSinkImpl.
  EXPECT_THAT(GetAllCanvases(), IsEmpty());
}

// Check that destroying EmbeddedFrameSinkProviderImpl closes connection to
// renderer.
TEST_F(EmbeddedFrameSinkProviderImplTest, ProviderClosesConnections) {
  StubEmbeddedFrameSinkClient efs_client;
  provider()->RegisterEmbeddedFrameSink(kFrameSinkParent, kFrameSinkA,
                                        efs_client.GetInterfaceRemote());

  RunUntilIdle();

  // There should be a EmbeddedFrameSinkImpl and |efs_client| should be
  // bound.
  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA));
  EXPECT_FALSE(efs_client.connection_error());

  // Delete EmbeddedFrameSinkProviderImpl before client disconnects.
  DeleteEmbeddedFrameSinkProviderImpl();

  RunUntilIdle();

  // This should destroy the EmbeddedFrameSinkImpl and close the connection
  // to |efs_client| triggering a connection error.
  EXPECT_TRUE(efs_client.connection_error());
}

// Check that connecting CompositorFrameSink without first making a
// EmbeddedFrameSink connection fails.
TEST_F(EmbeddedFrameSinkProviderImplTest, ClientConnectionWrongOrder) {
  // Mimic connection from the renderer main or worker thread.
  mojo::Remote<viz::mojom::CompositorFrameSink> compositor_frame_sink;
  viz::MockCompositorFrameSinkClient compositor_frame_sink_client;
  // Try to connect CompositorFrameSink without first making
  // EmbeddedFrameSink connection. This should fail.
  provider()->CreateCompositorFrameSink(
      kFrameSinkA, compositor_frame_sink_client.BindInterfaceRemote(),
      compositor_frame_sink.BindNewPipeAndPassReceiver());

  // The request will fail and trigger a connection error.
  WaitForConnectionError(&compositor_frame_sink);
}

// Check that trying to create an EmbeddedFrameSinkImpl when the parent
// FrameSinkId has already been invalidated fails.
TEST_F(EmbeddedFrameSinkProviderImplTest, ParentNotRegistered) {
  StubEmbeddedFrameSinkClient efs_client;
  provider()->RegisterEmbeddedFrameSink(kFrameSinkA, kFrameSinkB,
                                        efs_client.GetInterfaceRemote());

  mojo::Remote<viz::mojom::CompositorFrameSink> compositor_frame_sink;
  viz::MockCompositorFrameSinkClient compositor_frame_sink_client;
  // The embedder, kFrameSinkA, has already been invalidated and isn't
  // registered at this point. This request should fail.
  provider()->CreateCompositorFrameSink(
      kFrameSinkB, compositor_frame_sink_client.BindInterfaceRemote(),
      compositor_frame_sink.BindNewPipeAndPassReceiver());

  // The request will fail and trigger a connection error.
  WaitForConnectionError(&compositor_frame_sink);
}

// Check that trying to create an EmbeddedFrameSinkImpl with a client id
// that doesn't match the renderer fails.
TEST_F(EmbeddedFrameSinkProviderImplTest, InvalidClientId) {
  const viz::FrameSinkId invalid_frame_sink_id(4, 3);
  EXPECT_NE(kRendererClientId, invalid_frame_sink_id.client_id());

  StubEmbeddedFrameSinkClient efs_client;
  provider()->RegisterEmbeddedFrameSink(kFrameSinkParent, invalid_frame_sink_id,
                                        efs_client.GetInterfaceRemote());

  RunUntilIdle();

  // No EmbeddedFrameSinkImpl should have been created.
  EXPECT_THAT(GetAllCanvases(), IsEmpty());

  // The connection for |efs_client| will have failed and triggered a
  // connection error.
  EXPECT_TRUE(efs_client.connection_error());
}

// Mimic renderer with two offscreen canvases.
TEST_F(EmbeddedFrameSinkProviderImplTest,
       MultiHTMLCanvasElementTransferToOffscreen) {
  StubEmbeddedFrameSinkClient efs_client_a;
  provider()->RegisterEmbeddedFrameSink(kFrameSinkParent, kFrameSinkA,
                                        efs_client_a.GetInterfaceRemote());

  StubEmbeddedFrameSinkClient efs_client_b;
  provider()->RegisterEmbeddedFrameSink(kFrameSinkParent, kFrameSinkB,
                                        efs_client_b.GetInterfaceRemote());

  RunUntilIdle();

  // There should be two EmbeddedFrameSinkImpls created.
  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkA, kFrameSinkB));

  // Mimic closing first connection from the renderer.
  efs_client_a.Close();

  RunUntilIdle();

  EXPECT_THAT(GetAllCanvases(), ElementsAre(kFrameSinkB));

  // Mimic closing second connection from the renderer.
  efs_client_b.Close();

  RunUntilIdle();

  EXPECT_THAT(GetAllCanvases(), IsEmpty());
}

}  // namespace content

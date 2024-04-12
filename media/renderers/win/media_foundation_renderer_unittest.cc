// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_renderer.h"

#include <windows.media.protection.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/win/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

using ABI::Windows::Media::Protection::IMediaProtectionPMPServer;
using Microsoft::WRL::ComPtr;

class MockMediaFoundationCdmProxy : public MediaFoundationCdmProxy {
 public:
  MockMediaFoundationCdmProxy();

  // MediaFoundationCdmProxy.
  MOCK_METHOD2(GetPMPServer, HRESULT(REFIID riid, void** object_result));
  MOCK_METHOD6(GetInputTrustAuthority,
               HRESULT(uint32_t stream_id,
                       uint32_t stream_count,
                       const uint8_t* content_init_data,
                       uint32_t content_init_data_size,
                       REFIID riid,
                       IUnknown** object_result));
  MOCK_METHOD2(SetLastKeyId, HRESULT(uint32_t stream_id, REFGUID key_id));
  MOCK_METHOD0(RefreshTrustedInput, HRESULT());
  MOCK_METHOD2(ProcessContentEnabler,
               HRESULT(IUnknown* request, IMFAsyncResult* result));
  MOCK_METHOD0(OnHardwareContextReset, void());
  MOCK_METHOD0(OnSignificantPlayback, void());
  MOCK_METHOD1(OnPlaybackError, void(HRESULT));

 protected:
  ~MockMediaFoundationCdmProxy() override;
};

MockMediaFoundationCdmProxy::MockMediaFoundationCdmProxy() = default;
MockMediaFoundationCdmProxy::~MockMediaFoundationCdmProxy() = default;

class MockMediaProtectionPMPServer
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          IMediaProtectionPMPServer> {
 public:
  MockMediaProtectionPMPServer() = default;
  ~MockMediaProtectionPMPServer() override = default;

  static HRESULT MakeMockMediaProtectionPMPServer(
      IMediaProtectionPMPServer** pmp_server) {
    *pmp_server = Microsoft::WRL::Make<MockMediaProtectionPMPServer>().Detach();
    return S_OK;
  }

  // Return E_NOINTERFACE to avoid a crash when MFMediaEngine tries to use the
  // mocked IPropertySet from get_Properties().
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void** object_result) override {
    return E_NOINTERFACE;
  }

  // ABI::Windows::Media::Protection::IMediaProtectionPMPServer.
  MOCK_STDCALL_METHOD1(
      get_Properties,
      HRESULT(
          ABI::Windows::Foundation::Collections::IPropertySet** properties));
};

class MediaFoundationRendererTest : public testing::Test {
 public:
  MediaFoundationRendererTest() {
    mf_cdm_proxy_ =
        base::MakeRefCounted<NiceMock<MockMediaFoundationCdmProxy>>();

    // MF MediaEngine holds IMFMediaSource (MediaFoundationSourceWrapper) even
    // after the test finishes, which holds a reference to the `mf_cdm_proxy_`.
    testing::Mock::AllowLeak(mf_cdm_proxy_.get());

    MockMediaProtectionPMPServer::MakeMockMediaProtectionPMPServer(
        &pmp_server_);

    LUID empty_luid{0, 0};
    mf_renderer_ = std::make_unique<MediaFoundationRenderer>(
        task_environment_.GetMainThreadTaskRunner(),
        std::make_unique<NullMediaLog>(), empty_luid);

    // Some default actions.
    ON_CALL(cdm_context_, GetMediaFoundationCdmProxy())
        .WillByDefault(Return(mf_cdm_proxy_));
    ON_CALL(*mf_cdm_proxy_, GetPMPServer(_, _))
        .WillByDefault(
            Invoke(this, &MediaFoundationRendererTest::GetPMPServer));

    // Some expected calls with return values.
    EXPECT_CALL(media_resource_, GetAllStreams())
        .WillRepeatedly(
            Invoke(this, &MediaFoundationRendererTest::GetAllStreams));
    EXPECT_CALL(media_resource_, GetType())
        .WillRepeatedly(Return(MediaResource::Type::kStream));
  }

  ~MediaFoundationRendererTest() override { mf_renderer_.reset(); }

  void AddStream(DemuxerStream::Type type, bool encrypted) {
    streams_.push_back(CreateMockDemuxerStream(type, encrypted));
  }

  std::vector<DemuxerStream*> GetAllStreams() {
    std::vector<DemuxerStream*> streams;

    for (auto& stream : streams_) {
      streams.push_back(stream.get());
    }

    return streams;
  }

  HRESULT GetPMPServer(REFIID riid, LPVOID* object_result) {
    ComPtr<IMediaProtectionPMPServer> pmp_server;
    if (riid != __uuidof(**(&pmp_server)) || !object_result) {
      return E_INVALIDARG;
    }

    return pmp_server_.CopyTo(
        reinterpret_cast<IMediaProtectionPMPServer**>(object_result));
  }

 protected:
  // IMF* interfaces (e.g. IMediaProtectionPMPServer or
  // IMFContentDecryptionModule) may require an MTA to run successfully.
  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::kMTA};

  base::test::TaskEnvironment task_environment_;
  base::MockOnceCallback<void(bool)> set_cdm_cb_;
  base::MockOnceCallback<void(PipelineStatus)> renderer_init_cb_;
  NiceMock<MockCdmContext> cdm_context_;
  NiceMock<MockMediaResource> media_resource_;
  NiceMock<MockRendererClient> renderer_client_;
  scoped_refptr<NiceMock<MockMediaFoundationCdmProxy>> mf_cdm_proxy_;
  ComPtr<IMediaProtectionPMPServer> pmp_server_;
  std::unique_ptr<MediaFoundationRenderer> mf_renderer_;
  std::vector<std::unique_ptr<StrictMock<MockDemuxerStream>>> streams_;
};

TEST_F(MediaFoundationRendererTest, VerifyInitWithoutSetCdm) {
  AddStream(DemuxerStream::AUDIO, /*encrypted=*/false);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));

  mf_renderer_->Initialize(&media_resource_, &renderer_client_,
                           renderer_init_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationRendererTest, SetCdmThenInit) {
  AddStream(DemuxerStream::AUDIO, /*encrypted=*/true);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  EXPECT_CALL(set_cdm_cb_, Run(true));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));

  mf_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  mf_renderer_->Initialize(&media_resource_, &renderer_client_,
                           renderer_init_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationRendererTest, InitThenSetCdm) {
  AddStream(DemuxerStream::AUDIO, /*encrypted=*/true);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  EXPECT_CALL(set_cdm_cb_, Run(true));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));

  mf_renderer_->Initialize(&media_resource_, &renderer_client_,
                           renderer_init_cb_.Get());
  mf_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationRendererTest, DirectCompositionHandle) {
  base::MockCallback<MediaFoundationRendererExtension::GetDCompSurfaceCB>
      get_dcomp_surface_cb;

  AddStream(DemuxerStream::AUDIO, /*encrypted=*/true);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  EXPECT_CALL(set_cdm_cb_, Run(true));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  // Ignore the DirectComposition handle value returned as our |pmp_server_|
  // has no real implementation.
  EXPECT_CALL(get_dcomp_surface_cb, Run(_, _));

  mf_renderer_->Initialize(&media_resource_, &renderer_client_,
                           renderer_init_cb_.Get());
  mf_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  mf_renderer_->GetDCompSurface(get_dcomp_surface_cb.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationRendererTest, ClearStartsInFrameServer) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      media::kMediaFoundationClearRendering, {{"strategy", "dynamic"}});

  AddStream(DemuxerStream::AUDIO, /*encrypted=*/false);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/false);

  mf_renderer_->Initialize(&media_resource_, &renderer_client_,
                           renderer_init_cb_.Get());

  EXPECT_TRUE(mf_renderer_->InFrameServerMode());
}

TEST_F(MediaFoundationRendererTest, EncryptedStaysInDirectComposition) {
  AddStream(DemuxerStream::AUDIO, /*encrypted=*/true);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  mf_renderer_->Initialize(&media_resource_, &renderer_client_,
                           renderer_init_cb_.Get());

  EXPECT_FALSE(mf_renderer_->InFrameServerMode());
}

}  // namespace media

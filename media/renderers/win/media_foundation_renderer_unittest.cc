// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_renderer.h"

#include <windows.media.protection.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "media/base/demuxer_stream.h"
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

class MockMFCdmProxy
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFCdmProxy> {
 public:
  MockMFCdmProxy();
  ~MockMFCdmProxy() override;

  // IMFCdmProxy.
  MOCK_STDCALL_METHOD2(GetPMPServer,
                       HRESULT(REFIID riid, void** object_result));
  MOCK_STDCALL_METHOD6(GetInputTrustAuthority,
                       HRESULT(uint32_t stream_id,
                               uint32_t stream_count,
                               const uint8_t* content_init_data,
                               uint32_t content_init_data_size,
                               REFIID riid,
                               IUnknown** object_result));
  MOCK_STDCALL_METHOD2(SetLastKeyId,
                       HRESULT(uint32_t stream_id, REFGUID key_id));
  MOCK_STDCALL_METHOD0(RefreshTrustedInput, HRESULT());
  MOCK_STDCALL_METHOD2(ProcessContentEnabler,
                       HRESULT(IUnknown* request, IMFAsyncResult* result));
};

MockMFCdmProxy::MockMFCdmProxy() = default;
MockMFCdmProxy::~MockMFCdmProxy() = default;

class MockMediaProtectionPMPServer
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          IMediaProtectionPMPServer> {
 public:
  MockMediaProtectionPMPServer() = default;
  virtual ~MockMediaProtectionPMPServer() = default;

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
    if (!MediaFoundationRenderer::IsSupported())
      return;

    MockMediaProtectionPMPServer::MakeMockMediaProtectionPMPServer(
        &pmp_server_);

    mf_renderer_ = std::make_unique<MediaFoundationRenderer>(
        /*muted=*/false, task_environment_.GetMainThreadTaskRunner());

    // Some default actions.
    ON_CALL(cdm_context_, GetMediaFoundationCdmProxy(_))
        .WillByDefault(
            Invoke(this, &MediaFoundationRendererTest::MockGetMFCdm));
    ON_CALL(mf_cdm_proxy_, GetPMPServer(_, _))
        .WillByDefault(
            Invoke(this, &MediaFoundationRendererTest::MockGetPMPServer));

    // Some expected calls with return values.
    EXPECT_CALL(media_resource_, GetAllStreams())
        .WillRepeatedly(
            Invoke(this, &MediaFoundationRendererTest::GetAllStreams));
    EXPECT_CALL(media_resource_, GetType())
        .WillRepeatedly(Return(MediaResource::STREAM));
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

  void OnSendCdmProxy(
      CdmContext::GetMediaFoundationCdmProxyCB get_mf_cdm_proxy_cb) {
    std::move(get_mf_cdm_proxy_cb).Run(&mf_cdm_proxy_);
  }

  bool MockGetMFCdm(
      CdmContext::GetMediaFoundationCdmProxyCB get_mf_cdm_proxy_cb) {
    // The callback should be invoked asynchronously per API contract. Post
    // to make callback from OnSendCdmProxy().
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaFoundationRendererTest::OnSendCdmProxy,
                       base::Unretained(this), std::move(get_mf_cdm_proxy_cb)));
    return true;
  }

  HRESULT MockGetPMPServer(REFIID riid, LPVOID* object_result) {
    ComPtr<IMediaProtectionPMPServer> pmp_server;
    if (riid != __uuidof(**(&pmp_server)) || !object_result) {
      return E_INVALIDARG;
    }

    return pmp_server_.CopyTo(
        reinterpret_cast<IMediaProtectionPMPServer**>(object_result));
  }

 protected:
  base::win::ScopedCOMInitializer com_initializer_;
  base::test::TaskEnvironment task_environment_;
  base::MockOnceCallback<void(bool)> set_cdm_cb_;
  base::MockOnceCallback<void(PipelineStatus)> renderer_init_cb_;
  NiceMock<MockCdmContext> cdm_context_;
  NiceMock<MockMediaResource> media_resource_;
  NiceMock<MockRendererClient> renderer_client_;
  NiceMock<MockMFCdmProxy> mf_cdm_proxy_;
  ComPtr<IMediaProtectionPMPServer> pmp_server_;
  std::unique_ptr<MediaFoundationRenderer> mf_renderer_;
  std::vector<std::unique_ptr<StrictMock<MockDemuxerStream>>> streams_;
};

TEST_F(MediaFoundationRendererTest, VerifyInitWithoutSetCdm) {
  if (!MediaFoundationRenderer::IsSupported())
    return;

  AddStream(DemuxerStream::AUDIO, /*encrypted=*/false);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  EXPECT_CALL(renderer_init_cb_, Run(PIPELINE_OK));

  mf_renderer_->Initialize(&media_resource_, &renderer_client_,
                           renderer_init_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationRendererTest, SetCdmThenInit) {
  if (!MediaFoundationRenderer::IsSupported())
    return;

  AddStream(DemuxerStream::AUDIO, /*encrypted=*/true);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  EXPECT_CALL(set_cdm_cb_, Run(true));
  EXPECT_CALL(renderer_init_cb_, Run(PIPELINE_OK));

  mf_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  mf_renderer_->Initialize(&media_resource_, &renderer_client_,
                           renderer_init_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationRendererTest, InitThenSetCdm) {
  if (!MediaFoundationRenderer::IsSupported())
    return;

  AddStream(DemuxerStream::AUDIO, /*encrypted=*/true);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  EXPECT_CALL(set_cdm_cb_, Run(true));
  EXPECT_CALL(renderer_init_cb_, Run(PIPELINE_OK));

  mf_renderer_->Initialize(&media_resource_, &renderer_client_,
                           renderer_init_cb_.Get());
  mf_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(MediaFoundationRendererTest, DirectCompositionHandle) {
  if (!MediaFoundationRenderer::IsSupported())
    return;

  base::MockCallback<MediaFoundationRendererExtension::SetDCompModeCB>
      set_dcomp_mode_cb;
  base::MockCallback<MediaFoundationRendererExtension::GetDCompSurfaceCB>
      get_dcomp_cb;

  AddStream(DemuxerStream::AUDIO, /*encrypted=*/true);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  EXPECT_CALL(set_cdm_cb_, Run(true));
  EXPECT_CALL(renderer_init_cb_, Run(PIPELINE_OK));
  EXPECT_CALL(set_dcomp_mode_cb, Run(true));
  // Ignore the DirectComposition handle value returned as our |pmp_server_|
  // has no real implementation.
  EXPECT_CALL(get_dcomp_cb, Run(_));

  mf_renderer_->Initialize(&media_resource_, &renderer_client_,
                           renderer_init_cb_.Get());
  mf_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  mf_renderer_->SetDCompMode(true, set_dcomp_mode_cb.Get());
  mf_renderer_->GetDCompSurface(get_dcomp_cb.Get());

  task_environment_.RunUntilIdle();
}

}  // namespace media
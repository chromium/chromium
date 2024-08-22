// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for AudioOutputAuthorizationHandler.

#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "content/browser/media/media_devices_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/common/content_client.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/media_switches.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::MediaDeviceType;
using ::testing::_;

namespace content {

namespace {

const char kSecurityOriginString[] = "http://localhost";
const char kDefaultDeviceId[] = "default";
const char kEmptyDeviceId[] = "";
const char kInvalidDeviceId[] = "invalid-device-id";

using MockAuthorizationCallback = base::MockCallback<
    AudioOutputAuthorizationHandler::AuthorizationCompletedCallback>;

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() : salt_(CreateRandomMediaDeviceIDSalt()) {}
  ~TestBrowserClient() override = default;

  void GetMediaDeviceIDSalt(
      content::RenderFrameHost* rfh,
      const net::SiteForCookies& site_for_cookies,
      const blink::StorageKey& storage_key,
      base::OnceCallback<void(bool, const std::string&)> callback) override {
    std::move(callback).Run(true, salt_);
  }

  const std::string& media_device_id_salt() const { return salt_; }

  void set_media_device_id_salt(std::string salt) { salt_ = std::move(salt); }

 private:
  std::string salt_;
};

}  //  namespace

class AudioOutputAuthorizationHandlerTest : public RenderViewHostTestHarness {
 public:
  AudioOutputAuthorizationHandlerTest()
      : RenderViewHostTestHarness(
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {
    // Not threadsafe, thus set before threads are started:
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
  }

  AudioOutputAuthorizationHandlerTest(
      const AudioOutputAuthorizationHandlerTest&) = delete;
  AudioOutputAuthorizationHandlerTest& operator=(
      const AudioOutputAuthorizationHandlerTest&) = delete;

  ~AudioOutputAuthorizationHandlerTest() override {
    audio_manager_->Shutdown();
  }

  void SetUp() override {
    // Starts thread bundle:
    RenderViewHostTestHarness::SetUp();
    SetBrowserClientForTesting(&test_browser_client_);
    audio_manager_ = std::make_unique<media::FakeAudioManager>(
        std::make_unique<media::TestAudioThread>(true), &log_factory_);
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    media_stream_manager_ =
        std::make_unique<MediaStreamManager>(audio_system_.get());

    // Make sure everything is done initializing:
    SyncWithAllThreads();
    NavigateAndCommit(GURL(kSecurityOriginString));
  }

  void TearDown() override {
    SyncWithAllThreads();
    // All the audio/media things are destructed after the IO thread has been
    // joined.
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  MediaStreamManager* GetMediaStreamManager() {
    return media_stream_manager_.get();
  }

  media::AudioSystem* GetAudioSystem() { return audio_system_.get(); }

  void SyncWithAllThreads() {
    // New tasks might be posted while we are syncing, but in
    // every iteration at least one task will be run. 20 iterations should be
    // enough for our code.
    for (int i = 0; i < 20; ++i) {
      base::RunLoop().RunUntilIdle();
      SyncWith(GetIOThreadTaskRunner({}));
      SyncWith(audio_manager_->GetWorkerTaskRunner());
    }
  }

  std::string GetRawNondefaultId() {
    std::string id;
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AudioOutputAuthorizationHandlerTest::GetRawNondefaultIdOnIOThread,
            base::Unretained(this), base::Unretained(&id)));
    SyncWithAllThreads();
    return id;
  }

  TestBrowserClient& test_browser_client() { return test_browser_client_; }

 private:
  void SyncWith(scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    CHECK(task_runner);
    CHECK(!task_runner->BelongsToCurrentThread());
    base::WaitableEvent e = {base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED};
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&e)));
    e.Wait();
  }

  void GetRawNondefaultIdOnIOThread(std::string* out) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[static_cast<size_t>(
        MediaDeviceType::kMediaAudioOutput)] = true;

    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(
            [](std::string* out, const MediaDeviceEnumeration& result) {
              // Index 0 is default, so use 1.
              CHECK(
                  result[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)]
                      .size() > 1)
                  << "Expected to have a nondefault device.";
              *out =
                  result[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)]
                        [1]
                            .device_id;
            },
            base::Unretained(out)));
  }

  media::FakeAudioLogFactory log_factory_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  TestBrowserClient test_browser_client_;
};

TEST_F(AudioOutputAuthorizationHandlerTest, DoNothing) {}

TEST_F(AudioOutputAuthorizationHandlerTest, AuthorizeDefaultDevice_Ok) {
  MockAuthorizationCallback listener;
  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_OK, _, kDefaultDeviceId,
                            std::string()))
      .Times(1);
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(),
          base::UnguessableToken(), kDefaultDeviceId, listener.Get()));

  SyncWithAllThreads();
  GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeDefaultDeviceByEmptyId_Ok) {
  MockAuthorizationCallback listener;
  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_OK, _, kDefaultDeviceId,
                            std::string()))
      .Times(1);
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(),
          base::UnguessableToken(), kEmptyDeviceId, listener.Get()));

  SyncWithAllThreads();
  GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeNondefaultDeviceIdWithoutPermission_NotAuthorized) {
  std::string raw_nondefault_id = GetRawNondefaultId();
  base::test::TestFuture<const MediaDeviceSaltAndOrigin&> future;
  GetMediaDeviceSaltAndOrigin(main_rfh()->GetGlobalId(), future.GetCallback());
  MediaDeviceSaltAndOrigin salt_and_origin = future.Get();
  std::string hashed_id =
      GetHMACForRawMediaDeviceID(salt_and_origin, raw_nondefault_id);

  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::OverridePermissionsForTesting,
          base::Unretained(handler.get()), false));
  SyncWithAllThreads();

  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED, _,
                            std::string(), std::string()))
      .Times(1);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(),
          base::UnguessableToken(), hashed_id, listener.Get()));

  SyncWithAllThreads();
  GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeNondefaultDeviceIdWithPermission_Ok) {
  std::string raw_nondefault_id = GetRawNondefaultId();
  base::test::TestFuture<const MediaDeviceSaltAndOrigin&> future;
  GetMediaDeviceSaltAndOrigin(main_rfh()->GetGlobalId(), future.GetCallback());
  MediaDeviceSaltAndOrigin salt_and_origin = future.Get();
  std::string hashed_id =
      GetHMACForRawMediaDeviceID(salt_and_origin, raw_nondefault_id);
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::OverridePermissionsForTesting,
          base::Unretained(handler.get()), true));

  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_OK, _,
                            raw_nondefault_id, std::string()))
      .Times(1);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(),
          base::UnguessableToken(), hashed_id, listener.Get()));

  SyncWithAllThreads();
  GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest, AuthorizeInvalidDeviceId_NotFound) {
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());

  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND, _,
                            std::string(), std::string()))
      .Times(1);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(),
          base::UnguessableToken(), kInvalidDeviceId, listener.Get()));

  SyncWithAllThreads();
  // It is possible to request an invalid device id from JS APIs,
  // so we don't want to crash the renderer for this.
  EXPECT_EQ(process()->bad_msg_count(), 0);
  GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeNondefaultDeviceIdWithBadOrigin_NotAuthorized) {
  // We use about:blank here as it will definitely fail the permissions check.
  // Note that other urls may also fail the permissions check, e.g. when a
  // navigation is done during stream creation.
  GURL url("about:blank");
  MediaDeviceSaltAndOrigin salt_and_origin(
      test_browser_client().media_device_id_salt(), url::Origin::Create(url));
  std::string raw_nondefault_id = GetRawNondefaultId();
  std::string hashed_id =
      GetHMACForRawMediaDeviceID(salt_and_origin, raw_nondefault_id);
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());
  NavigateAndCommit(url);

  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED, _,
                            std::string(), std::string()))
      .Times(1);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(),
          base::UnguessableToken(), hashed_id, listener.Get()));
  SyncWithAllThreads();

  EXPECT_EQ(process()->bad_msg_count(), 0);
  GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeWithSessionIdWithoutDevice_GivesDefault) {
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());

  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_OK, _, kDefaultDeviceId,
                            std::string()))
      .Times(1);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(),
          base::UnguessableToken::Create(), std::string(), listener.Get()));

  SyncWithAllThreads();
  GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeNondefaultDeviceIdAfterSaltChange_NotFound) {
  std::string raw_nondefault_id = GetRawNondefaultId();
  base::test::TestFuture<const MediaDeviceSaltAndOrigin&> future;
  GetMediaDeviceSaltAndOrigin(main_rfh()->GetGlobalId(), future.GetCallback());
  MediaDeviceSaltAndOrigin salt_and_origin = future.Get();
  std::string hashed_id =
      GetHMACForRawMediaDeviceID(salt_and_origin, raw_nondefault_id);
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::OverridePermissionsForTesting,
          base::Unretained(handler.get()), true));

  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_OK, _,
                            raw_nondefault_id, std::string()));
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(),
          base::UnguessableToken(), hashed_id, listener.Get()));
  SyncWithAllThreads();

  // Reset the salt and expect authorization of the device ID hashed with
  // the old salt to fail.
  test_browser_client().set_media_device_id_salt("new salt");
  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND, _, _,
                            std::string()));
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(),
          base::UnguessableToken(), hashed_id, listener.Get()));

  SyncWithAllThreads();
  GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, handler.release());
  SyncWithAllThreads();
}

}  // namespace content

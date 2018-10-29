// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for AudioOutputAuthorizationHandler.

#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/mock_callback.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/base/media_switches.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;

namespace content {

namespace {

const int kSessionId = 1618;
const char kSecurityOriginString[] = "http://localhost";
const char kDefaultDeviceId[] = "default";
const char kEmptyDeviceId[] = "";
const char kInvalidDeviceId[] = "invalid-device-id";

using MockAuthorizationCallback = base::MockCallback<
    AudioOutputAuthorizationHandler::AuthorizationCompletedCallback>;

url::Origin SecurityOrigin() {
  return url::Origin::Create(GURL(kSecurityOriginString));
}

// TestBrowserContext has a URLRequestContextGetter which uses a NullTaskRunner.
// This causes it to be destroyed on the wrong thread. This BrowserContext
// instead uses the IO thread task runner for the URLRequestContextGetter.
class TestBrowserContextWithRealURLRequestContextGetter
    : public TestBrowserContext {
 public:
  TestBrowserContextWithRealURLRequestContextGetter() {
    request_context_ = base::MakeRefCounted<net::TestURLRequestContextGetter>(
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
    salt_ = TestBrowserContext::GetMediaDeviceIDSalt();
  }

  ~TestBrowserContextWithRealURLRequestContextGetter() override {}

  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors) override {
    return request_context_.get();
  }

  std::string GetMediaDeviceIDSalt() override { return salt_; }

  void set_media_device_id_salt(std::string salt) { salt_ = std::move(salt); }

 private:
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  std::string salt_;
};

}  //  namespace

class AudioOutputAuthorizationHandlerTest : public RenderViewHostTestHarness {
 public:
  AudioOutputAuthorizationHandlerTest()
      : RenderViewHostTestHarness(
            content::TestBrowserThreadBundle::REAL_IO_THREAD) {
    // Not threadsafe, thus set before threads are started:
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
  }

  ~AudioOutputAuthorizationHandlerTest() override {
    audio_manager_->Shutdown();
  }

  BrowserContext* CreateBrowserContext() override {
    // Caller takes ownership.
    return new TestBrowserContextWithRealURLRequestContextGetter();
  }

  void SetUp() override {
    // Starts thread bundle:
    RenderViewHostTestHarness::SetUp();

    audio_manager_ = std::make_unique<media::FakeAudioManager>(
        std::make_unique<media::AudioThreadImpl>(), &log_factory_);
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), audio_manager_->GetTaskRunner());

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
      SyncWith(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
      SyncWith(audio_manager_->GetWorkerTaskRunner());
    }
  }

  std::string GetRawNondefaultId() {
    std::string id;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &AudioOutputAuthorizationHandlerTest::GetRawNondefaultIdOnIOThread,
            base::Unretained(this), base::Unretained(&id)));
    SyncWithAllThreads();
    return id;
  }

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
    devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;

    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(
            [](std::string* out, const MediaDeviceEnumeration& result) {
              // Index 0 is default, so use 1.
              CHECK(result[MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT]
                        .size() > 1)
                  << "Expected to have a nondefault device.";
              *out = result[MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT][1]
                         .device_id;
            },
            base::Unretained(out)));
  }

  media::FakeAudioLogFactory log_factory_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputAuthorizationHandlerTest);
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

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(), 0,
          kDefaultDeviceId, listener.Get()));

  SyncWithAllThreads();
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
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

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(), 0,
          kEmptyDeviceId, listener.Get()));

  SyncWithAllThreads();
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeNondefaultDeviceIdWithoutPermission_NotAuthorized) {
  std::string raw_nondefault_id = GetRawNondefaultId();
  std::string hashed_id = MediaStreamManager::GetHMACForMediaDeviceID(
      browser_context()->GetMediaDeviceIDSalt(), SecurityOrigin(),
      raw_nondefault_id);

  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::OverridePermissionsForTesting,
          base::Unretained(handler.get()), false));
  SyncWithAllThreads();

  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED, _,
                            std::string(), std::string()))
      .Times(1);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(), 0,
          hashed_id, listener.Get()));

  SyncWithAllThreads();
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeNondefaultDeviceIdWithPermission_Ok) {
  std::string raw_nondefault_id = GetRawNondefaultId();
  std::string hashed_id = MediaStreamManager::GetHMACForMediaDeviceID(
      browser_context()->GetMediaDeviceIDSalt(), SecurityOrigin(),
      raw_nondefault_id);
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::OverridePermissionsForTesting,
          base::Unretained(handler.get()), true));

  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_OK, _,
                            raw_nondefault_id, std::string()))
      .Times(1);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(), 0,
          hashed_id, listener.Get()));

  SyncWithAllThreads();
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
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

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(), 0,
          kInvalidDeviceId, listener.Get()));

  SyncWithAllThreads();
  // It is possible to request an invalid device id from JS APIs,
  // so we don't want to crash the renderer for this.
  EXPECT_EQ(process()->bad_msg_count(), 0);
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeNondefaultDeviceIdWithBadOrigin_NotAuthorized) {
  // We use about:blank here as it will definitely fail the permissions check.
  // Note that other urls may also fail the permissions check, e.g. when a
  // navigation is done during stream creation.
  GURL url("about:blank");
  url::Origin origin = url::Origin::Create(url);
  std::string raw_nondefault_id = GetRawNondefaultId();
  std::string hashed_id = MediaStreamManager::GetHMACForMediaDeviceID(
      browser_context()->GetMediaDeviceIDSalt(), origin, raw_nondefault_id);
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());
  NavigateAndCommit(url);

  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED, _,
                            std::string(), std::string()))
      .Times(1);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(), 0,
          hashed_id, listener.Get()));
  SyncWithAllThreads();

  EXPECT_EQ(process()->bad_msg_count(), 0);
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
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

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(),
          kSessionId, std::string(), listener.Get()));

  SyncWithAllThreads();
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
}

TEST_F(AudioOutputAuthorizationHandlerTest,
       AuthorizeNondefaultDeviceIdAfterSaltChange_NotFound) {
  std::string raw_nondefault_id = GetRawNondefaultId();
  std::string hashed_id = MediaStreamManager::GetHMACForMediaDeviceID(
      browser_context()->GetMediaDeviceIDSalt(), SecurityOrigin(),
      raw_nondefault_id);
  MockAuthorizationCallback listener;
  std::unique_ptr<AudioOutputAuthorizationHandler> handler =
      std::make_unique<AudioOutputAuthorizationHandler>(
          GetAudioSystem(), GetMediaStreamManager(), process()->GetID());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::OverridePermissionsForTesting,
          base::Unretained(handler.get()), true));

  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_OK, _,
                            raw_nondefault_id, std::string()));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(), 0,
          hashed_id, listener.Get()));
  SyncWithAllThreads();

  // Reset the salt and expect authorization of the device ID hashed with
  // the old salt to fail.
  auto* context =
      static_cast<TestBrowserContextWithRealURLRequestContextGetter*>(
          browser_context());
  context->set_media_device_id_salt("new salt");
  EXPECT_CALL(listener, Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND, _, _,
                            std::string()));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AudioOutputAuthorizationHandler::RequestDeviceAuthorization,
          base::Unretained(handler.get()), main_rfh()->GetRoutingID(), 0,
          hashed_id, listener.Get()));

  SyncWithAllThreads();
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, handler.release());
  SyncWithAllThreads();
}

}  // namespace content

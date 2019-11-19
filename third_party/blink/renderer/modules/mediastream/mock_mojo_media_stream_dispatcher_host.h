// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_MOJO_MEDIA_STREAM_DISPATCHER_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_MOJO_MEDIA_STREAM_DISPATCHER_HOST_H_

#include <string>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/mediastream/media_stream_controls.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"

namespace blink {

class MockMojoMediaStreamDispatcherHost
    : public mojom::blink::MediaStreamDispatcherHost {
 public:
  MockMojoMediaStreamDispatcherHost();
  ~MockMojoMediaStreamDispatcherHost() override;

  mojo::PendingRemote<mojom::blink::MediaStreamDispatcherHost>
  CreatePendingRemoteAndBind();

  void GenerateStream(
      int32_t request_id,
      const StreamControls& controls,
      bool user_gesture,
      mojom::blink::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      GenerateStreamCallback callback) override;
  void CancelRequest(int32_t request_id) override;
  void StopStreamDevice(
      const WTF::String& device_id,
      const base::Optional<base::UnguessableToken>& session_id) override;
  void OpenDevice(int32_t request_id,
                  const WTF::String& device_id,
                  mojom::blink::MediaStreamType type,
                  OpenDeviceCallback callback) override;

  MOCK_METHOD1(CloseDevice, void(const WTF::String&));
  MOCK_METHOD3(SetCapturingLinkSecured,
               void(const base::Optional<base::UnguessableToken>&,
                    mojom::blink::MediaStreamType,
                    bool));
  MOCK_METHOD1(OnStreamStarted, void(const WTF::String&));

  void ResetSessionId() { session_id_ = base::UnguessableToken::Create(); }
  void DoNotRunCallback() { do_not_run_cb_ = true; }
  const base::UnguessableToken& session_id() { return session_id_; }

  int request_stream_counter() const { return request_stream_counter_; }
  int stop_audio_device_counter() const { return stop_audio_device_counter_; }
  int stop_video_device_counter() const { return stop_video_device_counter_; }

  const WTF::Vector<MediaStreamDevice>& audio_devices() const {
    return audio_devices_;
  }
  const WTF::Vector<MediaStreamDevice>& video_devices() const {
    return video_devices_;
  }

 private:
  int request_id_ = -1;
  int request_stream_counter_ = 0;
  int stop_audio_device_counter_ = 0;
  int stop_video_device_counter_ = 0;
  base::UnguessableToken session_id_ = base::UnguessableToken::Create();
  bool do_not_run_cb_ = false;
  WTF::Vector<MediaStreamDevice> audio_devices_;
  WTF::Vector<MediaStreamDevice> video_devices_;
  GenerateStreamCallback generate_stream_cb_;
  mojo::Receiver<mojom::blink::MediaStreamDispatcherHost> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(MockMojoMediaStreamDispatcherHost);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_MOJO_MEDIA_STREAM_DISPATCHER_HOST_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_MOCK_LOG_H_
#define SERVICES_AUDIO_TEST_MOCK_LOG_H_

#include <string>

#include "base/functional/bind.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace audio {

class MockLog : public media::mojom::AudioLog {
 public:
  MockLog();

  MockLog(const MockLog&) = delete;
  MockLog& operator=(const MockLog&) = delete;

  ~MockLog() override;

  // Should only be called once.
  mojo::PendingRemote<media::mojom::AudioLog> MakeRemote() {
    mojo::PendingRemote<media::mojom::AudioLog> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockLog::BindingConnectionError, base::Unretained(this)));
    return remote;
  }

  void CloseBinding() { receiver_.reset(); }

  MOCK_METHOD2(OnCreated,
               void(const media::AudioParameters& params,
                    const std::string& device_id));
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnClosed, void());
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnSetVolume, void(double));
  MOCK_METHOD1(OnProcessingStateChanged, void(const std::string&));
  MOCK_METHOD1(OnLogMessage, void(const std::string&));

  MOCK_METHOD0(BindingConnectionError, void());

 private:
  mojo::Receiver<media::mojom::AudioLog> receiver_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_MOCK_LOG_H_

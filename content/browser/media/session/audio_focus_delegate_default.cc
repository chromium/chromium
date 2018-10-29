// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_delegate.h"

#include "content/browser/media/session/media_session_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/media_session/public/cpp/switches.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

using media_session::mojom::AudioFocusType;

namespace {

const char kAudioFocusSourceName[] = "web";

// AudioFocusDelegateDefault is the default implementation of
// AudioFocusDelegate which only handles audio focus between WebContents.
class AudioFocusDelegateDefault : public AudioFocusDelegate {
 public:
  explicit AudioFocusDelegateDefault(MediaSessionImpl* media_session);
  ~AudioFocusDelegateDefault() override;

  // AudioFocusDelegate implementation.
  AudioFocusResult RequestAudioFocus(AudioFocusType audio_focus_type) override;
  void AbandonAudioFocus() override;
  base::Optional<media_session::mojom::AudioFocusType> GetCurrentFocusType()
      const override;
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr) override;

 private:
  // Finishes an async audio focus request.
  void FinishAudioFocusRequest(AudioFocusType type);

  // Ensures that |audio_focus_ptr_| is connected.
  void EnsureServiceConnection();

  // Holds the latest MediaSessionInfo for |media_session_|.
  media_session::mojom::MediaSessionInfoPtr session_info_;

  // Holds a pointer to the Media Session service.
  media_session::mojom::AudioFocusManagerPtr audio_focus_ptr_;

  // If the media session has acquired audio focus then this will contain a
  // pointer to that requests AudioFocusRequestClient.
  media_session::mojom::AudioFocusRequestClientPtr request_client_ptr_;

  // Weak pointer because |this| is owned by |media_session_|.
  MediaSessionImpl* media_session_;

  // The last requested AudioFocusType by the associated |media_session_|.
  base::Optional<AudioFocusType> audio_focus_type_;
};

}  // anonymous namespace

AudioFocusDelegateDefault::AudioFocusDelegateDefault(
    MediaSessionImpl* media_session)
    : media_session_(media_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

AudioFocusDelegateDefault::~AudioFocusDelegateDefault() = default;

AudioFocusDelegate::AudioFocusResult
AudioFocusDelegateDefault::RequestAudioFocus(AudioFocusType audio_focus_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!media_session::IsAudioFocusEnabled()) {
    audio_focus_type_ = audio_focus_type;
    return AudioFocusDelegate::AudioFocusResult::kSuccess;
  }

  if (request_client_ptr_.is_bound()) {
    // We have an existing request so we should request an updated focus type.
    request_client_ptr_->RequestAudioFocus(
        session_info_.Clone(), audio_focus_type,
        base::BindOnce(&AudioFocusDelegateDefault::FinishAudioFocusRequest,
                       base::Unretained(this), audio_focus_type));
  } else {
    EnsureServiceConnection();

    // Create a mojo interface pointer to our media session.
    media_session::mojom::MediaSessionPtr media_session;
    media_session_->BindToMojoRequest(mojo::MakeRequest(&media_session));

    audio_focus_ptr_->RequestAudioFocus(
        mojo::MakeRequest(&request_client_ptr_), std::move(media_session),
        session_info_.Clone(), audio_focus_type,
        base::BindOnce(&AudioFocusDelegateDefault::FinishAudioFocusRequest,
                       base::Unretained(this), audio_focus_type));
  }

  // Return delayed as we make the async call to request audio focus.
  return AudioFocusDelegate::AudioFocusResult::kDelayed;
}

void AudioFocusDelegateDefault::AbandonAudioFocus() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  audio_focus_type_.reset();

  if (!request_client_ptr_.is_bound())
    return;

  request_client_ptr_->AbandonAudioFocus();
  request_client_ptr_.reset();
  audio_focus_ptr_.reset();
}

base::Optional<media_session::mojom::AudioFocusType>
AudioFocusDelegateDefault::GetCurrentFocusType() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return audio_focus_type_;
}

void AudioFocusDelegateDefault::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (request_client_ptr_.is_bound())
    request_client_ptr_->MediaSessionInfoChanged(session_info.Clone());

  session_info_ = std::move(session_info);
}

void AudioFocusDelegateDefault::FinishAudioFocusRequest(AudioFocusType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(request_client_ptr_.is_bound());

  audio_focus_type_ = type;
  media_session_->FinishSystemAudioFocusRequest(type, true /* result */);
}

void AudioFocusDelegateDefault::EnsureServiceConnection() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!media_session::IsAudioFocusEnabled())
    return;

  if (audio_focus_ptr_.is_bound() && !audio_focus_ptr_.encountered_error())
    return;

  audio_focus_ptr_.reset();

  // Connect to the Media Session service and bind |audio_focus_ptr_| to it.
  service_manager::Connector* connector =
      ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(media_session::mojom::kServiceName,
                           mojo::MakeRequest(&audio_focus_ptr_));

  audio_focus_ptr_->SetSourceName(kAudioFocusSourceName);
}

// static
std::unique_ptr<AudioFocusDelegate> AudioFocusDelegate::Create(
    MediaSessionImpl* media_session) {
  return std::unique_ptr<AudioFocusDelegate>(
      new AudioFocusDelegateDefault(media_session));
}

}  // namespace content

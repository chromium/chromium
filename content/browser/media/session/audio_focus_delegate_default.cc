// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_delegate.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

using media_session::mojom::AudioFocusType;

namespace {

const char kAudioFocusSourceName[] = "web";

base::UnguessableToken GetAudioFocusGroupId(MediaSessionImpl* session) {
  // Allow the media session to override the group id.
  if (session->audio_focus_group_id() != base::UnguessableToken::Null())
    return session->audio_focus_group_id();

  // Use a shared audio focus group id for the whole browser. This will means
  // that tabs will share audio focus if the enforcement mode is set to
  // kSingleGroup.
  static const base::NoDestructor<base::UnguessableToken> token(
      base::UnguessableToken::Create());
  return *token;
}

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
  const base::UnguessableToken& request_id() const override {
    return request_id_;
  }

 private:
  // Finishes an async audio focus request.
  void FinishAudioFocusRequest(AudioFocusType type, bool success);

  // Ensures that |audio_focus_| is connected.
  void EnsureServiceConnection();

  // Holds the latest MediaSessionInfo for |media_session_|.
  media_session::mojom::MediaSessionInfoPtr session_info_;

  // Holds a remote to the Media Session service.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_;

  // If the media session has acquired audio focus then this will contain a
  // pointer to that requests AudioFocusRequestClient.
  mojo::Remote<media_session::mojom::AudioFocusRequestClient>
      request_client_remote_;

  // Weak pointer because |this| is owned by |media_session_|.
  MediaSessionImpl* media_session_;

  // The last requested AudioFocusType by the associated |media_session_|.
  base::Optional<AudioFocusType> audio_focus_type_;

  // ID to uniquely identify the audio focus delegate.
  base::UnguessableToken const request_id_ = base::UnguessableToken::Create();
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

  if (!base::FeatureList::IsEnabled(
          media_session::features::kMediaSessionService)) {
    audio_focus_type_ = audio_focus_type;
    return AudioFocusDelegate::AudioFocusResult::kSuccess;
  }

  if (request_client_remote_.is_bound()) {
    // We have an existing request so we should request an updated focus type.
    request_client_remote_->RequestAudioFocus(
        session_info_.Clone(), audio_focus_type,
        base::BindOnce(&AudioFocusDelegateDefault::FinishAudioFocusRequest,
                       base::Unretained(this), audio_focus_type,
                       true /* success */));
  } else {
    EnsureServiceConnection();

    // Create a mojo interface pointer to our media session.
    mojo::PendingRemote<media_session::mojom::MediaSession> media_session =
        media_session_->AddRemote();

    audio_focus_->RequestGroupedAudioFocus(
        request_id_, request_client_remote_.BindNewPipeAndPassReceiver(),
        std::move(media_session), session_info_.Clone(), audio_focus_type,
        GetAudioFocusGroupId(media_session_),
        base::BindOnce(&AudioFocusDelegateDefault::FinishAudioFocusRequest,
                       base::Unretained(this), audio_focus_type));
  }

  // Return delayed as we make the async call to request audio focus.
  return AudioFocusDelegate::AudioFocusResult::kDelayed;
}

void AudioFocusDelegateDefault::AbandonAudioFocus() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  audio_focus_type_.reset();

  if (!request_client_remote_.is_bound())
    return;

  request_client_remote_->AbandonAudioFocus();
  request_client_remote_.reset();
  audio_focus_.reset();
}

base::Optional<media_session::mojom::AudioFocusType>
AudioFocusDelegateDefault::GetCurrentFocusType() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return audio_focus_type_;
}

void AudioFocusDelegateDefault::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (request_client_remote_.is_bound())
    request_client_remote_->MediaSessionInfoChanged(session_info.Clone());

  session_info_ = std::move(session_info);
}

void AudioFocusDelegateDefault::FinishAudioFocusRequest(AudioFocusType type,
                                                        bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(request_client_remote_.is_bound());

  audio_focus_type_ = type;
  media_session_->FinishSystemAudioFocusRequest(type, success);
}

void AudioFocusDelegateDefault::EnsureServiceConnection() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(
          media_session::features::kMediaSessionService)) {
    return;
  }

  if (audio_focus_.is_bound() && audio_focus_.is_connected())
    return;

  audio_focus_.reset();

  // Connect to the Media Session service and bind |audio_focus_| to it.
  GetSystemConnector()->Connect(media_session::mojom::kServiceName,
                                audio_focus_.BindNewPipeAndPassReceiver());

  // We associate all media sessions with the browser context so we can filter
  // by browser context in the UI.
  audio_focus_->SetSource(media_session_->GetSourceId(), kAudioFocusSourceName);
}

// static
std::unique_ptr<AudioFocusDelegate> AudioFocusDelegate::Create(
    MediaSessionImpl* media_session) {
  return std::unique_ptr<AudioFocusDelegate>(
      new AudioFocusDelegateDefault(media_session));
}

}  // namespace content

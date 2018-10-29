// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_audio_input_stream.h"

#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/threading/thread_checker.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/capture/web_contents_tracker.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "media/audio/virtual_audio_input_stream.h"
#include "media/audio/virtual_audio_output_stream.h"
#include "media/audio/virtual_audio_sink.h"
#include "media/base/bind_to_current_loop.h"

namespace content {

class WebContentsAudioInputStream::Impl
    : public base::RefCountedThreadSafe<WebContentsAudioInputStream::Impl>,
      public AudioMirroringManager::MirroringDestination {
 public:
  // Takes ownership of |mixer_stream|.  The rest outlive this instance.
  Impl(int render_process_id,
       int main_render_frame_id,
       AudioMirroringManager* mirroring_manager,
       const scoped_refptr<WebContentsTracker>& tracker,
       media::VirtualAudioInputStream* mixer_stream,
       bool is_duplication);

  // Open underlying VirtualAudioInputStream and start tracker.
  bool Open();

  // Start the underlying VirtualAudioInputStream and instruct
  // AudioMirroringManager to begin a mirroring session.
  void Start(AudioInputCallback* callback);

  // Stop the underlying VirtualAudioInputStream and instruct
  // AudioMirroringManager to shutdown a mirroring session.
  void Stop();

  // Close the underlying VirtualAudioInputStream and stop the tracker.
  void Close();

  // Accessor to underlying VirtualAudioInputStream.
  media::VirtualAudioInputStream* mixer_stream() const {
    return mixer_stream_.get();
  }

 private:
  friend class base::RefCountedThreadSafe<WebContentsAudioInputStream::Impl>;

  enum State {
    CONSTRUCTED,
    OPENED,
    MIRRORING,
    CLOSED
  };

  ~Impl() override;

  // Notifies the consumer callback that the stream is now dead.
  void ReportError();

  // (Re-)Start/Stop mirroring by posting a call to AudioMirroringManager on the
  // IO BrowserThread.
  void StartMirroring();
  void StopMirroring();

  // Increment/decrement the capturer count on the UI BrowserThread.
  void IncrementCapturerCount();
  void DecrementCapturerCount();

  // Invoked on the UI thread to make sure WebContents muting is turned off for
  // successful audio capture.
  void UnmuteWebContentsAudio();

  // AudioMirroringManager::MirroringDestination implementation
  void QueryForMatches(const std::set<GlobalFrameRoutingId>& candidates,
                       MatchesCallback results_callback) override;
  void QueryForMatchesOnUIThread(
      const std::set<GlobalFrameRoutingId>& candidates,
      MatchesCallback results_callback);
  media::AudioOutputStream* AddInput(
      const media::AudioParameters& params) override;
  media::AudioPushSink* AddPushInput(
      const media::AudioParameters& params) override;

  // Callback which is run when |stream| is closed.  Deletes |stream|.
  void ReleaseInput(media::VirtualAudioOutputStream* stream);
  void ReleasePushInput(media::VirtualAudioSink* sink);

  // Called by WebContentsTracker when the target of the audio mirroring has
  // changed.
  void OnTargetChanged(bool had_target);

  // Injected dependencies.
  const int initial_render_process_id_;
  const int initial_main_render_frame_id_;
  AudioMirroringManager* const mirroring_manager_;
  const scoped_refptr<WebContentsTracker> tracker_;
  // The AudioInputStream implementation that handles the audio conversion and
  // mixing details.
  const std::unique_ptr<media::VirtualAudioInputStream> mixer_stream_;

  State state_;

  // Set to true if |tracker_| reports a NULL target, which indicates the target
  // is permanently lost.
  bool is_target_lost_;

  // Current callback used to consume the resulting mixed audio data.
  AudioInputCallback* callback_;

  // If true, this WebContentsAudioInputStream will request a duplication of
  // audio data, instead of exclusive access to pull the audio data.
  bool is_duplication_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

WebContentsAudioInputStream::Impl::Impl(
    int render_process_id,
    int main_render_frame_id,
    AudioMirroringManager* mirroring_manager,
    const scoped_refptr<WebContentsTracker>& tracker,
    media::VirtualAudioInputStream* mixer_stream,
    bool is_duplication)
    : initial_render_process_id_(render_process_id),
      initial_main_render_frame_id_(main_render_frame_id),
      mirroring_manager_(mirroring_manager),
      tracker_(tracker),
      mixer_stream_(mixer_stream),
      state_(CONSTRUCTED),
      is_target_lost_(false),
      callback_(nullptr),
      is_duplication_(is_duplication) {
  DCHECK(mirroring_manager_);
  DCHECK(tracker_);
  DCHECK(mixer_stream_);

  // WAIS::Impl can be constructed on any thread, but will DCHECK that all
  // its methods from here on are called from the same thread.
  thread_checker_.DetachFromThread();
}

WebContentsAudioInputStream::Impl::~Impl() {
  DCHECK(state_ == CONSTRUCTED || state_ == CLOSED);
}

bool WebContentsAudioInputStream::Impl::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(CONSTRUCTED, state_) << "Illegal to Open more than once.";

  // For browser tests, not to start audio track to a fake tab.
  if (initial_render_process_id_ == DesktopMediaID::kFakeId &&
      initial_main_render_frame_id_ == DesktopMediaID::kFakeId)
    return true;

  if (!mixer_stream_->Open())
    return false;

  state_ = OPENED;
  tracker_->Start(
      initial_render_process_id_, initial_main_render_frame_id_,
      base::Bind(&Impl::OnTargetChanged, this));
  IncrementCapturerCount();

  return true;
}

void WebContentsAudioInputStream::Impl::IncrementCapturerCount() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&Impl::IncrementCapturerCount, this));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (WebContents* contents = tracker_->web_contents())
    contents->IncrementCapturerCount(gfx::Size());
}

void WebContentsAudioInputStream::Impl::Start(AudioInputCallback* callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (state_ != OPENED)
    return;

  callback_ = callback;
  if (is_target_lost_) {
    ReportError();
    callback_ = nullptr;
    return;
  }

  state_ = MIRRORING;
  mixer_stream_->Start(callback);

  StartMirroring();

  // WebContents audio muting is implemented as audio capture to nowhere.
  // Unmuting will stop that audio capture, allowing AudioMirroringManager to
  // divert audio capture to here.
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(&Impl::UnmuteWebContentsAudio, this));
}

void WebContentsAudioInputStream::Impl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != MIRRORING)
    return;

  state_ = OPENED;

  mixer_stream_->Stop();
  callback_ = nullptr;

  StopMirroring();
}

void WebContentsAudioInputStream::Impl::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());

  Stop();

  if (state_ == OPENED) {
    state_ = CONSTRUCTED;
    DecrementCapturerCount();
    tracker_->Stop();
    mixer_stream_->Close();
  }

  DCHECK_EQ(CONSTRUCTED, state_);
  state_ = CLOSED;
}

void WebContentsAudioInputStream::Impl::DecrementCapturerCount() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&Impl::DecrementCapturerCount, this));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (WebContents* contents = tracker_->web_contents())
    contents->DecrementCapturerCount();
}

void WebContentsAudioInputStream::Impl::ReportError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  callback_->OnError();
}

void WebContentsAudioInputStream::Impl::StartMirroring() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioMirroringManager::StartMirroring,
                     base::Unretained(mirroring_manager_),
                     base::RetainedRef(this)));
}

void WebContentsAudioInputStream::Impl::StopMirroring() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&AudioMirroringManager::StopMirroring,
                                          base::Unretained(mirroring_manager_),
                                          base::RetainedRef(this)));
}

void WebContentsAudioInputStream::Impl::UnmuteWebContentsAudio() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* const contents = tracker_->web_contents();
  if (contents)
    contents->SetAudioMuted(false);
}

void WebContentsAudioInputStream::Impl::QueryForMatches(
    const std::set<GlobalFrameRoutingId>& candidates,
    MatchesCallback results_callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&Impl::QueryForMatchesOnUIThread, this, candidates,
                     media::BindToCurrentLoop(std::move(results_callback))));
}

void WebContentsAudioInputStream::Impl::QueryForMatchesOnUIThread(
    const std::set<GlobalFrameRoutingId>& candidates,
    MatchesCallback results_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::set<GlobalFrameRoutingId> matches;
  WebContents* const contents = tracker_->web_contents();
  if (contents) {
    // Add each ID to |matches| if it maps to a RenderFrameHost that maps to the
    // currently-tracked WebContents.
    for (const auto& it : candidates) {
      WebContents* const contents_containing_frame =
          WebContents::FromRenderFrameHost(
              RenderFrameHost::FromID(it.child_id, it.frame_routing_id));
      if (contents_containing_frame == contents)
        matches.insert(it);
    }
  }

  std::move(results_callback).Run(matches, is_duplication_);
}

media::AudioOutputStream* WebContentsAudioInputStream::Impl::AddInput(
    const media::AudioParameters& params) {
  // Note: The closure created here holds a reference to "this," which will
  // guarantee the VirtualAudioInputStream (mixer_stream_) outlives the
  // VirtualAudioOutputStream.
  return new media::VirtualAudioOutputStream(
      params,
      mixer_stream_.get(),
      base::Bind(&Impl::ReleaseInput, this));
}

void WebContentsAudioInputStream::Impl::ReleaseInput(
    media::VirtualAudioOutputStream* stream) {
  delete stream;
}

media::AudioPushSink* WebContentsAudioInputStream::Impl::AddPushInput(
    const media::AudioParameters& params) {
  // Note: The closure created here holds a reference to "this," which will
  // guarantee the VirtualAudioInputStream (mixer_stream_) outlives the
  // VirtualAudioSink.
  return new media::VirtualAudioSink(params, mixer_stream_.get(),
                                     base::Bind(&Impl::ReleasePushInput, this));
}

void WebContentsAudioInputStream::Impl::ReleasePushInput(
    media::VirtualAudioSink* stream) {
  delete stream;
}

void WebContentsAudioInputStream::Impl::OnTargetChanged(bool had_target) {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_target_lost_ = !had_target;

  if (state_ == MIRRORING) {
    if (is_target_lost_) {
      ReportError();
      Stop();
    } else {
      StartMirroring();
    }
  }
}

// static
WebContentsAudioInputStream* WebContentsAudioInputStream::Create(
    const std::string& device_id,
    const media::AudioParameters& params,
    const scoped_refptr<base::SingleThreadTaskRunner>& worker_task_runner,
    AudioMirroringManager* audio_mirroring_manager) {
  WebContentsMediaCaptureId media_id;
  if (!WebContentsMediaCaptureId::Parse(device_id, &media_id)) {
    return nullptr;
  }

  return new WebContentsAudioInputStream(
      media_id.render_process_id, media_id.main_render_frame_id,
      audio_mirroring_manager, new WebContentsTracker(false),
      new media::VirtualAudioInputStream(
          params, worker_task_runner,
          media::VirtualAudioInputStream::AfterCloseCallback()),
      !media_id.disable_local_echo);
}

WebContentsAudioInputStream::WebContentsAudioInputStream(
    int render_process_id,
    int main_render_frame_id,
    AudioMirroringManager* mirroring_manager,
    const scoped_refptr<WebContentsTracker>& tracker,
    media::VirtualAudioInputStream* mixer_stream,
    bool is_duplication)
    : impl_(new Impl(render_process_id,
                     main_render_frame_id,
                     mirroring_manager,
                     tracker,
                     mixer_stream,
                     is_duplication)) {}

WebContentsAudioInputStream::~WebContentsAudioInputStream() {}

bool WebContentsAudioInputStream::Open() {
  return impl_->Open();
}

void WebContentsAudioInputStream::Start(AudioInputCallback* callback) {
  impl_->Start(callback);
}

void WebContentsAudioInputStream::Stop() {
  impl_->Stop();
}

void WebContentsAudioInputStream::Close() {
  impl_->Close();
  delete this;
}

double WebContentsAudioInputStream::GetMaxVolume() {
  return impl_->mixer_stream()->GetMaxVolume();
}

void WebContentsAudioInputStream::SetVolume(double volume) {
  impl_->mixer_stream()->SetVolume(volume);
}

double WebContentsAudioInputStream::GetVolume() {
  return impl_->mixer_stream()->GetVolume();
}

bool WebContentsAudioInputStream::SetAutomaticGainControl(bool enabled) {
  return impl_->mixer_stream()->SetAutomaticGainControl(enabled);
}

bool WebContentsAudioInputStream::GetAutomaticGainControl() {
  return impl_->mixer_stream()->GetAutomaticGainControl();
}

bool WebContentsAudioInputStream::IsMuted() {
  return false;
}

void WebContentsAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

}  // namespace content

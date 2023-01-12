// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/track_observer.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

class TrackObserver::TrackObserverImpl
    : public WTF::ThreadSafeRefCounted<TrackObserver::TrackObserverImpl>,
      public webrtc::ObserverInterface {
 public:
  TrackObserverImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const scoped_refptr<webrtc::MediaStreamTrackInterface>& track)
      : main_thread_(main_thread), track_(track) {
    // We're on the signaling thread.
    DCHECK(!main_thread_->BelongsToCurrentThread());
    track->RegisterObserver(this);
  }

  const scoped_refptr<webrtc::MediaStreamTrackInterface>& track() const {
    return track_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread() const {
    return main_thread_;
  }

 protected:
  friend class TrackObserver;
  void SetCallback(const OnChangedCallback& callback) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(callback_.is_null());
    DCHECK(!callback.is_null());
    callback_ = callback;
  }

  // This needs to be called by the owner of the observer instance before
  // the owner releases its reference.
  // The reason for this is to avoid a potential race when unregistration is
  // done from the main thread while an event is being delivered on the
  // signaling thread.  If, on the main thread, we're releasing the last
  // reference to the observer and attempt to unregister from the observer's
  // dtor, and at the same time receive an OnChanged event on the signaling
  // thread, we will attempt to increment the refcount in the callback
  // from 0 to 1 while the object is being freed.  Not good.
  void Unregister() {
    DCHECK(main_thread_->BelongsToCurrentThread());
    callback_.Reset();
    track_->UnregisterObserver(this);
    // At this point we're guaranteed to not get further callbacks, so it's
    // OK to reset the pointer.
    track_ = nullptr;
  }

 private:
  friend class WTF::ThreadSafeRefCounted<TrackObserverImpl>;
  ~TrackObserverImpl() override {
    DCHECK(!track_.get()) << "must have been unregistered before deleting";
  }

  // webrtc::ObserverInterface implementation.
  void OnChanged() override {
    DCHECK(!main_thread_->BelongsToCurrentThread());
    webrtc::MediaStreamTrackInterface::TrackState state = track_->state();
    PostCrossThreadTask(
        *main_thread_.get(), FROM_HERE,
        CrossThreadBindOnce(&TrackObserverImpl::OnChangedOnMainThread,
                            WrapRefCounted(this), state));
  }

  void OnChangedOnMainThread(
      webrtc::MediaStreamTrackInterface::TrackState state) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (!callback_.is_null())
      callback_.Run(state);
  }

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  scoped_refptr<webrtc::MediaStreamTrackInterface> track_;
  OnChangedCallback callback_;  // Only touched on the main thread.
};

TrackObserver::TrackObserver(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    const scoped_refptr<webrtc::MediaStreamTrackInterface>& track)
    : observer_(base::MakeRefCounted<TrackObserverImpl>(main_thread, track)) {}

TrackObserver::~TrackObserver() {
  // Explicitly unregister before releasing our reference.
  // We do this to avoid a race that could happen if we try to unregister
  // inside the dtor of the observer and then receive an event that causes
  // the ref count to go up while being destroyed.
  observer_->Unregister();
}

void TrackObserver::SetCallback(const OnChangedCallback& callback) {
  DCHECK(observer_->main_thread()->BelongsToCurrentThread());
  observer_->SetCallback(callback);
}

const scoped_refptr<webrtc::MediaStreamTrackInterface>& TrackObserver::track()
    const {
  return observer_->track();
}

}  // namespace blink

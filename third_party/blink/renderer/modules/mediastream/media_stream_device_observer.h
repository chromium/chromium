// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_DEVICE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_DEVICE_OBSERVER_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {
class UserMediaProcessor;
class WebLocalFrame;

// This class implements a Mojo object that receives device stopped
// notifications and forwards them to UserMediaProcessor.
class MODULES_EXPORT MediaStreamDeviceObserver
    : public mojom::blink::MediaStreamDeviceObserver {
 public:
  explicit MediaStreamDeviceObserver(WebLocalFrame* frame);

  ~MediaStreamDeviceObserver() override;

  // Get all the media devices of video capture, e.g. webcam. This is the set
  // of devices that should be suspended when the content frame is no longer
  // being shown to the user.
  blink::MediaStreamDevices GetNonScreenCaptureDevices();

  void AddStream(
      const String& label,
      const blink::MediaStreamDevices& audio_devices,
      const blink::MediaStreamDevices& video_devices,
      WebMediaStreamDeviceObserver::OnDeviceStoppedCb on_device_stopped_cb,
      WebMediaStreamDeviceObserver::OnDeviceChangedCb on_device_changed_cb);
  void AddStream(const String& label, const blink::MediaStreamDevice& device);
  bool RemoveStream(const String& label);
  void RemoveStreamDevice(const blink::MediaStreamDevice& device);

  // Get the video session_id given a label. The label identifies a stream.
  // If the label does not designate a valid video session, an empty token
  // will be returned.
  base::UnguessableToken GetVideoSessionId(const String& label);
  // Returns an audio session_id given a label.
  // If the label does not designate a valid video session, an empty token
  // will be returned.
  base::UnguessableToken GetAudioSessionId(const String& label);

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDeviceObserverTest,
                           GetNonScreenCaptureDevices);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDeviceObserverTest, OnDeviceStopped);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDeviceObserverTest, OnDeviceChanged);

  // Private class for keeping track of opened devices and who have
  // opened it.
  struct Stream;

  // mojom::MediaStreamDeviceObserver implementation.
  void OnDeviceStopped(const String& label,
                       const MediaStreamDevice& device) override;
  void OnDeviceChanged(const String& label,
                       const MediaStreamDevice& old_device,
                       const MediaStreamDevice& new_device) override;

  void BindMediaStreamDeviceObserverReceiver(
      mojo::PendingReceiver<mojom::blink::MediaStreamDeviceObserver> receiver);

  mojo::Receiver<mojom::blink::MediaStreamDeviceObserver> receiver_{this};

  // Used for DCHECKs so methods calls won't execute in the wrong thread.
  THREAD_CHECKER(thread_checker_);

  using LabelStreamMap = HashMap<String, Stream>;
  LabelStreamMap label_stream_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDeviceObserver);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_DEVICE_OBSERVER_H_

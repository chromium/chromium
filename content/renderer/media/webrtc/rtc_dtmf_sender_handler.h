// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_DTMF_SENDER_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_DTMF_SENDER_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/web_rtc_dtmf_sender_handler.h"
#include "third_party/blink/public/platform/web_rtc_dtmf_sender_handler_client.h"
#include "third_party/webrtc/api/dtmfsenderinterface.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// RtcDtmfSenderHandler is a delegate for the RTC DTMF Sender API messages
// going between WebKit and native DTMF Sender in libjingle.
// Instances of this class are owned by WebKit.
// WebKit call all of these methods on the main render thread.
// Callbacks to the webrtc::DtmfSenderObserverInterface implementation also
// occur on the main render thread.
class CONTENT_EXPORT RtcDtmfSenderHandler
    : public blink::WebRTCDTMFSenderHandler {
 public:
  RtcDtmfSenderHandler(scoped_refptr<base::SingleThreadTaskRunner> main_thread,
                       webrtc::DtmfSenderInterface* dtmf_sender);
  ~RtcDtmfSenderHandler() override;

  // blink::WebRTCDTMFSenderHandler implementation.
  void SetClient(blink::WebRTCDTMFSenderHandlerClient* client) override;
  blink::WebString CurrentToneBuffer() override;
  bool CanInsertDTMF() override;
  bool InsertDTMF(const blink::WebString& tones,
                  long duration,
                  long interToneGap) override;

  void OnToneChange(const std::string& tone);

 private:
  scoped_refptr<webrtc::DtmfSenderInterface> dtmf_sender_;
  blink::WebRTCDTMFSenderHandlerClient* webkit_client_;
  class Observer;
  scoped_refptr<Observer> observer_;

  SEQUENCE_CHECKER(sequence_checker_);

  // |weak_factory_| must be the last member.
  base::WeakPtrFactory<RtcDtmfSenderHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RtcDtmfSenderHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_DTMF_SENDER_HANDLER_H_

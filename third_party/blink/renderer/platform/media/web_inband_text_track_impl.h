// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_INBAND_TEXT_TRACK_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_INBAND_TEXT_TRACK_IMPL_H_

#include "third_party/blink/public/platform/web_inband_text_track.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT WebInbandTextTrackImpl
    : public blink::WebInbandTextTrack {
 public:
  WebInbandTextTrackImpl(Kind kind,
                         const blink::WebString& label,
                         const blink::WebString& language,
                         const blink::WebString& id);
  WebInbandTextTrackImpl(const WebInbandTextTrackImpl&) = delete;
  WebInbandTextTrackImpl& operator=(const WebInbandTextTrackImpl&) = delete;
  ~WebInbandTextTrackImpl() override;

  void SetClient(blink::WebInbandTextTrackClient* client) override;
  blink::WebInbandTextTrackClient* Client() override;

  Kind GetKind() const override;

  blink::WebString Label() const override;
  blink::WebString Language() const override;
  blink::WebString Id() const override;

 private:
  blink::WebInbandTextTrackClient* client_;
  Kind kind_;
  blink::WebString label_;
  blink::WebString language_;
  blink::WebString id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_INBAND_TEXT_TRACK_IMPL_H_

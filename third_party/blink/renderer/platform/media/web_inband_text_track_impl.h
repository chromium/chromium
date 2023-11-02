// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_INBAND_TEXT_TRACK_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_INBAND_TEXT_TRACK_IMPL_H_

#include "third_party/blink/public/platform/web_inband_text_track.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT WebInbandTextTrackImpl : public WebInbandTextTrack {
 public:
  WebInbandTextTrackImpl(Kind kind,
                         const WebString& label,
                         const WebString& language,
                         const WebString& id);
  WebInbandTextTrackImpl(const WebInbandTextTrackImpl&) = delete;
  WebInbandTextTrackImpl& operator=(const WebInbandTextTrackImpl&) = delete;
  ~WebInbandTextTrackImpl() override;

  void SetClient(WebInbandTextTrackClient* client) override;
  WebInbandTextTrackClient* Client() override;

  Kind GetKind() const override;

  WebString Label() const override;
  WebString Language() const override;
  WebString Id() const override;

 private:
  WebInbandTextTrackClient* client_;
  Kind kind_;
  WebString label_;
  WebString language_;
  WebString id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_INBAND_TEXT_TRACK_IMPL_H_

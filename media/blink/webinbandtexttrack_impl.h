// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBINBANDTEXTTRACK_IMPL_H_
#define MEDIA_BLINK_WEBINBANDTEXTTRACK_IMPL_H_

#include "base/macros.h"
#include "third_party/blink/public/platform/web_inband_text_track.h"
#include "third_party/blink/public/platform/web_string.h"

namespace media {

class WebInbandTextTrackImpl : public blink::WebInbandTextTrack {
 public:
  WebInbandTextTrackImpl(Kind kind,
                         const blink::WebString& label,
                         const blink::WebString& language,
                         const blink::WebString& id);
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
  DISALLOW_COPY_AND_ASSIGN(WebInbandTextTrackImpl);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBINBANDTEXTTRACK_IMPL_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/web_inband_text_track_impl.h"

#include "base/check.h"

namespace blink {

WebInbandTextTrackImpl::WebInbandTextTrackImpl(Kind kind,
                                               const blink::WebString& label,
                                               const blink::WebString& language,
                                               const blink::WebString& id)
    : client_(nullptr),
      kind_(kind),
      label_(label),
      language_(language),
      id_(id) {}

WebInbandTextTrackImpl::~WebInbandTextTrackImpl() {
  DCHECK(!client_);
}

void WebInbandTextTrackImpl::SetClient(
    blink::WebInbandTextTrackClient* client) {
  client_ = client;
}

blink::WebInbandTextTrackClient* WebInbandTextTrackImpl::Client() {
  return client_;
}

WebInbandTextTrackImpl::Kind WebInbandTextTrackImpl::GetKind() const {
  return kind_;
}

blink::WebString WebInbandTextTrackImpl::Label() const {
  return label_;
}

blink::WebString WebInbandTextTrackImpl::Language() const {
  return language_;
}

blink::WebString WebInbandTextTrackImpl::Id() const {
  return id_;
}

}  // namespace blink

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/web_inband_text_track_impl.h"

#include "base/check.h"

namespace blink {

WebInbandTextTrackImpl::WebInbandTextTrackImpl(Kind kind,
                                               const WebString& label,
                                               const WebString& language,
                                               const WebString& id)
    : client_(nullptr),
      kind_(kind),
      label_(label),
      language_(language),
      id_(id) {}

WebInbandTextTrackImpl::~WebInbandTextTrackImpl() {
  DCHECK(!client_);
}

void WebInbandTextTrackImpl::SetClient(WebInbandTextTrackClient* client) {
  client_ = client;
}

WebInbandTextTrackClient* WebInbandTextTrackImpl::Client() {
  return client_;
}

WebInbandTextTrackImpl::Kind WebInbandTextTrackImpl::GetKind() const {
  return kind_;
}

WebString WebInbandTextTrackImpl::Label() const {
  return label_;
}

WebString WebInbandTextTrackImpl::Language() const {
  return language_;
}

WebString WebInbandTextTrackImpl::Id() const {
  return id_;
}

}  // namespace blink

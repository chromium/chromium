// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ACTIVE_URL_MESSAGE_FILTER_H_
#define CONTENT_PUBLIC_BROWSER_ACTIVE_URL_MESSAGE_FILTER_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {

class RenderFrameHost;

namespace internal {

// Message filter which sets the active URL for crash reporting while a mojo
// message is being handled.
//
// TODO(csharrison): Move out of the internal namespace if callers outside of
// //content are found. This is only exposed in //content/public because it is
// used by the RenderFrameHostReceiverSet which is a header-only class.
class CONTENT_EXPORT ActiveUrlMessageFilter : public mojo::MessageFilter {
 public:
  explicit ActiveUrlMessageFilter(RenderFrameHost* render_frame_host);
  ~ActiveUrlMessageFilter() override;

  // mojo::MessageFilter overrides.
  bool WillDispatch(mojo::Message* message) override;
  void DidDispatchOrReject(mojo::Message* message, bool accepted) override;

 private:
  raw_ptr<RenderFrameHost> render_frame_host_;
  bool debug_url_set_ = false;
};

}  // namespace internal
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ACTIVE_URL_MESSAGE_FILTER_H_

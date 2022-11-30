// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCOPED_ACTIVE_URL_H_
#define CONTENT_BROWSER_SCOPED_ACTIVE_URL_H_

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace content {

class FrameTreeNode;
class RenderFrameHost;
class RenderFrameProxyHost;
class RenderViewHost;

// ScopedActiveURL calls ContentClient::SetActiveURL when constructed
// and calls it again with empty arguments when destructed.
class ScopedActiveURL {
 public:
  // Calls ContentClient::SetActiveURL with |active_url| and |top_origin| (to
  // set the crash keys).
  ScopedActiveURL(const GURL& active_url, const url::Origin& top_origin);

  // Convenience constructor, calculating |active_url| and |top_origin| based on
  // |frame|'s last committed origin and main frame.
  explicit ScopedActiveURL(RenderFrameHost* frame);

  // Convenience constructor, calculating |active_url| and |top_origin| based on
  // |proxy|'s last committed origin and main frame.
  explicit ScopedActiveURL(RenderFrameProxyHost* proxy);

  // Convenience constructor, calculating |active_url| and |top_origin| based on
  // the frame tree node of |view|'s main frame.
  explicit ScopedActiveURL(RenderViewHost* view);

  ScopedActiveURL(const ScopedActiveURL&) = delete;
  ScopedActiveURL& operator=(const ScopedActiveURL&) = delete;

  // Calls ContentClient::SetActiveURL with empty arguments (to reset the crash
  // keys).
  ~ScopedActiveURL();

 private:
  explicit ScopedActiveURL(FrameTreeNode* node);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCOPED_ACTIVE_URL_H_

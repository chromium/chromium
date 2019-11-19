// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_FRAME_TREE_NODE_ID_REGISTRY_H_
#define CONTENT_BROWSER_WEB_CONTENTS_FRAME_TREE_NODE_ID_REGISTRY_H_

#include <map>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"

namespace content {

class WebContents;

// A global map of UnguessableToken to FrameTreeNode id. This registry lives and
// is used only on the thread identified by
// ServiceWorkerContext::GetCoreThreadId(), as that's the thread the
// class that adds/removes from this registry is on (ServiceWorkerProviderHost).
// TODO(crbug.com/824858): Make this live on the UI thread once the service
// worker core thread moves to the UI thread.
//
// This is currently used to map a network request to a frame so
// that the network service can tell the browser to display tab-level UI
// required for the request in certain cases, including client certificates and
// basic HTTP authentication.
//
// It uses FrameTreeNode rather than RenderFrameHost because the lookup can
// happen for browser-initiated navigation requests, where a RenderFrameHost
// might not have been created yet.
//
// Warning: A corresponding frame may have changed security contexts since it
// was added.  It's useful for looking up a WebContents or determining if it's a
// main frame or not, but callers should not make assumptions that it's in the
// same renderer process or origin as when it was added to the registry.
// To prevent a potential risk, the registry doesn't provide
// |static int /* FrameTreeNode id */ Get(const base::UnguessableToken& id)|.
class FrameTreeNodeIdRegistry {
 public:
  using WebContentsGetter = base::RepeatingCallback<WebContents*()>;
  using IsMainFrameGetter = base::RepeatingCallback<base::Optional<bool>()>;

  static FrameTreeNodeIdRegistry* GetInstance();

  void Add(const base::UnguessableToken& id, const int frame_tree_node_id);
  void Remove(const base::UnguessableToken&);
  // Returns a null callback if not found.
  WebContentsGetter GetWebContentsGetter(
      const base::UnguessableToken& id) const;
  // Returns a null callback if not found.  The returned callback will return
  // nullopt if a corresponding FrameTreeNode is not found.
  IsMainFrameGetter GetIsMainFrameGetter(
      const base::UnguessableToken& id) const;

 private:
  friend class base::NoDestructor<FrameTreeNodeIdRegistry>;

  FrameTreeNodeIdRegistry();
  ~FrameTreeNodeIdRegistry();

  std::map<base::UnguessableToken, int> map_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(FrameTreeNodeIdRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_FRAME_TREE_NODE_ID_REGISTRY_H_

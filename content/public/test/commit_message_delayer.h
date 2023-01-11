// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_COMMIT_MESSAGE_DELAYER_H_
#define CONTENT_PUBLIC_TEST_COMMIT_MESSAGE_DELAYER_H_

#include <memory>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHost;
class WebContents;

// A helper class to run a predefined callback just before processing the
// DidCommitProvisionalLoad IPC for |deferred_url|.
class CommitMessageDelayer {
 public:
  using DidCommitCallback = base::OnceCallback<void(RenderFrameHost*)>;

  // Starts monitoring |web_contents| for DidCommit IPC and executes
  // |deferred_action| for each DidCommit IPC that matches |deferred_url|.
  explicit CommitMessageDelayer(WebContents* web_contents,
                                const GURL& deferred_url,
                                DidCommitCallback deferred_action);
  ~CommitMessageDelayer();

  CommitMessageDelayer(const CommitMessageDelayer&) = delete;
  CommitMessageDelayer& operator=(const CommitMessageDelayer&) = delete;

  // Waits until DidCommit IPC arrives for |deferred_url|, then calls
  // |deferred_action|, then handles the IPC, then returns.
  void Wait();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_COMMIT_MESSAGE_DELAYER_H_

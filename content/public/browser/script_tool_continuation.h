// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SCRIPT_TOOL_CONTINUATION_H_
#define CONTENT_PUBLIC_BROWSER_SCRIPT_TOOL_CONTINUATION_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "content/public/browser/weak_document_ptr.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

class RenderFrameHost;

using ScriptToolContinuationTokenCreatedCallback = base::OnceClosure;
// `resumed_document` is the document that ran the resumed step, which may be in
// a different frame than the document that requested the token. It's null if
// the continuation failed before the token was redeemed.
using ScriptToolContinuationCompletedCallback =
    base::OnceCallback<void(base::expected<std::string, std::string> result,
                            WeakDocumentPtr resumed_document)>;

// A browser-initiated script tool execution registered with
// RegisterExternalScriptToolExecution(). Destroying it unregisters the
// execution, so the target document can no longer request a continuation token
// for it. If the target document already requested a token, destroying it also
// cancels the continuation: a running resumed step is aborted, and an
// unredeemed token stays redeemable until it expires, but its result is
// discarded.
//
// Must only be used on the UI thread.
class CONTENT_EXPORT ScriptToolExecutionRegistration {
 public:
  virtual ~ScriptToolExecutionRegistration() = default;

  // Sets `callback` to run with the result of the resumed step when the
  // continuation of this execution completes, or with an error if it fails.
  // Must be called at most once, from `on_token_created`. `callback` never runs
  // synchronously, never runs after `this` is destroyed, and never runs if the
  // target document doesn't request a continuation token.
  virtual void WaitForContinuation(
      ScriptToolContinuationCompletedCallback callback) = 0;
};

// Registers a browser-initiated script tool execution (such as from
// `actor::ScriptToolHost`) so that `ScriptToolContinuationManager` can look
// up its target document, `tool_name`, and `input_arguments` if the tool
// requests a continuation token. The caller should keep the returned
// registration until the tool execution finishes.
//
// Returns null if `target_rfh` isn't active or isn't in the primary page,
// because such a document can't request a continuation token.
//
// The caller must receive the result of the tool's initial step over an
// interface that is ordered with the target document's continuation token
// request (for example, a channel-associated interface of the target frame),
// so that `on_token_created` runs before that result arrives.
//
// If the target document requests a continuation token, `on_token_created`
// runs synchronously while the request is handled, before the token is
// returned to the target document. It never runs after the registration is
// destroyed.
//
// Must only be called on the UI thread.
[[nodiscard]] CONTENT_EXPORT std::unique_ptr<ScriptToolExecutionRegistration>
RegisterExternalScriptToolExecution(
    RenderFrameHost& target_rfh,
    const blink::ToolInvocationToken& invocation_id,
    std::string tool_name,
    std::string input_arguments,
    ScriptToolContinuationTokenCreatedCallback on_token_created);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SCRIPT_TOOL_CONTINUATION_H_

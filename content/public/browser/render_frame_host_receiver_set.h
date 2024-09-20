// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_RECEIVER_SET_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_RECEIVER_SET_H_

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/active_url_message_filter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {

class WebContents;

// Owns a set of Channel-associated interface receivers with frame context on
// message dispatch.
//
// When messages are dispatched to the implementation, the implementation can
// call GetCurrentTargetFrame() on this object (see below) to determine which
// frame sent the message.
//
// In order to expose the interface to all RenderFrames, a binder must be
// registered for the interface. Typically this is done in
// RegisterAssociatedInterfaceBindersForRenderFrameHost() in a
// ContentBrowserClient subclass.  Doing that will expose the interface to all
// remote RenderFrame objects. If the WebContents is destroyed at any point, the
// receivers will automatically reset and will cease to dispatch further
// incoming messages.
//
// Because this object uses Channel-associated interface receivers, all messages
// sent via these interfaces are ordered with respect to legacy Chrome IPC
// messages on the relevant IPC::Channel (i.e. the Channel between the browser
// and whatever render process hosts the sending frame.)
//
// Because this is a templated class, its complete implementation lives in the
// header file.
template <typename Interface>
class CONTENT_EXPORT RenderFrameHostReceiverSet : public WebContentsObserver {
 public:
  using ImplPointerType = Interface*;

  RenderFrameHostReceiverSet(WebContents* web_contents, Interface* impl)
      : WebContentsObserver(web_contents), impl_(impl) {}
  ~RenderFrameHostReceiverSet() override = default;

  RenderFrameHostReceiverSet(const RenderFrameHostReceiverSet&) = delete;
  RenderFrameHostReceiverSet& operator=(const RenderFrameHostReceiverSet&) =
      delete;

  void Bind(RenderFrameHost* render_frame_host,
            mojo::PendingAssociatedReceiver<Interface> pending_receiver) {
    // If the RenderFrameHost does not have a live RenderFrame:
    // 1. There is no point in binding receivers, as the renderer should not be
    //    doing anything with this RenderFrameHost.
    // 2. More problematic, `RenderFrameDeleted()` might not be called again
    //    for `render_frame_host`, potentially leaving dangling pointers to the
    //    RenderFrameHost (or other related objects) after the RenderFrameHost
    //    itself is later deleted.
    if (!render_frame_host->IsRenderFrameLive()) {
      return;
    }

    // Inject the ActiveUrlMessageFilter to improve crash reporting. This filter
    // sets the correct URL crash keys based on the target RFH that is
    // processing a message.
    mojo::ReceiverId id = receivers_.Add(
        impl_, std::move(pending_receiver), render_frame_host,
        std::make_unique<internal::ActiveUrlMessageFilter>(render_frame_host));
    frame_to_receivers_map_[render_frame_host].push_back(id);
  }

  // Implementations of `Interface` can call `GetCurrentTargetFrame()` to
  // determine which frame sent the message. `GetCurrentTargetFrame()` will
  // never return `nullptr`.
  //
  // Important: this method must only be called while the incoming message is
  // being dispatched on the stack.
  RETURNS_NONNULL RenderFrameHost* GetCurrentTargetFrame() {
    if (current_target_frame_for_testing_)
      return current_target_frame_for_testing_;
    return receivers_.current_context();
  }

  // Reports the currently dispatching Message as bad and closes+removes the
  // receiver which received the message. Prefer this over the global
  // `mojo::ReportBadMessage()` function, since calling this method promptly
  // disconnects the receiver, preventing further (potentially bad) messages
  // from being processed.
  //
  // Important: this method must only be called while the incoming message is
  // being dispatched on the stack. To report a bad message after asynchronous
  // processing (e.g. posting a task that then reports a the bad message), use
  // `GetMessageCallback()` and pass the returned callback to the async task
  // that needs to report the message as bad.
  NOT_TAIL_CALLED void ReportBadMessage(const std::string& message) {
    receivers_.ReportBadMessage(message);
  }

  // Creates a callback which, when run, reports the currently dispatching
  // Message as bad and closes+removes the receiver which received the message.
  // Prefer this over the global `mojo::GetBadMessageCallback()` function,
  // since running the callback promptly disconnects the receiver, preventing
  // further (potentially bad) messages from being processed.
  //
  // Important: like `ReportBadMessage()`, this method must only be called while
  // the incoming message is being dispatched on the stack. However, unlike
  // `ReportBadMessage()`, the returned callback may be called even if the
  // original message is no longer being dispatched on the stack.
  //
  // Sequence safety: the returned callback must be called on the sequence that
  // owns `this` (i.e. the UI thread).
  mojo::ReportBadMessageCallback GetBadMessageCallback() {
    return receivers_.GetBadMessageCallback();
  }

  void SetCurrentTargetFrameForTesting(RenderFrameHost* render_frame_host) {
    current_target_frame_for_testing_ = render_frame_host;
  }

  // Allows test code to swap the interface implementation.
  //
  // Returns the existing interface implementation to the caller.
  //
  // The caller needs to guarantee that `new_impl` will live longer than
  // `this` Receiver.  One way to achieve this is to store the returned
  // `old_impl` and swap it back in when `new_impl` is getting destroyed.
  // Test code should prefer using `mojo::test::ScopedSwapImplForTesting` if
  // possible.
  [[nodiscard]] ImplPointerType SwapImplForTesting(ImplPointerType new_impl) {
    ImplPointerType old_impl = impl_;
    impl_ = new_impl;

    for (const auto& it : frame_to_receivers_map_) {
      const std::vector<mojo::ReceiverId>& receiver_ids = it.second;
      for (const mojo::ReceiverId& id : receiver_ids) {
        // RenderFrameHostReceiverSet only allows all-or=nothing swaps, so
        // all the old impls are expected to be equal to `this`'s old impl_.
        CHECK_EQ(old_impl, receivers_.SwapImplForTesting(id, new_impl));
      }
    }

    return old_impl;
  }

 private:
  // content::WebContentsObserver:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    auto it = frame_to_receivers_map_.find(render_frame_host);
    if (it == frame_to_receivers_map_.end())
      return;
    for (auto id : it->second)
      receivers_.Remove(id);
    frame_to_receivers_map_.erase(it);
  }

  // Receiver set for each frame in the page. Note, bindings are reused across
  // navigations that are same-site since the RenderFrameHost is reused in that
  // case.
  mojo::AssociatedReceiverSet<Interface, RenderFrameHost*> receivers_;

  // Track which RenderFrameHosts are in the |receivers_| set so they can
  // be removed them when a RenderFrameHost is removed.
  std::map<RenderFrameHost*, std::vector<mojo::ReceiverId>>
      frame_to_receivers_map_;

  raw_ptr<RenderFrameHost> current_target_frame_for_testing_ = nullptr;

  // Must outlive this class.
  raw_ptr<Interface> impl_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_RECEIVER_SET_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_RECEIVER_SET_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_RECEIVER_SET_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace content {

class RenderFrameHost;
class WebContentsImpl;

// Base class for something which owns a mojo::AssociatedReceiverSet on behalf
// of a WebContents. See WebContentsFrameReceiverSet<T> below.
class CONTENT_EXPORT WebContentsReceiverSet {
 public:
  class CONTENT_EXPORT Binder {
   public:
    virtual ~Binder() = default;

    virtual void OnReceiverForFrame(RenderFrameHost* render_frame_host,
                                    mojo::ScopedInterfaceEndpointHandle handle);
    virtual void CloseAllReceivers();
  };

  // |binder| must outlive |this| or be reset to null before being destroyed.
  void SetBinder(Binder* binder) { binder_ = binder; }

  template <typename Interface>
  static WebContentsReceiverSet* GetForWebContents(WebContents* web_contents) {
    return GetForWebContents(web_contents, Interface::Name_);
  }

 protected:
  WebContentsReceiverSet(WebContents* web_contents,
                         const std::string& interface_name);
  ~WebContentsReceiverSet();

 private:
  friend class WebContentsImpl;

  static WebContentsReceiverSet* GetForWebContents(WebContents* web_contents,
                                                   const char* interface_name);

  void CloseAllReceivers();
  void OnReceiverForFrame(RenderFrameHost* render_frame_host,
                          mojo::ScopedInterfaceEndpointHandle handle);

  base::OnceClosure remove_callback_;
  Binder* binder_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebContentsReceiverSet);
};

// Owns a set of Channel-associated interface receivers with frame context on
// message dispatch.
//
// To use this, a |mojom::Foo| implementation need only own an instance of
// WebContentsFrameReceiverSet<mojom::Foo>. This allows remote RenderFrames to
// acquire handles to the |mojom::Foo| interface via
// RenderFrame::GetRemoteAssociatedInterfaces() and send messages here. When
// messages are dispatched to the implementation, the implementation can call
// GetCurrentTargetFrame() on this object (see below) to determine which
// frame sent the message.
//
// For example:
//
//   class FooImpl : public mojom::Foo {
//    public:
//     explicit FooImpl(WebContents* web_contents)
//         : web_contents_(web_contents), receivers_(web_contents, this) {}
//
//     // mojom::Foo:
//     void DoFoo() override {
//       if (receivers_.GetCurrentTargetFrame() ==
//           web_contents_->GetMainFrame())
//           ; // Do something interesting
//     }
//
//    private:
//     WebContents* web_contents_;
//     WebContentsFrameReceiverSet<mojom::Foo> receivers_;
//   };
//
// When an instance of FooImpl is constructed over a WebContents, the mojom::Foo
// interface will be exposed to all remote RenderFrame objects. If the
// WebContents is destroyed at any point, the receivers will automatically reset
// and will cease to dispatch further incoming messages.
//
// If FooImpl is destroyed first, the receivers are automatically removed and
// future incoming pending receivers for mojom::Foo will be rejected.
//
// Because this object uses Channel-associated interface receivers, all messages
// sent via these interfaces are ordered with respect to legacy Chrome IPC
// messages on the relevant IPC::Channel (i.e. the Channel between the browser
// and whatever render process hosts the sending frame.)
template <typename Interface>
class WebContentsFrameReceiverSet : public WebContentsReceiverSet {
 public:
  WebContentsFrameReceiverSet(WebContents* web_contents, Interface* impl)
      : WebContentsReceiverSet(web_contents, Interface::Name_),
        binder_(this, web_contents, impl) {
    SetBinder(&binder_);
  }
  ~WebContentsFrameReceiverSet() = default;

  // Returns the RenderFrameHost currently targeted by a message dispatch to
  // this interface. Must only be called during the extent of a message dispatch
  // for this interface.
  RenderFrameHost* GetCurrentTargetFrame() {
    if (current_target_frame_for_testing_)
      return current_target_frame_for_testing_;
    return binder_.GetCurrentTargetFrame();
  }

  void SetCurrentTargetFrameForTesting(RenderFrameHost* render_frame_host) {
    current_target_frame_for_testing_ = render_frame_host;
  }

 private:
  class FrameInterfaceBinder : public Binder, public WebContentsObserver {
   public:
    FrameInterfaceBinder(WebContentsFrameReceiverSet* receiver_set,
                         WebContents* web_contents,
                         Interface* impl)
        : WebContentsObserver(web_contents), impl_(impl) {}

    ~FrameInterfaceBinder() override = default;

    RenderFrameHost* GetCurrentTargetFrame() {
      return receivers_.current_context();
    }

   private:
    // Binder:
    void OnReceiverForFrame(
        RenderFrameHost* render_frame_host,
        mojo::ScopedInterfaceEndpointHandle handle) override {
      auto id = receivers_.Add(
          impl_, mojo::PendingAssociatedReceiver<Interface>(std::move(handle)),
          render_frame_host);
      frame_to_receivers_map_[render_frame_host].push_back(id);
    }

    void CloseAllReceivers() override { receivers_.Clear(); }

    // WebContentsObserver:
    void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
      auto it = frame_to_receivers_map_.find(render_frame_host);
      if (it == frame_to_receivers_map_.end())
        return;
      for (auto id : it->second)
        receivers_.Remove(id);
      frame_to_receivers_map_.erase(it);
    }

    Interface* const impl_;
    mojo::AssociatedReceiverSet<Interface, RenderFrameHost*> receivers_;
    std::map<RenderFrameHost*, std::vector<mojo::ReceiverId>>
        frame_to_receivers_map_;

    DISALLOW_COPY_AND_ASSIGN(FrameInterfaceBinder);
  };

  FrameInterfaceBinder binder_;
  RenderFrameHost* current_target_frame_for_testing_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebContentsFrameReceiverSet);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_RECEIVER_SET_H_

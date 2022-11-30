// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_ASSOCIATED_INTERFACE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_ASSOCIATED_INTERFACE_H_

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// A helper class which owns an associated interface receiver on the IO thread.
// Subclassess of BrowserMessageFilter may use this to simplify the transition
// to Mojo interfaces.
//
// In general the correct pattern for using this is as follows:
//
//   class FooMessageFilter : public BrowserMessageFilter,
//                            public BrowserAssociatedInterface<mojom::Foo> {
//    public:
//     FooMessageFilter()
//         : BrowserMessageFilter(FooMsgStart),
//           BrowserAssociatedInterface<mojom::Foo>(this, this) {}
//
//     // BrowserMessageFilter implementation:
//     bool OnMessageReceived(const IPC::Message& message) override {
//       // ...
//       return true;
//     }
//
//     // mojom::Foo implementation:
//     void DoStuff() override { /* ... */ }
//   };
//
// The remote side of an IPC channel can request the |mojom::Foo| associated
// interface and use it would use any other associated remote proxy. Messages
// received for |mojom::Foo| on the local side of the channel will retain FIFO
// with respect to classical IPC messages received via OnMessageReceived().
//
// See BrowserAssociatedInterfaceTest.Basic for a simple working example usage.
template <typename Interface>
class BrowserAssociatedInterface : public Interface {
 public:
  // |filter| and |impl| must live at least as long as this object.
  explicit BrowserAssociatedInterface(BrowserMessageFilter* filter)
      : internal_state_(new InternalState(this)) {
    filter->AddAssociatedInterface(
        Interface::Name_,
        base::BindRepeating(&InternalState::BindReceiver, internal_state_),
        base::BindOnce(&InternalState::ClearReceivers, internal_state_));
  }

  BrowserAssociatedInterface(const BrowserAssociatedInterface&) = delete;
  BrowserAssociatedInterface& operator=(const BrowserAssociatedInterface&) =
      delete;

  ~BrowserAssociatedInterface() { internal_state_->ClearReceivers(); }

 private:
  friend class TestDriverMessageFilter;

  class InternalState : public base::RefCountedThreadSafe<InternalState> {
   public:
    explicit InternalState(Interface* impl)
        : impl_(impl), receivers_(absl::in_place) {}

    InternalState(const InternalState&) = delete;
    InternalState& operator=(const InternalState&) = delete;

    void ClearReceivers() {
      if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
        GetIOThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(&InternalState::ClearReceivers, this));
        return;
      }
      receivers_.reset();
      // `impl_` destructor calls this function, so it must be reset. It isn't
      // useful anymore, because `BindReceiver` become a no-op after resetting
      // `receiver_`.
      impl_ = nullptr;
    }

    void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
      DCHECK_CURRENTLY_ON(BrowserThread::IO);
      // If this interface has already been shut down we drop the receiver.
      if (!receivers_)
        return;
      receivers_->Add(
          impl_, mojo::PendingAssociatedReceiver<Interface>(std::move(handle)));
    }

   private:
    friend class base::RefCountedThreadSafe<InternalState>;
    friend class TestDriverMessageFilter;

    ~InternalState() {}

    raw_ptr<Interface> impl_;
    absl::optional<mojo::AssociatedReceiverSet<Interface>> receivers_;
  };

  scoped_refptr<InternalState> internal_state_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_ASSOCIATED_INTERFACE_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INTERFACE_REGISTRY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INTERFACE_REGISTRY_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/platform/web_common.h"

#if INSIDE_BLINK
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"  // nogncheck
#include "third_party/blink/renderer/platform/wtf/functional.h"  // nogncheck
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

using InterfaceFactory =
    base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>;
using AssociatedInterfaceFactory =
    base::RepeatingCallback<void(mojo::ScopedInterfaceEndpointHandle)>;

class BLINK_PLATFORM_EXPORT InterfaceRegistry {
 public:
  virtual void AddInterface(
      const char* name,
      const InterfaceFactory&,
      scoped_refptr<base::SingleThreadTaskRunner> = nullptr) = 0;
  // The usage of associated interfaces should be very limited. Please
  // consult the owners of public/platform before adding one.
  virtual void AddAssociatedInterface(const char* name,
                                      const AssociatedInterfaceFactory&) = 0;

  static InterfaceRegistry* GetEmptyInterfaceRegistry();

#if INSIDE_BLINK
  template <typename Interface>
  void AddInterface(
      base::RepeatingCallback<void(mojo::InterfaceRequest<Interface>)>
          factory) {
    AddInterface(Interface::Name_,
                 WTF::BindRepeating(
                     &InterfaceRegistry::ForwardToInterfaceFactory<Interface>,
                     std::move(factory)));
  }

  template <typename Interface>
  void AddInterface(WTF::CrossThreadRepeatingFunction<
                        void(mojo::InterfaceRequest<Interface>)> factory,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    AddInterface(
        Interface::Name_,
        ConvertToBaseCallback(blink::CrossThreadBind(
            &InterfaceRegistry::ForwardToCrossThreadInterfaceFactory<Interface>,
            std::move(factory))),
        std::move(task_runner));
  }

  template <typename Interface>
  void AddAssociatedInterface(
      base::RepeatingCallback<void(mojo::AssociatedInterfaceRequest<Interface>)>
          factory) {
    AddAssociatedInterface(
        Interface::Name_,
        WTF::BindRepeating(
            &InterfaceRegistry::ForwardToAssociatedInterfaceFactory<Interface>,
            std::move(factory)));
  }

 private:
  template <typename Interface>
  static void ForwardToInterfaceFactory(
      base::RepeatingCallback<void(mojo::InterfaceRequest<Interface>)> factory,
      mojo::ScopedMessagePipeHandle handle) {
    factory.Run(mojo::InterfaceRequest<Interface>(std::move(handle)));
  }

  template <typename Interface>
  static void ForwardToCrossThreadInterfaceFactory(
      const WTF::CrossThreadRepeatingFunction<
          void(mojo::InterfaceRequest<Interface>)>& factory,
      mojo::ScopedMessagePipeHandle handle) {
    factory.Run(mojo::InterfaceRequest<Interface>(std::move(handle)));
  }

  template <typename Interface>
  static void ForwardToAssociatedInterfaceFactory(
      base::RepeatingCallback<void(mojo::AssociatedInterfaceRequest<Interface>)>
          factory,
      mojo::ScopedInterfaceEndpointHandle handle) {
    factory.Run(mojo::AssociatedInterfaceRequest<Interface>(std::move(handle)));
  }
#endif  // INSIDE_BLINK
};

}  // namespace blink

#endif

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_MANAGED_INTERFACE_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_MANAGED_INTERFACE_H_

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_managed_interface.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

using WebUIManagedInterfaceNoPageHandler = void;

// WebUIManagedInterface is an optional base class for implementing classes
// that interact with Mojo endpoints e.g. implement a Mojo interface or
// communicate with a Mojo Remote.
//
// This class provides automated handling of Mojo endpoint bindings and
// lifetime management. The subclass object is created when Create() is
// called and destroyed when the associated WebUI document is removed or
// navigates away.
//
// To implement interface Foo, use the following code:
//   class FooImpl : public WebUIManagedInterface<FooImpl, Foo> { ... }
//
// Additionally, to communiate with remote interface Bar:
//   class FooBarImpl : public WebUIManagedInterface<FooBarImpl, Foo, Bar> { ...
//   }
//
// To implement no interface but only communicate with a remote interface:
//   class Baz : public WebUIManagedInterface<
//                            Baz,
//                            WebUIManagedInterfaceNoPageHandler,
//                            BarObserver> { ... }
//
// TODO(crbug.com/40257252): provide helpers to retrieve InterfaceImpl objects.
template <typename InterfaceImpl, typename PageHandler, typename Page = void>
  requires(!std::is_void_v<PageHandler> || !std::is_void_v<Page>)
class WebUIManagedInterface : public WebUIManagedInterfaceBase {
 public:
  template <typename T = Page>
    requires(std::is_void_v<T>)
  static void Create(WebUIController* web_ui_controller,
                     mojo::PendingReceiver<PageHandler> pending_receiver) {
    return WebUIManagedInterface::Create(
        web_ui_controller, std::move(pending_receiver), mojo::NullRemote());
  }

  template <typename T = PageHandler>
    requires(std::is_void_v<T>)
  static void Create(WebUIController* webui_controller,
                     mojo::PendingRemote<Page> pending_remote) {
    return WebUIManagedInterface::Create(webui_controller, mojo::NullReceiver(),
                                         std::move(pending_remote));
  }

  static void Create(WebUIController* webui_controller,
                     mojo::PendingReceiver<PageHandler> pending_receiver,
                     mojo::PendingRemote<Page> pending_remote) {
    auto interface_impl = std::make_unique<InterfaceImpl>();
    auto* interface_impl_ptr = interface_impl.get();
    interface_impl->webui_controller_ = webui_controller;

    if constexpr (!std::is_void_v<PageHandler>) {
      interface_impl->page_handler_receiver_.emplace(
          interface_impl_ptr, std::move(pending_receiver));
    }

    if constexpr (!std::is_void_v<Page>) {
      interface_impl->page_remote_.Bind(std::move(pending_remote));
    }

    SaveWebUIManagedInterfaceInDocument(webui_controller,
                                        std::move(interface_impl));
    interface_impl_ptr->ready_ = true;
    interface_impl_ptr->OnReady();
  }

  // Invoked when Mojo endpoints are bound and ready to use.
  // TODO(crbug.com/40257247): Remove this by saving endpoints in an external
  // storage before constructing `InterfaceImpl`.
  virtual void OnReady() {}

  template <typename P = PageHandler>
    requires(!std::is_void_v<P>)
  mojo::Receiver<PageHandler>& receiver() {
    CHECK(ready_) << "Mojo endpoints are not ready. Please use OnReady().";
    return page_handler_receiver_.value();
  }

  template <typename P = Page>
    requires(!std::is_void_v<P>)
  mojo::Remote<Page>& remote() {
    CHECK(ready_) << "Mojo endpoints are not ready. Please use OnReady().";
    return page_remote_;
  }

  WebUIController* webui_controller() {
    CHECK(ready_) << "webui_controller() is not ready. Please use OnReady().";
    return webui_controller_;
  }

 private:
  bool ready_ = false;

  // When `PageHandler` is void, declare an unused bool member variable instead
  // of a mojo::Receiver<PageHandler>. This allows us to share the same class
  // instead of having three different specializations at the cost of an extra
  // bool member.
  using PageHandlerReceiverType =
      std::conditional_t<std::is_void_v<PageHandler>,
                         bool,
                         mojo::Receiver<PageHandler>>;
  std::optional<PageHandlerReceiverType> page_handler_receiver_;

  // When `Page` is void, declare a unused bool member variable instead of a
  // mojo::Remote<PageHandler>. This allows us to share the same class
  // instead of having three different specializations at the cost of an extra
  // bool member.
  using PageRemoteType =
      std::conditional_t<std::is_void_v<Page>, bool, mojo::Remote<Page>>;
  PageRemoteType page_remote_;

  raw_ptr<WebUIController> webui_controller_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_MANAGED_INTERFACE_H_

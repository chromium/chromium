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
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

CONTENT_EXPORT void RemoveWebUIManagedInterfaces(
    WebUIController* webui_controller);

namespace internal {

class CONTENT_EXPORT WebUIManagedInterfaceBase {
 public:
  virtual ~WebUIManagedInterfaceBase() = default;
};

// Stores an interface implementation instance in the WebUI host Document.
// This is called when constructing an implementation instance in
// WebUIManagedInterface::Create().
void CONTENT_EXPORT
SaveWebUIManagedInterfaceInDocument(content::WebUIController*,
                                    std::unique_ptr<WebUIManagedInterfaceBase>);

}  // namespace internal

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
// TODO(crbug.com/1417272): provide helpers to retrieve InterfaceImpl objects.
template <typename InterfaceImpl, typename... Interfaces>
class WebUIManagedInterface;

template <typename InterfaceImpl, typename PageHandler, typename Page>
class WebUIManagedInterface<InterfaceImpl, PageHandler, Page>
    : public PageHandler, public internal::WebUIManagedInterfaceBase {
 public:
  static void Create(content::WebUIController*,
                     mojo::PendingReceiver<PageHandler>,
                     mojo::PendingRemote<Page>);

  // Invoked when Mojo endpoints are bound and ready to use.
  // TODO(crbug.com/1417260): Remove this by saving endpoints in an external
  // storage before constructing `InterfaceImpl`.
  virtual void OnReady() {}

  mojo::Receiver<PageHandler>& receiver() {
    CHECK(ready_) << "Mojo endpoints are not ready. Please use OnReady().";
    return page_handler_receiver_;
  }
  mojo::Remote<Page>& remote() {
    CHECK(ready_) << "Mojo endpoints are not ready. Please use OnReady().";
    return page_remote_;
  }
  content::WebUIController* webui_controller() {
    CHECK(ready_) << "webui_controller() is not ready. Please use OnReady().";
    return webui_controller_;
  }

 private:
  bool ready_ = false;
  mojo::Receiver<PageHandler> page_handler_receiver_{this};
  mojo::Remote<Page> page_remote_;
  raw_ptr<content::WebUIController> webui_controller_;
};

template <typename InterfaceImpl, typename PageHandler>
class WebUIManagedInterface<InterfaceImpl, PageHandler>
    : public PageHandler, public internal::WebUIManagedInterfaceBase {
 public:
  static void Create(content::WebUIController*,
                     mojo::PendingReceiver<PageHandler>);

  // Invoked when Mojo endpoints are bound and ready to use.
  // TODO(crbug.com/1417260): Remove this by saving endpoints in an external
  // storage before constructing `InterfaceImpl`.
  virtual void OnReady() {}

  mojo::Receiver<PageHandler>& receiver() {
    CHECK(ready_) << "Mojo endpoints are not ready. Please use OnReady().";
    return page_handler_receiver_;
  }
  content::WebUIController* webui_controller() {
    CHECK(ready_) << "webui_controller() is not ready. Please use OnReady().";
    return webui_controller_;
  }

 private:
  bool ready_ = false;
  mojo::Receiver<PageHandler> page_handler_receiver_{this};
  raw_ptr<content::WebUIController> webui_controller_;
};

using WebUIManagedInterfaceNoPageHandler = void;

template <typename InterfaceImpl, typename Page>
class WebUIManagedInterface<InterfaceImpl,
                            WebUIManagedInterfaceNoPageHandler,
                            Page> : public internal::WebUIManagedInterfaceBase {
 public:
  static void Create(content::WebUIController*, mojo::PendingRemote<Page>);

  // Invoked when Mojo endpoints are bound and ready to use.
  // TODO(crbug.com/1417260): Remove this by saving endpoints in an external
  // storage before constructing `InterfaceImpl`.
  virtual void OnReady() {}

  mojo::Remote<Page>& remote() {
    CHECK(ready_) << "Mojo endpoints are not ready. Please use OnReady().";
    return page_remote_;
  }
  content::WebUIController* webui_controller() {
    CHECK(ready_) << "webui_controller() is not ready. Please use OnReady().";
    return webui_controller_;
  }

 private:
  bool ready_ = false;
  mojo::Remote<Page> page_remote_;
  raw_ptr<content::WebUIController> webui_controller_;
};

template <typename InterfaceImpl, typename PageHandler, typename Page>
void WebUIManagedInterface<InterfaceImpl, PageHandler, Page>::Create(
    content::WebUIController* webui_controller,
    mojo::PendingReceiver<PageHandler> pending_receiver,
    mojo::PendingRemote<Page> pending_remote) {
  auto interface_impl = std::make_unique<InterfaceImpl>();
  auto* interface_impl_ptr = interface_impl.get();
  interface_impl->webui_controller_ = webui_controller;
  interface_impl->page_handler_receiver_.Bind(std::move(pending_receiver));
  interface_impl->page_remote_.Bind(std::move(pending_remote));
  internal::SaveWebUIManagedInterfaceInDocument(webui_controller,
                                                std::move(interface_impl));
  interface_impl_ptr->ready_ = true;
  interface_impl_ptr->OnReady();
}

template <typename InterfaceImpl, typename PageHandler>
void WebUIManagedInterface<InterfaceImpl, PageHandler>::Create(
    content::WebUIController* webui_controller,
    mojo::PendingReceiver<PageHandler> pending_receiver) {
  auto interface_impl = std::make_unique<InterfaceImpl>();
  auto* interface_impl_ptr = interface_impl.get();
  interface_impl->webui_controller_ = webui_controller;
  interface_impl->page_handler_receiver_.Bind(std::move(pending_receiver));
  internal::SaveWebUIManagedInterfaceInDocument(webui_controller,
                                                std::move(interface_impl));
  interface_impl_ptr->ready_ = true;
  interface_impl_ptr->OnReady();
}

template <typename InterfaceImpl, typename Page>
void WebUIManagedInterface<InterfaceImpl, void, Page>::Create(
    content::WebUIController* webui_controller,
    mojo::PendingRemote<Page> pending_remote) {
  auto interface_impl = std::make_unique<InterfaceImpl>();
  auto* interface_impl_ptr = interface_impl.get();
  interface_impl->webui_controller_ = webui_controller;
  interface_impl->page_remote_.Bind(std::move(pending_remote));
  internal::SaveWebUIManagedInterfaceInDocument(webui_controller,
                                                std::move(interface_impl));
  interface_impl_ptr->ready_ = true;
  interface_impl_ptr->OnReady();
}

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_MANAGED_INTERFACE_H_

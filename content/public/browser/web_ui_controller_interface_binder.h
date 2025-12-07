// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_INTERFACE_BINDER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_INTERFACE_BINDER_H_

#include <type_traits>

#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {
namespace internal {

template <typename Interface, int N, typename... Subclasses>
struct BinderHelper;

template <typename Interface, typename WebUIControllerSubclass, typename = void>
struct BinderTakesRenderFrameHost : std::false_type {};

template <typename Interface, typename WebUIControllerSubclass>
struct BinderTakesRenderFrameHost<
    Interface,
    WebUIControllerSubclass,
    decltype(std::declval<WebUIControllerSubclass>().BindInterface(
        std::declval<RenderFrameHost*>(),
        std::declval<mojo::PendingReceiver<Interface>>()))> : std::true_type {};

template <typename Interface, typename WebUIControllerSubclass>
bool SafeDownCastAndBindInterface(RenderFrameHost* host,
                                  mojo::PendingReceiver<Interface>& receiver) {
  // Performs a safe downcast to the concrete WebUIController subclass.
  WebUI* web_ui = host->GetWebUI();
  if (!web_ui) {
    return false;
  }

  auto* concrete_controller =
      web_ui->GetController()->GetAs<WebUIControllerSubclass>();
  if (!concrete_controller) {
    return false;
  }

  // Fails to compile if `Subclass` does not implement the appropriate overload
  // for `Interface`.
  if constexpr (BinderTakesRenderFrameHost<Interface,
                                           WebUIControllerSubclass>::value) {
    concrete_controller->BindInterface(host, std::move(receiver));
  } else {
    concrete_controller->BindInterface(std::move(receiver));
  }
  return true;
}

template <typename Interface, int N, typename Subclass, typename... Subclasses>
struct BinderHelper<Interface, N, std::tuple<Subclass, Subclasses...>> {
  static bool BindInterface(RenderFrameHost* host,
                            mojo::PendingReceiver<Interface> receiver) {
    if (SafeDownCastAndBindInterface<Interface, Subclass>(host, receiver)) {
      return true;
    }

    // Try a different subclass if the current one is not the right
    // WebUIController for the current WebUI page, and only fail if none of the
    // passed subclasses match.
    return BinderHelper<Interface, N - 1, std::tuple<Subclasses...>>::
        BindInterface(host, std::move(receiver));
  }
};

template <typename Interface, typename Subclass, typename... Subclasses>
struct BinderHelper<Interface, 0, std::tuple<Subclass, Subclasses...>> {
  static bool BindInterface(RenderFrameHost* host,
                            mojo::PendingReceiver<Interface> receiver) {
    return SafeDownCastAndBindInterface<Interface, Subclass>(host, receiver);
  }
};

void CONTENT_EXPORT ReceivedInvalidWebUIControllerMessage(RenderFrameHost* rfh);

}  // namespace internal

// Registers a binder in `map` that binds `Interface` iff the RenderFrameHost
// has a WebUIController among type `WebUIControllerSubclasses`.
template <typename Interface, typename... WebUIControllerSubclasses>
void RegisterWebUIControllerInterfaceBinder(
    mojo::BinderMapWithContext<RenderFrameHost*>* map) {
  DCHECK(!map->Contains<Interface>())
      << "A binder for " << Interface::Name_ << " has already been registered.";
  map->Add<Interface>(
      [](RenderFrameHost* host, mojo::PendingReceiver<Interface> receiver) {
        // This is expected to be called only for outermost main frames.
        if (host->GetParentOrOuterDocument()) {
          internal::ReceivedInvalidWebUIControllerMessage(host);
          return;
        }

        constexpr int size = sizeof...(WebUIControllerSubclasses);
        const bool is_bound =
            internal::BinderHelper<Interface, size - 1,
                                   std::tuple<WebUIControllerSubclasses...>>::
                BindInterface(host, std::move(receiver));
        if (is_bound) {
          return;
        }

        // This is expected to be called only for the right WebUI pages matching
        // the same WebUI associated to the RenderFrameHost.
        LOG(ERROR) << "WebUIController binder missing for: "
                   << Interface::Name_;
        internal::ReceivedInvalidWebUIControllerMessage(host);
      });
}

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_INTERFACE_BINDER_H_

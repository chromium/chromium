// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_INTERFACE_BINDER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_INTERFACE_BINDER_H_

#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {
namespace internal {

template <typename Interface, int N, typename... Subclasses>
struct BinderHelper;

template <typename Interface, typename WebUIControllerSubclass>
bool SafeDownCastAndBindInterface(WebUI* web_ui,
                                  mojo::PendingReceiver<Interface>& receiver) {
  // Performs a safe downcast to the concrete WebUIController subclass.
  WebUIControllerSubclass* concrete_controller =
      web_ui ? web_ui->GetController()->GetAs<WebUIControllerSubclass>()
             : nullptr;

  if (!concrete_controller)
    return false;

  // Fails to compile if |Subclass| does not implement the appropriate overload
  // for |Interface|.
  concrete_controller->BindInterface(std::move(receiver));
  return true;
}

template <typename Interface, int N, typename Subclass, typename... Subclasses>
struct BinderHelper<Interface, N, std::tuple<Subclass, Subclasses...>> {
  static bool BindInterface(WebUI* web_ui,
                            mojo::PendingReceiver<Interface> receiver) {
    // Try a different subclass if the current one is not the right
    // WebUIController for the current WebUI page, and only fail if none of the
    // passed subclasses match.
    if (!SafeDownCastAndBindInterface<Interface, Subclass>(web_ui, receiver)) {
      return BinderHelper<Interface, N - 1, std::tuple<Subclasses...>>::
          BindInterface(web_ui, std::move(receiver));
    }
    return true;
  }
};

template <typename Interface, typename Subclass, typename... Subclasses>
struct BinderHelper<Interface, 0, std::tuple<Subclass, Subclasses...>> {
  static bool BindInterface(WebUI* web_ui,
                            mojo::PendingReceiver<Interface> receiver) {
    return SafeDownCastAndBindInterface<Interface, Subclass>(web_ui, receiver);
  }
};

void CONTENT_EXPORT ReceivedInvalidWebUIControllerMessage(RenderFrameHost* rfh);

}  // namespace internal

// Registers a binder in |map| that binds |Interface| iff the RenderFrameHost
// has a WebUIController among type |WebUIControllerSubclasses|.
template <typename Interface, typename... WebUIControllerSubclasses>
void RegisterWebUIControllerInterfaceBinder(
    mojo::BinderMapWithContext<RenderFrameHost*>* map) {
  DCHECK(!map->Contains<Interface>())
      << "A binder for " << Interface::Name_ << " has already been registered.";
  map->Add<Interface>(base::BindRepeating(
      [](RenderFrameHost* host, mojo::PendingReceiver<Interface> receiver) {
        // This is expected to be called only for outermost main frames.
        if (host->GetParentOrOuterDocument()) {
          internal::ReceivedInvalidWebUIControllerMessage(host);
          return;
        }

        const int size = sizeof...(WebUIControllerSubclasses);
        bool is_bound =
            internal::BinderHelper<Interface, size - 1,
                                   std::tuple<WebUIControllerSubclasses...>>::
                BindInterface(host->GetWebUI(), std::move(receiver));

        // This is expected to be called only for the right WebUI pages matching
        // the same WebUI associated to the RenderFrameHost.
        if (!is_bound) {
          internal::ReceivedInvalidWebUIControllerMessage(host);
          return;
        }
      }));
}

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_INTERFACE_BINDER_H_

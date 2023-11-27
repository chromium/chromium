// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PER_WEB_UI_BROWSER_INTERFACE_BROKER_H_
#define CONTENT_PUBLIC_BROWSER_PER_WEB_UI_BROWSER_INTERFACE_BROKER_H_

#include "base/memory/raw_ref.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"

namespace content {

class WebUIController;
using WebUIBinderMap = mojo::BinderMapWithContext<content::WebUIController*>;
using BinderInitializer = base::RepeatingCallback<void(WebUIBinderMap* map)>;

// A browser interface broker that only binds the interface specified in
// binder_initializers. It should be owned by the |controller| in the
// constructor.
class PerWebUIBrowserInterfaceBroker
    : public blink::mojom::BrowserInterfaceBroker {
 public:
  PerWebUIBrowserInterfaceBroker(
      WebUIController& controller,
      const std::vector<BinderInitializer>& binder_initializers);
  ~PerWebUIBrowserInterfaceBroker() override;

  void GetInterface(mojo::GenericPendingReceiver receiver) override;

  [[nodiscard]] mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
  BindNewPipeAndPassRemote();

 private:
  const raw_ref<WebUIController> controller_;
  WebUIBinderMap binder_map_;
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> receiver_{this};
};
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PER_WEB_UI_BROWSER_INTERFACE_BROKER_H_

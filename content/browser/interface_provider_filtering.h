// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTERFACE_PROVIDER_FILTERING_H_
#define CONTENT_BROWSER_INTERFACE_PROVIDER_FILTERING_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"

namespace content {

// Filters interface requests received from an execution context of the type
// corresponding to |spec| in the renderer process with ID |process_id|.
// |receiver| is the PendingReceiver<InterfaceProvider> from the renderer; an
// equivalent PendingReceiver<InterfaceProvider> where GetInterface receivers
// have been filtered.
//
// If |process_id| does not refer to a renderer process or if that renderer's
// BrowserContext does not have a Connector, the connection is broken instead;
// that is, |receiver| and the mojo::PendingRemote<Interface> corresponding
// to the returned receiver are both closed.
mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
FilterRendererExposedInterfaces(
    const char* spec,
    int process_id,
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver);

namespace test {

// Allows through all interface requests while in scope. For testing only.
//
// TODO(https://crbug.com/792407): See if browser tests can just set up the
// service_Manager::Connector properly instead of this heavy-handed solution.
class CONTENT_EXPORT ScopedInterfaceFilterBypass {
 public:
  ScopedInterfaceFilterBypass();
  ~ScopedInterfaceFilterBypass();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedInterfaceFilterBypass);
};

}  // namespace test

}  // namespace content

#endif  // CONTENT_BROWSER_INTERFACE_PROVIDER_FILTERING_H_

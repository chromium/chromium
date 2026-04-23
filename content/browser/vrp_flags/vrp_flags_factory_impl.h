// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_VRP_FLAGS_VRP_FLAGS_FACTORY_IMPL_H_
#define CONTENT_BROWSER_VRP_FLAGS_VRP_FLAGS_FACTORY_IMPL_H_

#include "components/vrp_flags/vrp_flags.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

class CONTENT_EXPORT VrpFlagsFactoryImpl
    : public vrp_flags::mojom::VrpFlagsFactory {
 public:
  VrpFlagsFactoryImpl();
  ~VrpFlagsFactoryImpl() override;
  VrpFlagsFactoryImpl(const VrpFlagsFactoryImpl&) = delete;
  VrpFlagsFactoryImpl& operator=(const VrpFlagsFactoryImpl&) = delete;

  static void Bind(
      mojo::PendingReceiver<vrp_flags::mojom::VrpFlagsFactory> receiver);

  // mojom::VrpFlagsFactory:
  void BindBrowserVrpFlags(
      mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver) override;
  void BindNetworkVrpFlags(
      mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver) override;
  void BindGpuVrpFlags(
      mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver) override;

 private:
  mojo::ReceiverSet<vrp_flags::mojom::VrpFlagsFactory> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_VRP_FLAGS_VRP_FLAGS_FACTORY_IMPL_H_

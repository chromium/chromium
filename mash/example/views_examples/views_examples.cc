// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "mash/public/mojom/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/mus/aura_init.h"

class ViewsExamples : public service_manager::Service,
                      public mash::mojom::Launchable {
 public:
  ViewsExamples() {
    registry_.AddInterface<mash::mojom::Launchable>(
        base::Bind(&ViewsExamples::Create, base::Unretained(this)));
  }
  ~ViewsExamples() override = default;

 private:
  // service_manager::Service:
  void OnStart() override {
    views::AuraInit::InitParams params;
    params.connector = context()->connector();
    params.identity = context()->identity();
    aura_init_ = views::AuraInit::Create(params);
    if (!aura_init_)
      context()->QuitNow();
  }
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  // mash::mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override {
    views::examples::ShowExamplesWindow(
        base::BindOnce(&service_manager::ServiceContext::QuitNow,
                       base::Unretained(context())));
  }

  void Create(mash::mojom::LaunchableRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  mojo::BindingSet<mash::mojom::Launchable> bindings_;

  service_manager::BinderRegistry registry_;

  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(ViewsExamples);
};

MojoResult ServiceMain(MojoHandle service_request_handle) {
  return service_manager::ServiceRunner(new ViewsExamples)
      .Run(service_request_handle);
}

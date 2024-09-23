// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_CAST_COMPONENT_H_
#define FUCHSIA_WEB_RUNNERS_CAST_CAST_COMPONENT_H_

#include <chromium/cast/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/fuchsia/startup_context.h"
#include "base/gtest_prod_util.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_fuchsia.h"
#include "fuchsia_web/runners/cast/api_bindings_client.h"
#include "fuchsia_web/runners/cast/application_controller_impl.h"
#include "fuchsia_web/runners/cast/named_message_port_connector_fuchsia.h"
#include "fuchsia_web/runners/common/web_component.h"

FORWARD_DECLARE_TEST(HeadlessCastRunnerIntegrationTest, Headless);

// A specialization of WebComponent which adds Cast-specific services.
class CastComponent final
    : public WebComponent,
      public fuchsia::component::runner::ComponentController,
      public base::MessagePumpFuchsia::ZxHandleWatcher {
 public:
  struct Params {
    Params();
    Params(Params&&);
    ~Params();

    // Returns true if all parameters required for component launch have
    // been initialized.
    bool AreComplete() const;

    // Parameters populated directly from the StartComponent() arguments.
    std::unique_ptr<base::StartupContext> startup_context;
    fidl::InterfaceRequest<fuchsia::component::runner::ComponentController>
        controller_request;

    // Parameters initialized synchronously.
    chromium::cast::UrlRequestRewriteRulesProviderPtr
        url_rewrite_rules_provider;

    // Parameters asynchronously initialized by PendingCastComponent.
    std::unique_ptr<ApiBindingsClient> api_bindings_client;
    chromium::cast::ApplicationConfig application_config;
    fidl::ClientEnd<chromium_cast::ApplicationContext> application_context;
    std::optional<std::vector<fuchsia::web::UrlRequestRewriteRule>>
        initial_url_rewrite_rules;
    std::optional<fuchsia::web::FrameMediaSettings> media_settings;

    // ID of flow used in the with the Fuchsia Trace API to trace the
    // application lifetime.
    uint64_t trace_flow_id;
  };

  // See WebComponent documentation for details of `debug_name` and `runner`.
  // `debug_name` will be set on the underlying `Frame`, for use e.g. in log
  //   tagging.
  // `runner` must be non-null, and out-live `this`.
  // `params` provides the Cast application configuration to use.
  // `is_headless` must match the headless setting of the specified `runner`, to
  //   have CreateView() operations trigger enabling & disabling of off-screen
  //   rendering.
  CastComponent(std::string_view debug_name,
                WebContentRunner* runner,
                Params params,
                bool is_headless);

  CastComponent(const CastComponent&) = delete;
  CastComponent& operator=(const CastComponent&) = delete;

  ~CastComponent() override;

  // WebComponent overrides.
  void StartComponent() override;
  void DestroyComponent(int64_t exit_code) override;

 private:
  void OnRewriteRulesReceived(
      std::vector<fuchsia::web::UrlRequestRewriteRule> url_rewrite_rules);

  // fuchsia::web::NavigationEventListener implementation.
  // Triggers the injection of API channels into the page content.
  void OnNavigationStateChanged(
      fuchsia::web::NavigationState change,
      OnNavigationStateChangedCallback callback) override;

  // fuchsia::ui::app::ViewProvider implementation.
  void CreateViewWithViewRef(zx::eventpair view_token,
                             fuchsia::ui::views::ViewRefControl control_ref,
                             fuchsia::ui::views::ViewRef view_ref) override;
  void CreateView2(fuchsia::ui::app::CreateView2Args view_args) override;

  // fuchsia::component::runner::ComponentController implementation.
  void Kill() override;
  void Stop() override;

  // base::MessagePumpFuchsia::ZxHandleWatcher implementation.
  // Called when the headless "view" token is disconnected.
  void OnZxHandleSignalled(zx_handle_t handle, zx_signals_t signals) override;

  const bool is_headless_;

  chromium::cast::ApplicationConfig application_config_;
  chromium::cast::UrlRequestRewriteRulesProviderPtr url_rewrite_rules_provider_;
  std::vector<fuchsia::web::UrlRequestRewriteRule> initial_url_rewrite_rules_;

  bool constructor_active_ = false;
  std::unique_ptr<NamedMessagePortConnectorFuchsia> connector_;
  std::unique_ptr<ApiBindingsClient> api_bindings_client_;
  std::unique_ptr<ApplicationControllerImpl> application_controller_;
  fidl::Client<chromium_cast::ApplicationContext> application_context_;
  fuchsia::web::FrameMediaSettings media_settings_;
  zx::eventpair headless_view_token_;

  // Used by the Component Framework to control the component's lifetime.
  fidl::Binding<fuchsia::component::runner::ComponentController>
      component_controller_{this};

  base::MessagePumpForIO::ZxHandleWatchController headless_disconnect_watch_;

  uint64_t trace_flow_id_;
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_CAST_COMPONENT_H_

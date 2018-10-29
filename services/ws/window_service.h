// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_WINDOW_SERVICE_H_
#define SERVICES_WS_WINDOW_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/ws/ids.h"
#include "services/ws/ime/ime_driver_bridge.h"
#include "services/ws/ime/ime_registrar_impl.h"
#include "services/ws/input_devices/input_device_server.h"
#include "services/ws/public/mojom/event_injector.mojom.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "services/ws/public/mojom/remoting_event_injector.mojom.h"
#include "services/ws/public/mojom/user_activity_monitor.mojom.h"
#include "services/ws/public/mojom/window_server_test.mojom.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/base/mojo/clipboard.mojom.h"

namespace aura {
class Env;
class Window;
class WindowTreeHost;
namespace client {
class FocusClient;
}
}  // namespace aura

namespace base {
class UnguessableToken;
}

namespace display {
class Display;
}

namespace gfx {
class Insets;
}

namespace ui {
class ClipboardHost;
}

namespace ws {

class EventInjector;
class EventQueue;
class GpuInterfaceProvider;
class HostEventDispatcher;
class HostEventQueue;
class RemotingEventInjector;
class ScreenProvider;
class ServerWindow;
class UserActivityMonitor;
class WindowServiceDelegate;
class WindowServiceObserver;
class WindowTree;
class WindowTreeFactory;

namespace mojom {
class WindowTreeClient;
}

// WindowService is the entry point into providing an implementation of
// the mojom::WindowTree related mojoms on top of an aura Window hierarchy.
// A WindowTree is created for each client.
class COMPONENT_EXPORT(WINDOW_SERVICE) WindowService
    : public service_manager::Service {
 public:
  WindowService(WindowServiceDelegate* delegate,
                std::unique_ptr<GpuInterfaceProvider> gpu_support,
                aura::client::FocusClient* focus_client,
                bool decrement_client_ids = false,
                aura::Env* env = nullptr);
  ~WindowService() override;

  // Gets the ServerWindow for |window|, creating if necessary.
  ServerWindow* GetServerWindowForWindowCreateIfNecessary(aura::Window* window);

  // Creates a new WindowTree, caller must call one of the Init() functions on
  // the returned object.
  std::unique_ptr<WindowTree> CreateWindowTree(
      mojom::WindowTreeClient* window_tree_client,
      const std::string& client_name = std::string());

  // Sets the window frame metrics.
  void SetFrameDecorationValues(const gfx::Insets& client_area_insets,
                                int max_title_bar_button_width);

  // Sets the display to use for new windows. The window server broadcasts this
  // over mojo to all remote clients.
  void SetDisplayForNewWindows(int64_t display_id);

  // Whether |window| hosts a remote client.
  static bool HasRemoteClient(const aura::Window* window);

  // Returns true if |window| hosts a remote client and is a toplevel window.
  static bool IsTopLevelWindow(const aura::Window* window);

  struct TreeAndWindowId {
    ClientWindowId id;
    WindowTree* tree = nullptr;
  };
  // Returns the WindowTree that previously made a call to
  // ScheduleEmbedForExistingClient(). If a WindowTree made a call to
  // ScheduleEmbedForExistingClient() the |id| supplied by the client is
  // returned.
  TreeAndWindowId FindTreeWithScheduleEmbedForExistingClient(
      const base::UnguessableToken& embed_token);

  // Completes a previous call to ScheduleEmbedForExistingClient(). |window|
  // is the Window to perform the embedding in. See the mojom for details on
  // |embed_flags| and |embed_token|.
  bool CompleteScheduleEmbedForExistingClient(
      aura::Window* window,
      const base::UnguessableToken& embed_token,
      int embed_flags);

  void AddObserver(WindowServiceObserver* observer);
  void RemoveObserver(WindowServiceObserver* observer);
  base::ObserverList<WindowServiceObserver>::Unchecked& observers() {
    return observers_;
  }

  // Called when the surface of |client_name| is first activated.
  void OnFirstSurfaceActivation(const std::string& client_name);

  // Called when a WindowServiceClient is about to be destroyed.
  void OnWillDestroyWindowTree(WindowTree* tree);

  // Asks the client that created |window| to close |window|. Returns true if
  // |window| is a top-level created by a remote client, false otherwise.
  bool RequestClose(aura::Window* window);

  // Called when the metrics of a display changes. It is expected the local
  // environment call this *before* the change is applied to any
  // WindowTreeHosts. This is necessary to ensure clients are notified of the
  // change before the client is notified of a bounds change. To do otherwise
  // results in the client applying bounds change with the wrong scale factor.
  // |changed_metrics| is a bitmask of the DisplayObserver::DisplayMetric.
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics);

  // Registers the HostEventDispatcher for a WindowTreeHost and returns the
  // HostEventQueue that |window_tree_host| should use to dispatch events.
  std::unique_ptr<HostEventQueue> RegisterHostEventDispatcher(
      aura::WindowTreeHost* window_tree_host,
      HostEventDispatcher* dispatcher);

  // Returns an id useful for debugging. See ServerWindow::GetIdForDebugging()
  // for details.
  std::string GetIdForDebugging(aura::Window* window);

  aura::Env* env() { return env_; }

  ScreenProvider* screen_provider() { return screen_provider_.get(); }

  WindowServiceDelegate* delegate() { return delegate_; }

  aura::PropertyConverter* property_converter() { return &property_converter_; }

  aura::client::FocusClient* focus_client() { return focus_client_; }

  const std::set<WindowTree*>& window_trees() const { return window_trees_; }

  service_manager::BinderRegistry* registry() { return &registry_; }

  EventQueue* event_queue() { return event_queue_.get(); }

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle handle) override;

 private:
  friend class WindowServerTestImpl;

  // Sets a callback to be called whenever a surface is activated. This
  // corresponds to a client submitting a new CompositorFrame for a Window. This
  // should only be called in a test configuration.
  void SetSurfaceActivationCallback(
      base::OnceCallback<void(const std::string&)> callback);

  void BindClipboardHostRequest(ui::mojom::ClipboardHostRequest request);
  void BindImeRegistrarRequest(mojom::IMERegistrarRequest request);
  void BindImeDriverRequest(mojom::IMEDriverRequest request);
  void BindInputDeviceServerRequest(mojom::InputDeviceServerRequest request);
  void BindRemotingEventInjectorRequest(
      mojom::RemotingEventInjectorRequest request);
  void BindUserActivityMonitorRequest(
      mojom::UserActivityMonitorRequest request);
  void BindWindowServerTestRequest(mojom::WindowServerTestRequest request);

  void BindWindowTreeFactoryRequest(
      mojom::WindowTreeFactoryRequest request,
      const service_manager::BindSourceInfo& source_info);

  WindowServiceDelegate* delegate_;

  aura::Env* env_;

  // GpuInterfaceProvider may be null in tests.
  std::unique_ptr<GpuInterfaceProvider> gpu_interface_provider_;

  std::unique_ptr<ScreenProvider> screen_provider_;

  aura::client::FocusClient* focus_client_;

  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_with_source_info_;
  service_manager::BinderRegistry registry_;

  std::unique_ptr<UserActivityMonitor> user_activity_monitor_;

  std::unique_ptr<WindowTreeFactory> window_tree_factory_;

  // Helper used to serialize and deserialize window properties.
  aura::PropertyConverter property_converter_;

  // Provides info to InputDeviceClient users, via InputDeviceManager.
  InputDeviceServer input_device_server_;

  std::unique_ptr<EventInjector> event_injector_;
  std::unique_ptr<RemotingEventInjector> remoting_event_injector_;

  std::unique_ptr<ui::ClipboardHost> clipboard_host_;

  // Id for the next WindowTree.
  ClientSpecificId next_client_id_;

  // If true, client ids are decremented, not incremented; the default is false.
  const bool decrement_client_ids_;

  // Id used for the next window created locally that is exposed to clients.
  ClientSpecificId next_window_id_ = 1;

  IMERegistrarImpl ime_registrar_;
  IMEDriverBridge ime_driver_;

  // All WindowTrees created by the WindowService.
  std::set<WindowTree*> window_trees_;

  base::ObserverList<WindowServiceObserver>::Unchecked observers_;

  // Returns true if various test interfaces are exposed.
  bool test_config_ = false;

  std::unique_ptr<EventQueue> event_queue_;

  base::OnceCallback<void(const std::string&)> surface_activation_callback_;

  DISALLOW_COPY_AND_ASSIGN(WindowService);
};

}  // namespace ui

#endif  // SERVICES_WS_WINDOW_SERVICE_H_

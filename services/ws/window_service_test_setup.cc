// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_service_test_setup.h"

#include "services/ws/embedding.h"
#include "services/ws/event_queue.h"
#include "services/ws/event_queue_test_helper.h"
#include "services/ws/host_event_queue.h"
#include "services/ws/public/cpp/host/gpu_interface_provider.h"
#include "services/ws/test_host_event_dispatcher.h"
#include "services/ws/window_service.h"
#include "services/ws/window_tree.h"
#include "services/ws/window_tree_binding.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/display/screen.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ws {
namespace {

class TestFocusRules : public wm::BaseFocusRules {
 public:
  TestFocusRules() = default;
  ~TestFocusRules() override = default;

  // wm::BaseFocusRules:
  bool SupportsChildActivation(aura::Window* window) const override {
    return window == window->GetRootWindow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFocusRules);
};

// EventTargeterWs sole purpose is to make OnEventFromSource() forward events
// to the appropriate HostEventQueue. This is normally done by a WindowTreeHost
// subclass, but because tests create a platform specific WindowTreeHost
// implementation, that isn't possible.
class EventTargeterWs : public ui::EventTarget,
                        public ui::EventTargeter,
                        public ui::EventSource,
                        public ui::EventSink {
 public:
  EventTargeterWs(WindowServiceTestSetup* test_setup,
                  HostEventQueue* host_event_queue)
      : test_setup_(test_setup), host_event_queue_(host_event_queue) {}

  ~EventTargeterWs() override = default;

  // ui::EventTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    return this;
  }
  ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                      ui::Event* event) override {
    return this;
  }

  // ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override { return true; }
  ui::EventTarget* GetParentTarget() override { return nullptr; }
  std::unique_ptr<ui::EventTargetIterator> GetChildIterator() const override {
    return nullptr;
  }
  ui::EventTargeter* GetEventTargeter() override { return this; }

  // ui::EventSource:
  ui::EventSink* GetEventSink() override { return this; }

  // ui::EventSink:
  ui::EventDispatchDetails OnEventFromSource(ui::Event* event) override {
    host_event_queue_->DispatchOrQueueEvent(event);
    WindowService* window_service = test_setup_->service();
    if (test_setup_->ack_events_immediately() &&
        EventQueueTestHelper(window_service->event_queue())
            .HasInFlightEvent()) {
      EventQueueTestHelper(window_service->event_queue()).AckInFlightEvent();
    }
    return ui::EventDispatchDetails();
  }

 private:
  WindowServiceTestSetup* test_setup_;
  HostEventQueue* host_event_queue_;

  DISALLOW_COPY_AND_ASSIGN(EventTargeterWs);
};

// EventGeneratorDelegate implementation for mus.
class EventGeneratorDelegateWs : public aura::test::EventGeneratorDelegateAura {
 public:
  EventGeneratorDelegateWs(WindowServiceTestSetup* test_setup,
                           HostEventQueue* host_event_queue)
      : event_targeter_(test_setup, host_event_queue) {}
  ~EventGeneratorDelegateWs() override = default;

  // EventGeneratorDelegateAura:
  ui::EventTarget* GetTargetAt(const gfx::Point& location) override {
    return &event_targeter_;
  }
  aura::client::ScreenPositionClient* GetScreenPositionClient(
      const aura::Window* window) const override {
    return aura::client::GetScreenPositionClient(window->GetRootWindow());
  }
  ui::EventSource* GetEventSource(ui::EventTarget* target) override {
    return target == &event_targeter_
               ? &event_targeter_
               : EventGeneratorDelegateAura::GetEventSource(target);
  }
  gfx::Point CenterOfTarget(const ui::EventTarget* target) const override {
    if (target != &event_targeter_)
      return EventGeneratorDelegateAura::CenterOfTarget(target);
    return display::Screen::GetScreen()
        ->GetPrimaryDisplay()
        .bounds()
        .CenterPoint();
  }
  void ConvertPointFromTarget(const ui::EventTarget* target,
                              gfx::Point* point) const override {
    if (target != &event_targeter_)
      EventGeneratorDelegateAura::ConvertPointFromTarget(target, point);
  }
  void ConvertPointToTarget(const ui::EventTarget* target,
                            gfx::Point* point) const override {
    if (target != &event_targeter_)
      EventGeneratorDelegateAura::ConvertPointToTarget(target, point);
  }
  void ConvertPointFromHost(const ui::EventTarget* hosted_target,
                            gfx::Point* point) const override {
    if (hosted_target != &event_targeter_)
      EventGeneratorDelegateAura::ConvertPointFromHost(hosted_target, point);
  }

 private:
  EventTargeterWs event_targeter_;

  DISALLOW_COPY_AND_ASSIGN(EventGeneratorDelegateWs);
};

std::unique_ptr<ui::test::EventGeneratorDelegate> CreateEventGeneratorDelegate(
    WindowServiceTestSetup* test_setup,
    ui::test::EventGenerator* owner,
    aura::Window* root_window,
    aura::Window* window) {
  DCHECK(root_window);
  DCHECK(root_window->GetHost());
  return std::make_unique<EventGeneratorDelegateWs>(
      test_setup,
      test_setup->service()->event_queue()->FindHostEventQueueForWindowTreeHost(
          root_window->GetHost()));
}

}  // namespace

WindowServiceTestSetup::WindowServiceTestSetup()
    // FocusController takes ownership of TestFocusRules.
    : focus_controller_(new TestFocusRules()) {
  DCHECK_EQ(gl::kGLImplementationNone, gl::GetGLImplementation());
  gl::GLSurfaceTestSupport::InitializeOneOff();

  ui::ContextFactory* context_factory = nullptr;
  ui::ContextFactoryPrivate* context_factory_private = nullptr;
  const bool enable_pixel_output = false;
  ui::InitializeContextFactoryForTests(enable_pixel_output, &context_factory,
                                       &context_factory_private);
  aura_test_helper_.SetUp(context_factory, context_factory_private);
  scoped_capture_client_ = std::make_unique<wm::ScopedCaptureClient>(
      aura_test_helper_.root_window());
  service_ =
      std::make_unique<WindowService>(&delegate_, nullptr, focus_controller());
  aura::client::SetFocusClient(root(), focus_controller());
  wm::SetActivationClient(root(), focus_controller());
  delegate_.set_top_level_parent(aura_test_helper_.root_window());
  host_event_dispatcher_ =
      std::make_unique<TestHostEventDispatcher>(aura_test_helper_.host());
  host_event_queue_ = service_->RegisterHostEventDispatcher(
      aura_test_helper_.host(), host_event_dispatcher_.get());

  window_tree_ = service_->CreateWindowTree(&window_tree_client_);
  window_tree_->InitFromFactory();
  window_tree_test_helper_ =
      std::make_unique<WindowTreeTestHelper>(window_tree_.get());

  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      base::BindRepeating(&CreateEventGeneratorDelegate, this));
}

WindowServiceTestSetup::~WindowServiceTestSetup() {
  window_tree_test_helper_.reset();
  window_tree_.reset();
  service_.reset();
  scoped_capture_client_.reset();
  aura::client::SetFocusClient(root(), nullptr);
  aura_test_helper_.TearDown();
  ui::TerminateContextFactoryForTests();
  gl::GLSurfaceTestSupport::ShutdownGL();
  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      ui::test::EventGeneratorDelegate::FactoryFunction());
}

std::unique_ptr<EmbeddingHelper> WindowServiceTestSetup::CreateEmbedding(
    aura::Window* embed_root,
    uint32_t flags) {
  auto embedding_helper = std::make_unique<EmbeddingHelper>();
  embedding_helper->embedding = window_tree_test_helper_->Embed(
      embed_root, nullptr, &embedding_helper->window_tree_client, flags);
  if (!embedding_helper->embedding)
    return nullptr;
  embedding_helper->window_tree = embedding_helper->embedding->embedded_tree();
  embedding_helper->window_tree_test_helper =
      std::make_unique<WindowTreeTestHelper>(embedding_helper->window_tree);
  embedding_helper->parent_window_tree =
      embedding_helper->embedding->embedding_tree();
  return embedding_helper;
}

EmbeddingHelper::EmbeddingHelper() = default;

EmbeddingHelper::~EmbeddingHelper() {
  if (!embedding)
    return;

  WindowTreeTestHelper(parent_window_tree).DestroyEmbedding(embedding);
}

}  // namespace ws

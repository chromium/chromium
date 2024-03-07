// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_TEST_EVENT_ROUTER_H_

#include <map>
#include <memory>
#include <string>
#include <type_traits>

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// An EventRouter that tests can use to observe, await, or verify events
// dispatched to an extension.
class TestEventRouter : public EventRouter {
 public:
  // Allows observing events dispatched to the event router.
  class EventObserver {
   public:
    // These functions correspond to the ones in EventRouter.
    virtual void OnBroadcastEvent(const Event& event);
    virtual void OnDispatchEventToExtension(const ExtensionId& extension_id,
                                            const Event& event);

   protected:
    virtual ~EventObserver();
  };

  explicit TestEventRouter(content::BrowserContext* context);

  TestEventRouter(const TestEventRouter&) = delete;
  TestEventRouter& operator=(const TestEventRouter&) = delete;

  ~TestEventRouter() override;

  // Returns the number of times an event has been broadcast or dispatched.
  int GetEventCount(std::string event_name) const;

  void AddEventObserver(EventObserver* obs);
  void RemoveEventObserver(EventObserver* obs);

  // Sets the extension ID all dispatched events will be expected to be sent to.
  void set_expected_extension_id(const ExtensionId& extension_id) {
    expected_extension_id_ = extension_id;
  }

  // EventRouter:
  void BroadcastEvent(std::unique_ptr<Event> event) override;
  void DispatchEventToExtension(const ExtensionId& extension_id,
                                std::unique_ptr<Event> event) override;

 private:
  // Increments the count of dispatched events seen with the given name.
  void IncrementEventCount(const std::string& event_name);

  ExtensionId expected_extension_id_;

  // Count of dispatched and broadcasted events by event name.
  std::map<std::string, int> seen_events_;

  base::ObserverList<EventObserver, false>::UncheckedAndDanglingUntriaged
      observers_;
};

// Creates and enables a TestEventRouter for testing. Callers can override T to
// provide a derived event router.
template <typename T = TestEventRouter>
T* CreateAndUseTestEventRouter(content::BrowserContext* context) {
  // The factory function only requires that T be a KeyedService. Ensure it is
  // actually derived from EventRouter to avoid undefined behavior.
  static_assert(std::is_base_of<EventRouter, T>(),
                "T must be derived from EventRouter");
  return static_cast<T*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          context, base::BindRepeating([](content::BrowserContext* context) {
            return static_cast<std::unique_ptr<KeyedService>>(
                std::make_unique<T>(context));
          })));
}

}  // namespace extensions

namespace base {

template <>
struct ScopedObservationTraits<extensions::TestEventRouter,
                               extensions::TestEventRouter::EventObserver> {
  static void AddObserver(
      extensions::TestEventRouter* source,
      extensions::TestEventRouter::EventObserver* observer) {
    source->AddEventObserver(observer);
  }
  static void RemoveObserver(
      extensions::TestEventRouter* source,
      extensions::TestEventRouter::EventObserver* observer) {
    source->RemoveEventObserver(observer);
  }
};

}  // namespace base

#endif  // EXTENSIONS_BROWSER_TEST_EVENT_ROUTER_H_

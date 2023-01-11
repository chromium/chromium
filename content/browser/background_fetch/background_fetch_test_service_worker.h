// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_SERVICE_WORKER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_SERVICE_WORKER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {

// Extension of the FakeServiceWorker that enables instrumentation of the
// events related to the Background Fetch API. Storage for these tests will
// always be kept in memory, as data persistence is tested elsewhere.
class BackgroundFetchTestServiceWorker : public FakeServiceWorker {
 public:
  explicit BackgroundFetchTestServiceWorker(EmbeddedWorkerTestHelper* helper);

  BackgroundFetchTestServiceWorker(const BackgroundFetchTestServiceWorker&) =
      delete;
  BackgroundFetchTestServiceWorker& operator=(
      const BackgroundFetchTestServiceWorker&) = delete;

  ~BackgroundFetchTestServiceWorker() override;

  // Toggles whether the named Service Worker event should fail.
  void set_fail_abort_event(bool fail) { fail_abort_event_ = fail; }
  void set_fail_click_event(bool fail) { fail_click_event_ = fail; }
  void set_fail_fetch_fail_event(bool fail) { fail_fetch_fail_event_ = fail; }
  void set_fail_fetched_event(bool fail) { fail_fetched_event_ = fail; }
  void delay_dispatch() { delay_dispatch_ = true; }

  // Sets a callback that should be executed when the next named event is
  // ran.
  void set_abort_event_closure(base::OnceClosure closure) {
    abort_event_closure_ = std::move(closure);
  }
  void set_click_event_closure(base::OnceClosure closure) {
    click_event_closure_ = std::move(closure);
  }
  void set_fetch_fail_event_closure(base::OnceClosure closure) {
    fetch_fail_event_closure_ = std::move(closure);
  }
  void set_fetched_event_closure(base::OnceClosure closure) {
    fetched_event_closure_ = std::move(closure);
  }

  const blink::mojom::BackgroundFetchRegistrationDataPtr& last_registration()
      const {
    DCHECK(last_registration_);
    return last_registration_->registration_data;
  }

 protected:
  // FakeServiceWorker overrides:
  void DispatchBackgroundFetchAbortEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchAbortEventCallback
          callback) override;

  void DispatchBackgroundFetchClickEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchClickEventCallback
          callback) override;

  void DispatchBackgroundFetchFailEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchFailEventCallback
          callback) override;

  void DispatchBackgroundFetchSuccessEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      blink::mojom::ServiceWorker::DispatchBackgroundFetchSuccessEventCallback
          callback) override;

 private:
  bool fail_abort_event_ = false;
  bool fail_click_event_ = false;
  bool fail_fetch_fail_event_ = false;
  bool fail_fetched_event_ = false;

  // Delays the dispatch event until the end of the test.
  bool delay_dispatch_ = false;
  base::OnceClosure delayed_closure_;

  base::OnceClosure abort_event_closure_;
  base::OnceClosure click_event_closure_;
  base::OnceClosure fetch_fail_event_closure_;
  base::OnceClosure fetched_event_closure_;

  blink::mojom::BackgroundFetchRegistrationPtr last_registration_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_SERVICE_WORKER_H_

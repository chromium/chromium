// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SCRIPT_RESULT_QUEUE_H_
#define EXTENSIONS_BROWSER_SCRIPT_RESULT_QUEUE_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "extensions/browser/api/test/test_api_observer.h"
#include "extensions/browser/api/test/test_api_observer_registry.h"

namespace extensions {

// Intercepts results sent via chrome.test.sendScriptResult().
// TODO(devlin): Add details of this class and sendScriptResult() to
// //extensions/docs/extension_tests.md.
class ScriptResultQueue : public TestApiObserver {
 public:
  ScriptResultQueue();
  ~ScriptResultQueue() override;

  // TestApiObserver:
  void OnScriptResult(const base::Value& script_result) override;

  // Returns the next result, optionally waiting for it to come in.
  base::Value GetNextResult();

 private:
  // The index of the next result to return.
  size_t next_result_index_ = 0u;

  // The collection of all script results this queue has seen.
  base::Value::List results_;

  // Quit closure to call when waiting for a result.
  base::OnceClosure quit_closure_;

  base::ScopedObservation<TestApiObserverRegistry, TestApiObserver>
      test_api_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SCRIPT_RESULT_QUEUE_H_

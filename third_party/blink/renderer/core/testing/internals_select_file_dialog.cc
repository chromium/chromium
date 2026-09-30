// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/internals_select_file_dialog.h"

#include <utility>

#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/test/mojom/select_file_dialog/select_file_dialog_automation.test-mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
ScriptPromise<IDLUndefined>
InternalsSelectFileDialog::setSelectFileDialogResult(
    ScriptState* script_state,
    Internals&,
    const std::optional<Vector<String>>& paths,
    ExceptionState& exception_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  CHECK(window);
  CHECK(script_state->ContextIsValid());

  if (paths.has_value() && paths->empty()) {
    exception_state.ThrowTypeError(
        "paths sequence cannot be empty. Pass null to simulate cancellation.");
    return EmptyPromise();
  }

  mojo::Remote<test::mojom::blink::SelectFileDialogAutomation> automation;
  window->GetBrowserInterfaceBroker().GetInterface(
      automation.BindNewPipeAndPassReceiver());
  CHECK(automation.is_bound());

  std::optional<Vector<base::FilePath>> file_paths;
  if (paths.has_value()) {
    file_paths.emplace();
    file_paths->reserve(paths->size());
    for (const String& path : *paths) {
      file_paths->push_back(StringToFilePath(path));
    }
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  auto* raw_automation = automation.get();
  raw_automation->SetSelectFileDialogResult(
      std::move(file_paths),
      BindOnce(
          [](ScriptPromiseResolver<IDLUndefined>* resolver,
             mojo::Remote<test::mojom::blink::SelectFileDialogAutomation>) {
            resolver->Resolve();
          },
          WrapPersistent(resolver),
          // Keep `automation` alive to wait for callback.
          std::move(automation)));

  return promise;
}

}  // namespace blink

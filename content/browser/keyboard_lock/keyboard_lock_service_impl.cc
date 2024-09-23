// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/keyboard_lock/keyboard_lock_service_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/keyboard_lock/keyboard_lock_metrics.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

using blink::mojom::GetKeyboardLayoutMapResult;
using blink::mojom::KeyboardLockRequestResult;

namespace content {

namespace {

void LogKeyboardLockMethodCalled(KeyboardLockMethods method) {
  UMA_HISTOGRAM_ENUMERATION(kKeyboardLockMethodCalledHistogramName, method,
                            KeyboardLockMethods::kCount);
}

}  // namespace

KeyboardLockServiceImpl::KeyboardLockServiceImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::KeyboardLockService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

// static
void KeyboardLockServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::KeyboardLockService> receiver) {
  CHECK(render_frame_host);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new KeyboardLockServiceImpl(*render_frame_host, std::move(receiver));
}

void KeyboardLockServiceImpl::RequestKeyboardLock(
    const std::vector<std::string>& key_codes,
    RequestKeyboardLockCallback callback) {
  if (key_codes.empty()) {
    LogKeyboardLockMethodCalled(KeyboardLockMethods::kRequestAllKeys);
  } else {
    LogKeyboardLockMethodCalled(KeyboardLockMethods::kRequestSomeKeys);
  }
  if (render_frame_host().GetParentOrOuterDocument()) {
    std::move(callback).Run(KeyboardLockRequestResult::kChildFrameError);
    return;
  }
  if (!render_frame_host().IsActive()) {
    std::move(callback).Run(KeyboardLockRequestResult::kFrameDetachedError);
    return;
  }

  // Per base::flat_set usage notes, the proper way to init a flat_set is
  // inserting into a vector and using that to init the flat_set.
  std::vector<ui::DomCode> dom_codes;
  bool invalid_key_code_found = false;
  for (const std::string& code : key_codes) {
    ui::DomCode dom_code = ui::KeycodeConverter::CodeStringToDomCode(code);
    if (dom_code != ui::DomCode::NONE) {
      dom_codes.push_back(dom_code);
    } else {
      invalid_key_code_found = true;
      render_frame_host().AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "Invalid DOMString passed into keyboard.lock(): '" + code + "'");
    }
  }

  auto& frame_host_impl =
      static_cast<RenderFrameHostImpl&>(render_frame_host());

  // If we are provided with a vector containing one or more invalid key codes,
  // then exit without enabling keyboard lock.  Also cancel any previous
  // keyboard lock request since the most recent request failed.
  if (invalid_key_code_found) {
    frame_host_impl.GetRenderWidgetHost()->CancelKeyboardLock();
    std::move(callback).Run(KeyboardLockRequestResult::kNoValidKeyCodesError);
    return;
  }

  std::optional<base::flat_set<ui::DomCode>> dom_code_set;
  if (!dom_codes.empty()) {
    dom_code_set = std::move(dom_codes);
  }
  frame_host_impl.GetRenderWidgetHost()->RequestKeyboardLock(
      std::move(dom_code_set),
      // We use a lambda function here instead of binding to a method because
      // we want the Mojo callback to be called even if `this` goes out of
      // scope.
      base::BindOnce(
          [](base::WeakPtr<KeyboardLockServiceImpl> weak_this,
             RequestKeyboardLockCallback callback,
             blink::mojom::KeyboardLockRequestResult result) {
            std::move(callback).Run(result);
            if (weak_this &&
                result == blink::mojom::KeyboardLockRequestResult::kSuccess) {
              weak_this->feature_handle_ =
                  static_cast<RenderFrameHostImpl&>(
                      weak_this->render_frame_host())
                      .RegisterBackForwardCacheDisablingNonStickyFeature(
                          blink::scheduler::WebSchedulerTrackedFeature::
                              kKeyboardLock);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void KeyboardLockServiceImpl::CancelKeyboardLock() {
  LogKeyboardLockMethodCalled(KeyboardLockMethods::kCancelLock);
  auto& frame_host_impl =
      static_cast<RenderFrameHostImpl&>(render_frame_host());
  frame_host_impl.GetRenderWidgetHost()->CancelKeyboardLock();
  feature_handle_.reset();
}

void KeyboardLockServiceImpl::GetKeyboardLayoutMap(
    GetKeyboardLayoutMapCallback callback) {
  auto& frame_host_impl =
      static_cast<RenderFrameHostImpl&>(render_frame_host());

  auto response = GetKeyboardLayoutMapResult::New();
  // The keyboard layout map is only accessible from the outermost main frame or
  // with the permission policy enabled.
  if (frame_host_impl.GetParentOrOuterDocument() &&
      !frame_host_impl.IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kKeyboardMap)) {
    response->status = blink::mojom::GetKeyboardLayoutMapStatus::kDenied;
    std::move(callback).Run(std::move(response));
    return;
  }
  response->status = blink::mojom::GetKeyboardLayoutMapStatus::kSuccess;
  response->layout_map = frame_host_impl.GetPage().GetKeyboardLayoutMap();

  std::move(callback).Run(std::move(response));
}

KeyboardLockServiceImpl::~KeyboardLockServiceImpl() = default;

}  // namespace content

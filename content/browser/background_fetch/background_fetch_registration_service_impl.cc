// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_registration_service_impl.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_metrics.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/bad_message.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace content {

namespace {

// Maximum length of a developer-provided title for a Background Fetch.
constexpr size_t kMaxTitleLength = 1024 * 1024;

}  // namespace

// static
mojo::PendingRemote<blink::mojom::BackgroundFetchRegistrationService>
BackgroundFetchRegistrationServiceImpl::CreateInterfaceInfo(
    BackgroundFetchRegistrationId registration_id,
    base::WeakPtr<BackgroundFetchContext> background_fetch_context) {
  DCHECK(background_fetch_context);

  mojo::PendingRemote<blink::mojom::BackgroundFetchRegistrationService>
      mojo_interface;

  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new BackgroundFetchRegistrationServiceImpl(
          std::move(registration_id), std::move(background_fetch_context))),
      mojo_interface.InitWithNewPipeAndPassReceiver());

  return mojo_interface;
}

BackgroundFetchRegistrationServiceImpl::BackgroundFetchRegistrationServiceImpl(
    BackgroundFetchRegistrationId registration_id,
    base::WeakPtr<BackgroundFetchContext> background_fetch_context)
    : registration_id_(std::move(registration_id)),
      background_fetch_context_(std::move(background_fetch_context)) {
  DCHECK(background_fetch_context_);
  DCHECK(!registration_id_.is_null());
}

BackgroundFetchRegistrationServiceImpl::
    ~BackgroundFetchRegistrationServiceImpl() = default;

void BackgroundFetchRegistrationServiceImpl::MatchRequests(
    blink::mojom::FetchAPIRequestPtr request_to_match,
    blink::mojom::CacheQueryOptionsPtr cache_query_options,
    bool match_all,
    MatchRequestsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!background_fetch_context_) {
    // Return without running the callback because this case happens only when
    // the browser is shutting down.
    return;
  }

  // Create BackgroundFetchMatchRequestMatchParams.
  auto match_params = std::make_unique<BackgroundFetchRequestMatchParams>(
      std::move(request_to_match), std::move(cache_query_options), match_all);

  background_fetch_context_->MatchRequests(
      registration_id_, std::move(match_params), std::move(callback));
}

void BackgroundFetchRegistrationServiceImpl::UpdateUI(
    const std::optional<std::string>& title,
    const SkBitmap& icon,
    UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!background_fetch_context_) {
    // Return without running the callback because this case happens only when
    // the browser is shutting down.
    return;
  }

  if (title && !ValidateTitle(*title)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    return;
  }

  // Wrap the icon in an optional for clarity.
  auto optional_icon =
      icon.isNull() ? std::nullopt : std::optional<SkBitmap>(icon);

  background_fetch_context_->UpdateUI(registration_id_, title, optional_icon,
                                      std::move(callback));
}

void BackgroundFetchRegistrationServiceImpl::Abort(AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!background_fetch_context_) {
    // Return without running the callback because this case happens only when
    // the browser is shutting down.
    return;
  }
  background_fetch_context_->Abort(registration_id_, std::move(callback));
}

void BackgroundFetchRegistrationServiceImpl::AddRegistrationObserver(
    mojo::PendingRemote<blink::mojom::BackgroundFetchRegistrationObserver>
        observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!background_fetch_context_)
    return;
  background_fetch_context_->AddRegistrationObserver(
      registration_id_.unique_id(), std::move(observer));
}

bool BackgroundFetchRegistrationServiceImpl::ValidateTitle(
    const std::string& title) {
  if (title.empty() || title.size() > kMaxTitleLength) {
    receiver_.ReportBadMessage("Invalid title");
    return false;
  }

  return true;
}

}  // namespace content

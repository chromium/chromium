// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/installedapp/native_win_app_fetcher_impl.h"

#include <windows.foundation.h>

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/win/async_operation.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/vector.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

namespace content {

namespace {
constexpr char kWindowsPlatformName[] = "windows";

using ABI::Windows::ApplicationModel::AppInfo;
using ABI::Windows::ApplicationModel::IAppInfo;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using ABI::Windows::Foundation::IUriRuntimeClass;
using ABI::Windows::Foundation::IUriRuntimeClassFactory;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::System::ILauncherStatics4;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
}  // namespace

NativeWinAppFetcherImpl::NativeWinAppFetcherImpl() = default;
NativeWinAppFetcherImpl::~NativeWinAppFetcherImpl() = default;

void NativeWinAppFetcherImpl::FetchAppsForUrl(
    const GURL& url,
    base::OnceCallback<void(std::vector<blink::mojom::RelatedApplicationPtr>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = std::move(callback);

  ComPtr<IUriRuntimeClassFactory> url_factory;
  HRESULT hr =
      base::win::GetActivationFactory<IUriRuntimeClassFactory,
                                      RuntimeClass_Windows_Foundation_Uri>(
          &url_factory);
  if (FAILED(hr)) {
    return OnFailure();
  }

  ComPtr<IUriRuntimeClass> url_win;
  hr = url_factory->CreateUri(
      base::win::ScopedHString::Create(url.spec()).get(), &url_win);
  if (FAILED(hr)) {
    return OnFailure();
  }

  ComPtr<ILauncherStatics4> launcher;
  hr = base::win::GetActivationFactory<ILauncherStatics4,
                                       RuntimeClass_Windows_System_Launcher>(
      &launcher);
  if (FAILED(hr)) {
    return OnFailure();
  }

  // FindAppUriHandlersAsync API returns list of apps that is validated
  // by the sites.
  // https://docs.microsoft.com/en-us/windows/uwp/
  // launch-resume/web-to-app-linking
  hr = launcher->FindAppUriHandlersAsync(url_win.Get(), &enum_operation_);
  if (FAILED(hr)) {
    return OnFailure();
  }

  base::win::PostAsyncHandlers(
      enum_operation_.Get(),
      base::BindOnce(&NativeWinAppFetcherImpl::OnGetAppUriHandlers,
                     weak_ptr_factory_.GetWeakPtr())),
      base::BindOnce(&NativeWinAppFetcherImpl::OnFailure,
                     weak_ptr_factory_.GetWeakPtr());
}

void NativeWinAppFetcherImpl::OnGetAppUriHandlers(
    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVectorView<
        ABI::Windows::ApplicationModel::AppInfo*>> found_app_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<blink::mojom::RelatedApplicationPtr> found_related_apps;

  if (!found_app_list) {
    return OnFailure();
  }

  UINT found_app_url_size = 0;
  HRESULT hr = found_app_list->get_Size(&found_app_url_size);
  if (FAILED(hr) || found_app_url_size == 0) {
    return OnFailure();
  }

  for (size_t i = 0; i < found_app_url_size; ++i) {
    ComPtr<IAppInfo> app_info;
    hr = found_app_list->GetAt(i, &app_info);
    if (FAILED(hr)) {
      continue;
    }

    HSTRING app_user_model_id_native;
    hr = app_info->get_AppUserModelId(&app_user_model_id_native);
    if (FAILED(hr)) {
      continue;
    }

    std::wstring app_user_model_id(
        base::win::ScopedHString(app_user_model_id_native).Get());

    auto application = blink::mojom::RelatedApplication::New();
    application->platform = kWindowsPlatformName;
    application->id = base::WideToASCII(app_user_model_id);
    found_related_apps.push_back(std::move(application));
  }

  CHECK(callback_);
  return std::move(callback_).Run(std::move(found_related_apps));
}

void NativeWinAppFetcherImpl::OnFailure() {
  return std::move(callback_).Run(
      std::vector<blink::mojom::RelatedApplicationPtr>());
}

}  // namespace content

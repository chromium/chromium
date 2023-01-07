// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Windows.ApplicationModel.h>
#include <Windows.System.h>
#include <combaseapi.h>
#include <windows.foundation.collections.h>
#include <wrl/async.h>

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "content/browser/installedapp/installed_app_provider_impl_win.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "url/gurl.h"

namespace content {
namespace installed_app_provider_win {

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

namespace {

constexpr size_t kMaxAllowedRelatedApps = 3;
constexpr char kWindowsPlatformName[] = "windows";

void OnGetAppUrlHandlers(
    std::vector<blink::mojom::RelatedApplicationPtr> related_apps,
    blink::mojom::InstalledAppProvider::FilterInstalledAppsCallback callback,
    ComPtr<IVectorView<AppInfo*>> found_app_list) {
  std::vector<blink::mojom::RelatedApplicationPtr> found_installed_apps;

  if (!found_app_list) {
    // |found_app_list| can be null when returned from the OS.
    std::move(callback).Run(std::move(found_installed_apps));
    return;
  }

  UINT found_app_url_size = 0;
  HRESULT hr = found_app_list->get_Size(&found_app_url_size);
  if (FAILED(hr) || found_app_url_size == 0) {
    std::move(callback).Run(std::move(found_installed_apps));
    return;
  }

  for (size_t i = 0; i < found_app_url_size; ++i) {
    ComPtr<IAppInfo> app_info;
    hr = found_app_list->GetAt(i, &app_info);
    if (FAILED(hr))
      continue;

    HSTRING app_user_model_id_native;
    hr = app_info->get_AppUserModelId(&app_user_model_id_native);
    if (FAILED(hr))
      continue;

    std::wstring app_user_model_id(
        base::win::ScopedHString(app_user_model_id_native).Get());

    size_t windows_app_count = 0;
    for (size_t j = 0; j < related_apps.size(); ++j) {
      auto& related_app = related_apps[j];

      // v1 supports only 'windows' platform name.
      if (related_app->platform != kWindowsPlatformName)
        continue;

      // It iterates only max 3 windows related apps.
      if (++windows_app_count > kMaxAllowedRelatedApps)
        break;

      if (!related_app->id.has_value())
        continue;

      // alphanumeric AppModelUerId.
      // https://docs.microsoft.com/en-us/uwp/schemas/
      // appinstallerschema/element-package
      if (base::CompareCaseInsensitiveASCII(
              related_app->id.value(), base::WideToASCII(app_user_model_id)) ==
          0) {
        auto application = blink::mojom::RelatedApplication::New();
        application->platform = related_app->platform;
        application->id = related_app->id.value();
        if (related_app->url.has_value())
          application->url = related_app->url.value();

        found_installed_apps.push_back(std::move(application));
      }
    }
  }
  std::move(callback).Run(std::move(found_installed_apps));
}

}  // namespace

void FilterInstalledAppsForWin(
    std::vector<blink::mojom::RelatedApplicationPtr> related_apps,
    blink::mojom::InstalledAppProvider::FilterInstalledAppsCallback callback,
    const GURL frame_url) {
  if (!base::win::ScopedHString::ResolveCoreWinRTStringDelayload() ||
      !base::win::ResolveCoreWinRTDelayload()) {
    std::move(callback).Run(std::vector<blink::mojom::RelatedApplicationPtr>());
    return;
  }

  ComPtr<ILauncherStatics4> launcher_statics;
  HRESULT hr = base::win::RoActivateInstance(
      base::win::ScopedHString::Create(RuntimeClass_Windows_System_Launcher)
          .get(),
      &launcher_statics);

  ComPtr<IUriRuntimeClassFactory> url_factory;
  hr = base::win::GetActivationFactory<IUriRuntimeClassFactory,
                                       RuntimeClass_Windows_Foundation_Uri>(
      &url_factory);
  if (FAILED(hr)) {
    std::move(callback).Run(std::vector<blink::mojom::RelatedApplicationPtr>());
    return;
  }

  ComPtr<IUriRuntimeClass> url;
  hr = url_factory->CreateUri(
      base::win::ScopedHString::Create(frame_url.spec()).get(), &url);
  if (FAILED(hr)) {
    std::move(callback).Run(std::vector<blink::mojom::RelatedApplicationPtr>());
    return;
  }

  ComPtr<ILauncherStatics4> launcher;
  hr = base::win::GetActivationFactory<ILauncherStatics4,
                                       RuntimeClass_Windows_System_Launcher>(
      &launcher);
  if (FAILED(hr)) {
    std::move(callback).Run(std::vector<blink::mojom::RelatedApplicationPtr>());
    return;
  }

  // FindAppUriHandlersAsync API returns list of apps that is validated
  // by the sites.
  // https://docs.microsoft.com/en-us/windows/uwp/
  // launch-resume/web-to-app-linking
  ComPtr<IAsyncOperation<IVectorView<AppInfo*>*>> enum_operation;
  hr = launcher->FindAppUriHandlersAsync(url.Get(), &enum_operation);
  if (FAILED(hr)) {
    std::move(callback).Run(std::vector<blink::mojom::RelatedApplicationPtr>());
    return;
  }

  base::win::PostAsyncResults(
      std::move(enum_operation),
      base::BindOnce(&OnGetAppUrlHandlers, std::move(related_apps),
                     std::move(callback)));
}

}  // namespace installed_app_provider_win
}  // namespace content

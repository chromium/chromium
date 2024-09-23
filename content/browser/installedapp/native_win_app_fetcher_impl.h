// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INSTALLEDAPP_NATIVE_WIN_APP_FETCHER_IMPL_H_
#define CONTENT_BROWSER_INSTALLEDAPP_NATIVE_WIN_APP_FETCHER_IMPL_H_

#include <Windows.ApplicationModel.h>
#include <Windows.System.h>
#include <combaseapi.h>
#include <windows.foundation.collections.h>
#include <wrl/async.h>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/installedapp/native_win_app_fetcher.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom-forward.h"
#include "url/gurl.h"

namespace content {

// The implementation of NativeWinAppFetcher.
class CONTENT_EXPORT NativeWinAppFetcherImpl : public NativeWinAppFetcher {
 public:
  NativeWinAppFetcherImpl();
  ~NativeWinAppFetcherImpl() override;

  void FetchAppsForUrl(
      const GURL& url,
      base::OnceCallback<void(std::vector<blink::mojom::RelatedApplicationPtr>)>
          callback) override;

 private:
  void OnGetAppUriHandlers(
      Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::ApplicationModel::AppInfo*>> found_app_list);

  void OnFailure();

  base::OnceCallback<void(std::vector<blink::mojom::RelatedApplicationPtr>)>
      callback_;
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::ApplicationModel::AppInfo*>*>>
      enum_operation_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NativeWinAppFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INSTALLEDAPP_NATIVE_WIN_APP_FETCHER_IMPL_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/webui_bundled_code_cache_fetcher.h"

#include "base/metrics/histogram_functions.h"
#include "mojo/public/cpp/base/ref_counted_memory_mojom_traits.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WebUIBundledCodeCacheFetcher::WebUIBundledCodeCacheFetcher(
    scoped_refptr<base::SequencedTaskRunner> host_task_runner,
    int resource_id,
    base::OnceClosure done_closure)
    : host_task_runner_(std::move(host_task_runner)),
      resource_id_(resource_id),
      done_closure_(std::move(done_closure)) {}

WebUIBundledCodeCacheFetcher::~WebUIBundledCodeCacheFetcher() = default;

void WebUIBundledCodeCacheFetcher::DidReceiveCachedMetadataFromUrlLoader() {
  NOTREACHED();
}

std::optional<mojo_base::BigBuffer>
WebUIBundledCodeCacheFetcher::TakeCodeCacheForResponse(
    const network::mojom::URLResponseHead& response_head) {
  CHECK(!is_waiting_);
  return std::move(code_cache_data_);
}

void WebUIBundledCodeCacheFetcher::OnReceivedRedirect(const KURL& new_url) {
  // WebUI requests should not be redirected.
  NOTREACHED();
}

bool WebUIBundledCodeCacheFetcher::IsWaiting() const {
  return is_waiting_;
}

void WebUIBundledCodeCacheFetcher::Start() {
  // TODO(crbug.com/375509504): Evaluate whether async fetch is necessary and
  // remove either the sync or async fetch codepath.
  if (RuntimeEnabledFeatures::WebUIBundledCodeCacheAsyncFetchEnabled()) {
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(
            [](int code_cache_resource_id,
               scoped_refptr<base::SequencedTaskRunner> host_task_runner,
               base::WeakPtr<WebUIBundledCodeCacheFetcher> weak_this) {
              base::RefCountedMemory* buffer_data =
                  Platform::Current()->GetDataResourceBytes(
                      code_cache_resource_id);
              PostCrossThreadTask(
                  *host_task_runner, FROM_HERE,
                  CrossThreadBindOnce(
                      &WebUIBundledCodeCacheFetcher::DidReceiveCachedCode,
                      std::move(weak_this),
                      buffer_data ? mojo_base::BigBuffer(*buffer_data)
                                  : mojo_base::BigBuffer()));
            },
            resource_id_, host_task_runner_, weak_factory_.GetWeakPtr()));
    return;
  }

  scoped_refptr<base::RefCountedMemory> buffer_data =
      Platform::Current()->GetDataResourceBytes(resource_id_);
  host_task_runner_->PostTask(
      FROM_HERE,
      blink::BindOnce(&WebUIBundledCodeCacheFetcher::DidReceiveCachedCode,
                      weak_factory_.GetWeakPtr(),
                      buffer_data ? mojo_base::BigBuffer(*buffer_data)
                                  : mojo_base::BigBuffer()));
}

void WebUIBundledCodeCacheFetcher::DidReceiveCachedCode(
    mojo_base::BigBuffer data) {
  base::UmaHistogramBoolean(
      "Blink.ResourceRequest.WebUIBundledCodeCacheFetcher.DidReceiveCachedCode",
      (data.size() > 0));
  if (data.size() > 0) {
    code_cache_data_ = std::move(data);
  }
  is_waiting_ = false;
  std::move(done_closure_).Run();
}

}  // namespace blink

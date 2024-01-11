// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_proxy_lookup_helper.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace content {

// Runs the proxy lookup on the UI thread. Created on the
// PepperProxyLookupHelper's thread, but then does all work on the UI thread,
// and is deleted there by a task posted by the PepperProxyLookupHelper.
class PepperProxyLookupHelper::UIThreadHelper
    : public network::mojom::ProxyLookupClient {
 public:
  UIThreadHelper(const GURL& url,
                 LookUpProxyForURLCallback look_up_proxy_for_url_callback,
                 LookUpCompleteCallback look_up_complete_callback)
      : look_up_complete_callback_(std::move(look_up_complete_callback)),
        callback_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&UIThreadHelper::StartLookup, base::Unretained(this),
                       url, std::move(look_up_proxy_for_url_callback)));
  }

  UIThreadHelper(const UIThreadHelper&) = delete;
  UIThreadHelper& operator=(const UIThreadHelper&) = delete;

  ~UIThreadHelper() override { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

 private:
  void StartLookup(const GURL& url,
                   LookUpProxyForURLCallback look_up_proxy_for_url_callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    mojo::PendingRemote<network::mojom::ProxyLookupClient> proxy_lookup_client =
        receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(
        base::BindOnce(&UIThreadHelper::OnProxyLookupComplete,
                       base::Unretained(this), net::ERR_ABORTED, std::nullopt));
    if (!std::move(look_up_proxy_for_url_callback)
             .Run(url, std::move(proxy_lookup_client))) {
      OnProxyLookupComplete(net::ERR_FAILED, std::nullopt);
    }
  }

  void OnProxyLookupComplete(
      int32_t net_error,
      const std::optional<net::ProxyInfo>& proxy_info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    receiver_.reset();
    callback_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(look_up_complete_callback_), proxy_info));
  }

  mojo::Receiver<network::mojom::ProxyLookupClient> receiver_{this};

  LookUpCompleteCallback look_up_complete_callback_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
};

PepperProxyLookupHelper::PepperProxyLookupHelper() {}

PepperProxyLookupHelper::~PepperProxyLookupHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                        std::move(ui_thread_helper_));
}

void PepperProxyLookupHelper::Start(
    const GURL& url,
    LookUpProxyForURLCallback look_up_proxy_for_url_callback,
    LookUpCompleteCallback look_up_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!look_up_complete_callback_);
  DCHECK(!ui_thread_helper_);

  look_up_complete_callback_ = std::move(look_up_complete_callback);

  ui_thread_helper_ = std::make_unique<UIThreadHelper>(
      url, std::move(look_up_proxy_for_url_callback),
      base::BindOnce(&PepperProxyLookupHelper::OnProxyLookupComplete,
                     weak_factory_.GetWeakPtr()));
}

void PepperProxyLookupHelper::OnProxyLookupComplete(
    std::optional<net::ProxyInfo> proxy_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(look_up_complete_callback_).Run(std::move(proxy_info));
}

}  // namespace content

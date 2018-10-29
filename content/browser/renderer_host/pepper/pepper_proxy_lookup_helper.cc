// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_proxy_lookup_helper.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
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
      : binding_(this),
        look_up_complete_callback_(std::move(look_up_complete_callback)),
        callback_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&UIThreadHelper::StartLookup, base::Unretained(this),
                       url, std::move(look_up_proxy_for_url_callback)));
  }

  ~UIThreadHelper() override { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

 private:
  void StartLookup(const GURL& url,
                   LookUpProxyForURLCallback look_up_proxy_for_url_callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    network::mojom::ProxyLookupClientPtr proxy_lookup_client;
    binding_.Bind(mojo::MakeRequest(&proxy_lookup_client));
    binding_.set_connection_error_handler(
        base::BindOnce(&UIThreadHelper::OnProxyLookupComplete,
                       base::Unretained(this), base::nullopt));
    if (!std::move(look_up_proxy_for_url_callback)
             .Run(url, std::move(proxy_lookup_client))) {
      OnProxyLookupComplete(base::nullopt);
    }
  }

  void OnProxyLookupComplete(
      const base::Optional<net::ProxyInfo>& proxy_info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    binding_.Close();
    callback_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(look_up_complete_callback_), proxy_info));
  }

  mojo::Binding<network::mojom::ProxyLookupClient> binding_;

  LookUpCompleteCallback look_up_complete_callback_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UIThreadHelper);
};

PepperProxyLookupHelper::PepperProxyLookupHelper() : weak_factory_(this) {}

PepperProxyLookupHelper::~PepperProxyLookupHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE,
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
    base::Optional<net::ProxyInfo> proxy_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(look_up_complete_callback_).Run(std::move(proxy_info));
}

}  // namespace content

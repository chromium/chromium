// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/io_thread_shared_url_loader_factory_owner.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace content {
namespace {

using SharedURLLoaderFactoryGetterCallback =
    base::OnceCallback<scoped_refptr<network::SharedURLLoaderFactory>()>;

void InitializeSharedFactoryOnIOThread(
    SharedURLLoaderFactoryGetterCallback shared_url_loader_factory_getter,
    scoped_refptr<network::SharedURLLoaderFactory>* out_shared_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::RunLoop run_loop;
  GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](SharedURLLoaderFactoryGetterCallback getter,
             scoped_refptr<network::SharedURLLoaderFactory>*
                 shared_factory_ptr) {
            DCHECK(!shared_factory_ptr->get())
                << "shared_url_loader_factory_ can only be initialized once.";
            *shared_factory_ptr = std::move(getter).Run();
          },
          std::move(shared_url_loader_factory_getter),
          base::Unretained(out_shared_factory)),
      run_loop.QuitClosure());
  run_loop.Run();
}

network::SimpleURLLoader::BodyAsStringCallbackDeprecated RunOnUIThread(
    network::SimpleURLLoader::BodyAsStringCallbackDeprecated ui_callback) {
  return base::BindOnce(
      [](network::SimpleURLLoader::BodyAsStringCallbackDeprecated callback,
         std::unique_ptr<std::string> response_body) {
        DCHECK_CURRENTLY_ON(BrowserThread::IO);
        GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), std::move(response_body)));
      },
      std::move(ui_callback));
}

}  // namespace

// static
IOThreadSharedURLLoaderFactoryOwner::IOThreadSharedURLLoaderFactoryOwnerPtr
IOThreadSharedURLLoaderFactoryOwner::Create(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> info) {
  return IOThreadSharedURLLoaderFactoryOwnerPtr(
      new IOThreadSharedURLLoaderFactoryOwner(std::move(info)));
}

IOThreadSharedURLLoaderFactoryOwner::IOThreadSharedURLLoaderFactoryOwner(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InitializeSharedFactoryOnIOThread(
      base::BindOnce(&network::SharedURLLoaderFactory::Create, std::move(info)),
      &shared_url_loader_factory_);
}

IOThreadSharedURLLoaderFactoryOwner::~IOThreadSharedURLLoaderFactoryOwner() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

int IOThreadSharedURLLoaderFactoryOwner::LoadBasicRequestOnIOThread(
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;

  // |simple_loader_helper| lives on UI thread and shouldn't be accessed on
  // other threads.
  SimpleURLLoaderTestHelper simple_loader_helper;

  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](network::SimpleURLLoader* loader,
             network::mojom::URLLoaderFactory* factory,
             network::SimpleURLLoader::BodyAsStringCallbackDeprecated
                 body_as_string_callback) {
            loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
                factory, std::move(body_as_string_callback));
          },
          base::Unretained(simple_loader.get()),
          base::Unretained(shared_url_loader_factory_.get()),
          RunOnUIThread(simple_loader_helper.GetCallbackDeprecated())));

  simple_loader_helper.WaitForCallback();
  return simple_loader->NetError();
}

}  // namespace content

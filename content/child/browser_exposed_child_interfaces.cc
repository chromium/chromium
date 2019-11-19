// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/browser_exposed_child_interfaces.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "content/child/child_histogram_fetcher_impl.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {

void ExposeChildInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::BinderMap* binders) {
  binders->Add(base::BindRepeating(&ChildHistogramFetcherFactoryImpl::Create),
               io_task_runner);

  GetContentClient()->ExposeInterfacesToBrowser(io_task_runner, binders);
}

}  // namespace content

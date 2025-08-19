// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/corp_messaging_playground.h"

#include <memory>

#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/base/url_request_context_getter.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {

CorpMessagingPlayground::CorpMessagingPlayground() {
  auto url_request_context_getter =
      base::MakeRefCounted<URLRequestContextGetter>(
          base::SingleThreadTaskRunner::GetCurrentDefault());
  url_loader_factory_owner_ =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          url_request_context_getter, /* is_trusted= */ true);
}

CorpMessagingPlayground::~CorpMessagingPlayground() = default;

void CorpMessagingPlayground::Start() {
  base::RunLoop run_loop;
  // TODO: joedow - Do stuff....
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace remoting

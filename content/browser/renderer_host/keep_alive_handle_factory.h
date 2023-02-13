// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_KEEP_ALIVE_HANDLE_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_KEEP_ALIVE_HANDLE_FACTORY_H_

#include <memory>

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/loader/keep_alive_handle_factory.mojom-forward.h"

namespace content {

class RenderProcessHost;

// A KeepAliveHandleFactory creates blink::mojom::KeepAliveHandle. Each created
// handle prolongs the associated renderer's lifetime by using
// RenderProcessHost's KeepAliveRefCounts while alive.
// When a certain time passes after the factory is destroyed, all created
// handles are invalidated, which will result in render process shutdown.
class KeepAliveHandleFactory final {
 public:
  KeepAliveHandleFactory(RenderProcessHost* process_host,
                         base::TimeDelta timeout);
  ~KeepAliveHandleFactory();

  KeepAliveHandleFactory(const KeepAliveHandleFactory&) = delete;
  KeepAliveHandleFactory& operator=(const KeepAliveHandleFactory&) = delete;

  // Sets the timeout after which all created handles will be invalidated.
  void set_timeout(base::TimeDelta timeout) { timeout_ = timeout; }

  void Bind(
      mojo::PendingReceiver<blink::mojom::KeepAliveHandleFactory> receiver);

 private:
  class Context;

  std::unique_ptr<Context> context_;
  base::TimeDelta timeout_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_KEEP_ALIVE_HANDLE_FACTORY_H_

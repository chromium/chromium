// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_INTERNALS_HANDLER_IMPL_H_

#include "content/browser/preloading/prerender/prerender_internals.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class PrerenderInternalsHandlerImpl : public mojom::PrerenderInternalsHandler {
 public:
  explicit PrerenderInternalsHandlerImpl(
      mojo::PendingReceiver<mojom::PrerenderInternalsHandler> receiver);
  ~PrerenderInternalsHandlerImpl() override;

  void GetPrerenderInfo(GetPrerenderInfoCallback callback) override;

 private:
  mojo::Receiver<mojom::PrerenderInternalsHandler> receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_INTERNALS_HANDLER_IMPL_H_

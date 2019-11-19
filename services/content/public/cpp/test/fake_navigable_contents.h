// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CONTENT_PUBLIC_CPP_TEST_FAKE_NAVIGABLE_CONTENTS_H_
#define SERVICES_CONTENT_PUBLIC_CPP_TEST_FAKE_NAVIGABLE_CONTENTS_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "services/content/public/mojom/navigable_contents.mojom.h"

namespace content {

// Implementation of NavigableContents to be used by tests in conjunction with
// FakeNavigableContentsFactory.
class FakeNavigableContents : public mojom::NavigableContents {
 public:
  FakeNavigableContents();
  ~FakeNavigableContents() override;

  // Set response headers to provide in calls to |DidFinishNavigation()| on any
  // NavigableContentsClient when driven by a call to
  // |NavigableContents::Navigate()|.
  void set_default_response_headers(
      const scoped_refptr<net::HttpResponseHeaders> headers) {
    default_response_headers_ = headers;
  }

  // Binds this object to a NavigableContents receiver and gives it a
  // corresponding remote client interface. May only be called once.
  void Bind(mojo::PendingReceiver<mojom::NavigableContents> receiver,
            mojo::PendingRemote<mojom::NavigableContentsClient> client);

 private:
  // mojom::NavigableContents:
  void Navigate(const GURL& url, mojom::NavigateParamsPtr params) override;
  void GoBack(mojom::NavigableContents::GoBackCallback callback) override;
  void CreateView(CreateViewCallback callback) override;
  void Focus() override;
  void FocusThroughTabTraversal(bool reverse) override;

  mojo::Receiver<mojom::NavigableContents> receiver_{this};
  mojo::Remote<mojom::NavigableContentsClient> client_;

  scoped_refptr<net::HttpResponseHeaders> default_response_headers_;

  DISALLOW_COPY_AND_ASSIGN(FakeNavigableContents);
};

}  // namespace content

#endif  // SERVICES_CONTENT_PUBLIC_CPP_TEST_FAKE_NAVIGABLE_CONTENTS_H_

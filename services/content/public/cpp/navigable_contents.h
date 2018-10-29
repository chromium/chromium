// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_H_
#define SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/content/public/cpp/navigable_contents_observer.h"
#include "services/content/public/mojom/navigable_contents.mojom.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"

namespace content {

class NavigableContentsView;

// A NavigableContents controls a single dedicated instance of a top-level,
// navigable content frame hosted by the Content Service. In addition to
// maintaining its own navigation state, a NavigableContents may be used to
// acquire an embeddable native UI object to display renderered content within a
// client application's own UI.
class COMPONENT_EXPORT(CONTENT_SERVICE_CPP) NavigableContents
    : public mojom::NavigableContentsClient {
 public:
  // Constructs a new NavigableContents using |factory|.
  explicit NavigableContents(mojom::NavigableContentsFactory* factory);
  NavigableContents(mojom::NavigableContentsFactory* factory,
                    mojom::NavigableContentsParamsPtr params);
  ~NavigableContents() override;

  // These methods NavigableContentsObservers registered on this object.
  void AddObserver(NavigableContentsObserver* observer);
  void RemoveObserver(NavigableContentsObserver* observer);

  // Returns a NavigableContentsView which renders this NavigableContents's
  // currently navigated contents. This widget can be parented and displayed
  // anywhere within the application's own window tree.
  //
  // Note that this NavigableContentsView is created lazily on first call, and
  // by default NavigableContents does not otherwise create or manipulate UI
  // objects.
  NavigableContentsView* GetView();

  // Begins an attempt to asynchronously navigate this NavigableContents to
  // |url|.
  void Navigate(const GURL& url);
  void NavigateWithParams(const GURL& url, mojom::NavigateParamsPtr params);

 private:
  // mojom::NavigableContentsClient:
  void DidFinishNavigation(
      const GURL& url,
      bool is_main_frame,
      bool is_error_page,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers) override;
  void DidStopLoading() override;
  void DidAutoResizeView(const gfx::Size& new_size) override;
  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override;

  void OnEmbedTokenReceived(const base::UnguessableToken& token);

  mojom::NavigableContentsPtr contents_;
  mojo::Binding<mojom::NavigableContentsClient> client_binding_;
  std::unique_ptr<NavigableContentsView> view_;

  base::ReentrantObserverList<NavigableContentsObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(NavigableContents);
};

}  // namespace content

#endif  // SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_H_

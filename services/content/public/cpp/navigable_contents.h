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
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/content/public/cpp/navigable_contents_observer.h"
#include "services/content/public/mojom/navigable_contents.mojom.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "ui/accessibility/ax_tree_id.h"

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

  // Returns the last known ID of the content area's accessibility tree, if any.
  const ui::AXTreeID& content_ax_tree_id() const { return content_ax_tree_id_; }

  // Begins an attempt to asynchronously navigate this NavigableContents to
  // |url|.
  void Navigate(const GURL& url);
  void NavigateWithParams(const GURL& url, mojom::NavigateParamsPtr params);

  // Attempts to navigate back in the web contents' history stack. The supplied
  // |callback| is run to indicate success/failure of the navigation attempt.
  // The navigation attempt will fail if the history stack is empty.
  void GoBack(content::mojom::NavigableContents::GoBackCallback callback);

  // Attempts to transfer global input focus to the navigated contents if they
  // have an active visual representation.
  void Focus();

  // Similar to above but for use specifically when UI element traversal is
  // being done via Tab-key cycling or a similar mechanism.
  void FocusThroughTabTraversal(bool reverse);

 private:
  // mojom::NavigableContentsClient:
  void ClearViewFocus() override;
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
  void UpdateCanGoBack(bool can_go_back) override;
  void UpdateContentAXTree(const ui::AXTreeID& id) override;
  void FocusedNodeChanged(bool is_editable_node,
                          const gfx::Rect& node_bounds_in_screen) override;

  void OnEmbedTokenReceived(const base::UnguessableToken& token);

  mojo::Remote<mojom::NavigableContents> contents_;
  mojo::Receiver<mojom::NavigableContentsClient> client_receiver_;
  std::unique_ptr<NavigableContentsView> view_;

  base::ReentrantObserverList<NavigableContentsObserver> observers_;

  ui::AXTreeID content_ax_tree_id_;

  DISALLOW_COPY_AND_ASSIGN(NavigableContents);
};

}  // namespace content

#endif  // SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_H_

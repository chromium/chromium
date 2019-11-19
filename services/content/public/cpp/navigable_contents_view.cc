// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/public/cpp/navigable_contents_view.h"

#include <map>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/synchronization/atomic_flag.h"
#include "base/unguessable_token.h"
#include "services/content/public/cpp/navigable_contents.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/focus/focus_manager.h"  // nogncheck
#include "ui/views/layout/fill_layout.h"  // nogncheck
#include "ui/views/view.h"                // nogncheck
#endif  // defined(TOOLKIT_VIEWS)

#if defined(USE_AURA)
#include "ui/aura/layout_manager.h"  // nogncheck
#include "ui/aura/window.h"          // nogncheck
#endif

namespace content {

namespace {

using InProcessEmbeddingMap =
    std::map<base::UnguessableToken,
             base::OnceCallback<void(NavigableContentsView*)>>;

InProcessEmbeddingMap& GetInProcessEmbeddingMap() {
  static base::NoDestructor<InProcessEmbeddingMap> embedding_map;
  return *embedding_map;
}

base::AtomicFlag& GetInServiceProcessFlag() {
  static base::NoDestructor<base::AtomicFlag> in_service_process;
  return *in_service_process;
}

#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)

// Keeps child windows sized to the same bounds as the owning window.
class LocalWindowLayoutManager : public aura::LayoutManager {
 public:
  explicit LocalWindowLayoutManager(aura::Window* owner) : owner_(owner) {}
  ~LocalWindowLayoutManager() override = default;

  // aura::LayoutManger:
  void OnWindowResized() override { ResizeChildren(); }
  void OnWindowAddedToLayout(aura::Window* child) override { ResizeChildren(); }
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}

 private:
  void ResizeChildren() {
    for (auto* child : owner_->children())
      SetChildBoundsDirect(child, gfx::Rect(owner_->bounds().size()));
  }

  aura::Window* const owner_;

  DISALLOW_COPY_AND_ASSIGN(LocalWindowLayoutManager);
};

// Owns an Aura window which parents another Aura window in the same process,
// corresponding to a web contents view hosted in the process.
class LocalViewHost : public views::NativeViewHost {
 public:
  LocalViewHost(aura::Window* window, NavigableContents* contents)
      : window_(window), contents_(contents) {
    window_->SetLayoutManager(new LocalWindowLayoutManager(window_));
  }

  ~LocalViewHost() override = default;

  // views::View:
  void AddedToWidget() override {
    if (!native_view())
      Attach(window_);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kWebView;

    // The document title is provided to the accessibility system by other
    // means, so setting it here would be redundant.
    node_data->SetNameExplicitlyEmpty();

    if (contents_->content_ax_tree_id() != ui::AXTreeIDUnknown()) {
      node_data->AddStringAttribute(ax::mojom::StringAttribute::kChildTreeId,
                                    contents_->content_ax_tree_id().ToString());
    }
  }

 private:
  aura::Window* const window_;
  NavigableContents* const contents_;

  DISALLOW_COPY_AND_ASSIGN(LocalViewHost);
};

#endif  // defined(TOOLKIT_VIEWS) && defined(USE_AURA)

}  // namespace

NavigableContentsView::~NavigableContentsView() = default;

// static
void NavigableContentsView::SetClientRunningInServiceProcess() {
  GetInServiceProcessFlag().Set();
}

// static
bool NavigableContentsView::IsClientRunningInServiceProcess() {
  return GetInServiceProcessFlag().IsSet();
}

void NavigableContentsView::ClearNativeFocus() {
#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)
  auto* focus_manager = view_->GetFocusManager();
  if (focus_manager)
    focus_manager->ClearNativeFocus();
#endif  // defined(TOOLKIT_VIEWS) && defined(USE_AURA)
}

void NavigableContentsView::NotifyAccessibilityTreeChange() {
#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)
  view_->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, false);
#endif
}

NavigableContentsView::NavigableContentsView(NavigableContents* contents)
    : contents_(contents) {
#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)
  window_ = std::make_unique<aura::Window>(nullptr);
  window_->set_owned_by_parent(false);
  window_->SetName("NavigableContentsViewWindow");
  window_->SetType(aura::client::WINDOW_TYPE_CONTROL);
  window_->Init(ui::LAYER_NOT_DRAWN);
  window_->Show();

  view_ = std::make_unique<LocalViewHost>(window_.get(), contents_);
  view_->set_owned_by_client();
#endif  // defined(TOOLKIT_VIEWS) && defined(USE_AURA)
}

void NavigableContentsView::EmbedUsingToken(
    const base::UnguessableToken& token) {
#if defined(TOOLKIT_VIEWS)
  DCHECK(IsClientRunningInServiceProcess());

  // |token| should already have an embed callback entry in the in-process
  // callback map, injected by the in-process Content Service implementation.
  auto& embeddings = GetInProcessEmbeddingMap();
  auto it = embeddings.find(token);
  if (it == embeddings.end()) {
    DLOG(ERROR) << "Unable to embed with unknown token " << token.ToString();
    return;
  }

  // Invoke a callback provided by the Content Service's host environment. This
  // should parent a web content view to our own |view()|, as well as set
  // |native_view_| to the corresponding web contents' own NativeView.
  auto callback = std::move(it->second);
  embeddings.erase(it);
  std::move(callback).Run(this);
#endif  // defined(TOOLKIT_VIEWS)
}

// static
void NavigableContentsView::RegisterInProcessEmbedCallback(
    const base::UnguessableToken& token,
    base::OnceCallback<void(NavigableContentsView*)> callback) {
  GetInProcessEmbeddingMap()[token] = std::move(callback);
}

}  // namespace content

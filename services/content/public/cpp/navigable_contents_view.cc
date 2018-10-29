// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/public/cpp/navigable_contents_view.h"

#include <map>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/synchronization/atomic_flag.h"
#include "base/unguessable_token.h"
#include "services/content/public/cpp/buildflags.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/layout/fill_layout.h"  // nogncheck
#include "ui/views/view.h"                // nogncheck

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
#include "services/ws/public/mojom/window_tree_constants.mojom.h"  // nogncheck
#include "ui/base/ui_base_features.h"                   // nogncheck
#include "ui/views/controls/native/native_view_host.h"  // nogncheck
#include "ui/views/mus/remote_view/remote_view_host.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
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

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

std::unique_ptr<NavigableContentsView::RemoteViewManager>&
GetRemoteViewManager() {
  static base::NoDestructor<
      std::unique_ptr<NavigableContentsView::RemoteViewManager>>
      manager;
  return *manager;
}

#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

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
      SetChildBoundsDirect(child, owner_->bounds());
  }

  aura::Window* const owner_;

  DISALLOW_COPY_AND_ASSIGN(LocalWindowLayoutManager);
};

// Owns an Aura window which parents another Aura window in the same process,
// corresponding to a web contents view hosted in the process.
class LocalViewHost : public views::NativeViewHost {
 public:
  explicit LocalViewHost(aura::Window* window) : window_(window) {
    window_->SetLayoutManager(new LocalWindowLayoutManager(window_));
  }

  ~LocalViewHost() override = default;

  // views::View:
  void AddedToWidget() override {
    if (!native_view())
      Attach(window_);
  }

 private:
  aura::Window* const window_;

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

NavigableContentsView::NavigableContentsView() {
#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)
#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
  if (!IsClientRunningInServiceProcess()) {
    RemoteViewManager* manager = GetRemoteViewManager().get();
    if (manager)
      view_ = manager->CreateRemoteViewHost();
    else
      view_ = std::make_unique<views::RemoteViewHost>();
    view_->set_owned_by_client();
    return;
  }
#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

  window_ = std::make_unique<aura::Window>(nullptr);
  window_->set_owned_by_parent(false);
  window_->SetName("NavigableContentsViewWindow");
  window_->SetType(aura::client::WINDOW_TYPE_CONTROL);
  window_->Init(ui::LAYER_NOT_DRAWN);
  window_->Show();

  view_ = std::make_unique<LocalViewHost>(window_.get());
  view_->set_owned_by_client();
#endif  // defined(TOOLKIT_VIEWS) && defined(USE_AURA)
}

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

// static
void NavigableContentsView::SetRemoteViewManager(
    std::unique_ptr<RemoteViewManager> manager) {
  GetRemoteViewManager() = std::move(manager);
}

#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

void NavigableContentsView::EmbedUsingToken(
    const base::UnguessableToken& token) {
#if defined(TOOLKIT_VIEWS)
#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
  if (!IsClientRunningInServiceProcess()) {
    RemoteViewManager* manager = GetRemoteViewManager().get();
    if (manager) {
      manager->EmbedUsingToken(view_.get(), token);
    } else {
      constexpr uint32_t kEmbedFlags =
          ws::mojom::kEmbedFlagEmbedderInterceptsEvents |
          ws::mojom::kEmbedFlagEmbedderControlsVisibility;
      static_cast<views::RemoteViewHost*>(view_.get())
          ->EmbedUsingToken(token, kEmbedFlags, base::DoNothing());
    }

    return;
  }
#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

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

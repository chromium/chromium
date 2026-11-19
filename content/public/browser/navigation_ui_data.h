// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_UI_DATA_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_UI_DATA_H_

#include <concepts>
#include <cstddef>
#include <memory>
#include <utility>

#include "content/common/content_export.h"

namespace content {

class NavigationHandle;

// Copyable interface for embedders to pass opaque data to `//content`.
//
// Lifecycle and timing on `NavigationHandle::GetNavigationUIData()`:
//
// 1. When upfront `NavigationUIData` is supplied (via `LoadURLParams`,
//    `OpenURLParams`, or `WebContents::StartPrerendering()`):
//    - Before `NavigationRequest::StartNavigation()` (e.g. while waiting on
//      `BeforeUnload` or prerender activation checks), `GetNavigationUIData()`
//      is already non-null, with only upfront caller-supplied fields set.
//    - In `NavigationRequest::StartNavigation()`,
//      `PopulateFromNavigationHandle()` is called before
//      `WebContentsObserver::DidStartNavigation()` fires, and
//      `ContentBrowserClient::GetNavigationUIData()` is not called.
// 2. When no upfront `NavigationUIData` is supplied (e.g. renderer-initiated
//    navigations or subframe navigations):
//    - `GetNavigationUIData()` is still `nullptr` during
//      `DidStartNavigation()`, and is only created in
//      `NavigationRequest::WillStartRequest()` via
//      `ContentBrowserClient::GetNavigationUIData()` (or remains `nullptr` for
//      navigations that do not use a `NavigationURLLoader`, such as
//      same-document or `about:blank` navigations).
// 3. `PopulateFromNavigationHandle()` may be called more than once on the same
//    instance (e.g. if a same-document navigation restarts as a cross-document
//    navigation in `NavigationRequest::ResetForCrossDocumentRestart()`).
class CONTENT_EXPORT NavigationUIData {
 public:
  virtual ~NavigationUIData();

  // Creates a new `NavigationUIData` that is a deep copy of the original for
  // the same navigation (e.g. passing to `NavigationURLLoader`).
  virtual std::unique_ptr<NavigationUIData> Clone() = 0;

  // Returns the upfront `NavigationUIData` for a new navigation re-dispatched
  // from this navigation via `OpenURLParams::FromNavigationHandle()`, carrying
  // only the caller-supplied inputs that should apply to the new navigation
  // and no `NavigationHandle`-dependent state.
  //
  // Returns `nullptr` if the new navigation should start without upfront
  // `NavigationUIData`, in which case it is created lazily via
  // `ContentBrowserClient::GetNavigationUIData()` as usual. Implementations
  // must only return `nullptr` when the result would otherwise have all
  // caller-supplied fields at their default values, so that both outcomes are
  // equivalent apart from timing. The default implementation always returns
  // `nullptr`.
  virtual std::unique_ptr<NavigationUIData>
  MaybeCreateForRedispatchedNavigation();

  // Populates or refreshes `NavigationHandle`-dependent fields at navigation
  // start when `NavigationUIData` was supplied upfront before the
  // `NavigationHandle` existed. May be called more than once on the same
  // instance if the navigation restarts (e.g.
  // `NavigationRequest::ResetForCrossDocumentRestart()`).
  virtual void PopulateFromNavigationHandle(
      NavigationHandle& navigation_handle);
};

// Owning pointer to `NavigationUIData` that deep-copies via `Clone()` when
// copied, allowing parameter structs such as `OpenURLParams` and
// `PrerenderAttributes` to keep `= default` copy and move operations.
class CONTENT_EXPORT CloneableNavigationUIDataPtr {
 public:
  CloneableNavigationUIDataPtr();
  CloneableNavigationUIDataPtr(std::nullptr_t);  // NOLINT
  CloneableNavigationUIDataPtr(                  // NOLINT
      std::unique_ptr<NavigationUIData> ptr);
  template <typename T>
    requires(std::convertible_to<T*, NavigationUIData*>)
  CloneableNavigationUIDataPtr(std::unique_ptr<T> ptr)  // NOLINT
      : CloneableNavigationUIDataPtr(
            std::unique_ptr<NavigationUIData>(std::move(ptr))) {}
  ~CloneableNavigationUIDataPtr();

  // Movable and copyable.
  CloneableNavigationUIDataPtr(const CloneableNavigationUIDataPtr& other);
  CloneableNavigationUIDataPtr& operator=(
      const CloneableNavigationUIDataPtr& other);
  CloneableNavigationUIDataPtr(CloneableNavigationUIDataPtr&& other) noexcept;
  CloneableNavigationUIDataPtr& operator=(
      CloneableNavigationUIDataPtr&& other) noexcept;

  NavigationUIData* get() const { return ptr_.get(); }
  NavigationUIData* operator->() const { return ptr_.get(); }
  NavigationUIData& operator*() const { return *ptr_; }
  explicit operator bool() const { return static_cast<bool>(ptr_); }

  std::unique_ptr<NavigationUIData> Clone() const {
    return ptr_ ? ptr_->Clone() : nullptr;
  }

  friend bool operator==(const CloneableNavigationUIDataPtr& lhs,
                         std::nullptr_t) {
    return lhs.ptr_ == nullptr;
  }

 private:
  std::unique_ptr<NavigationUIData> ptr_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_UI_DATA_H_

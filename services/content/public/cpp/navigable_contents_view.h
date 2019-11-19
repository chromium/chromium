// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_VIEW_H_
#define SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_VIEW_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/unguessable_token.h"
#include "ui/gfx/native_widget_types.h"

#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)
#include "ui/views/controls/native/native_view_host.h"  // nogncheck
#endif

namespace aura {
class Window;
}

namespace views {
class View;
class NativeViewHost;
}  // namespace views

namespace content {

class NavigableContents;
class NavigableContentsImpl;

// NavigableContentsView encapsulates cross-platform manipulation and
// presentation of a NavigableContents within a native application UI based on
// either Views, UIKit, AppKit, or the Android Framework.
//
// TODO(https://crbug.com/855092): Actually support UI frameworks other than
// Views UI on Aura.
class COMPONENT_EXPORT(CONTENT_SERVICE_CPP) NavigableContentsView {
 public:
  ~NavigableContentsView();

  // Used to set/query whether the calling process is the same process in which
  // all Content Service instances are running. This should be used sparingly,
  // and in general is only here to support internal sanity checks when
  // performing, e.g., UI embedding operations on platforms where remote
  // NavigableContentsViews are not yet supported.
  static void SetClientRunningInServiceProcess();
  static bool IsClientRunningInServiceProcess();

#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)
  views::View* view() const { return view_.get(); }

  gfx::NativeView native_view() const { return view_->native_view(); }
#endif  // defined(TOOLKIT_VIEWS) && defined(USE_AURA)

  // Clears the native view having focus. See FocusManager::ClearNativeFocus.
  void ClearNativeFocus();

  // Has this view notify the UI subsystem of an accessibility tree change.
  void NotifyAccessibilityTreeChange();

 private:
  friend class FakeNavigableContents;
  friend class NavigableContents;
  friend class NavigableContentsImpl;

  explicit NavigableContentsView(NavigableContents* contents_);

  // Establishes a hierarchical relationship between this view's native UI
  // object and another native UI object within the Content Service.
  void EmbedUsingToken(const base::UnguessableToken& token);

  // Used by the service directly when running in the same process. Establishes
  // a way for an embed token to be used without the UI service.
  static void RegisterInProcessEmbedCallback(
      const base::UnguessableToken& token,
      base::OnceCallback<void(NavigableContentsView*)> callback);

  NavigableContents* const contents_;

#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)
  // This NavigableContents's Window and corresponding View.
  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<views::NativeViewHost> view_;
#endif  // defined(TOOLKIT_VIEWS) && defined(USE_AURA)

  DISALLOW_COPY_AND_ASSIGN(NavigableContentsView);
};

}  // namespace content

#endif  // SERVICES_CONTENT_PUBLIC_CPP_NAVIGABLE_CONTENTS_VIEW_H_

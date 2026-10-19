// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_

#include "base/memory/raw_ptr.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class AppWindow;

class AppCurrentWindowInternalExtensionFunction : public ExtensionFunction {
 protected:
  ~AppCurrentWindowInternalExtensionFunction() override = default;

  AppWindow* window() { return window_; }

 private:
  // ExtensionFunction:
  bool PreRunValidation(std::string* error) override;

  // The current AppWindow.
  raw_ptr<AppWindow> window_ = nullptr;
};

class AppCurrentWindowInternalFocusFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.focus",
                             APP_CURRENTWINDOWINTERNAL_FOCUS)

 protected:
  ~AppCurrentWindowInternalFocusFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalFullscreenFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.fullscreen",
                             APP_CURRENTWINDOWINTERNAL_FULLSCREEN)

 protected:
  ~AppCurrentWindowInternalFullscreenFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalMaximizeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.maximize",
                             APP_CURRENTWINDOWINTERNAL_MAXIMIZE)

 protected:
  ~AppCurrentWindowInternalMaximizeFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalMinimizeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.minimize",
                             APP_CURRENTWINDOWINTERNAL_MINIMIZE)

 protected:
  ~AppCurrentWindowInternalMinimizeFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalRestoreFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.restore",
                             APP_CURRENTWINDOWINTERNAL_RESTORE)

 protected:
  ~AppCurrentWindowInternalRestoreFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalDrawAttentionFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.drawAttention",
                             APP_CURRENTWINDOWINTERNAL_DRAWATTENTION)

 protected:
  ~AppCurrentWindowInternalDrawAttentionFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalClearAttentionFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.clearAttention",
                             APP_CURRENTWINDOWINTERNAL_CLEARATTENTION)

 protected:
  ~AppCurrentWindowInternalClearAttentionFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalShowFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.show",
                             APP_CURRENTWINDOWINTERNAL_SHOW)

 protected:
  ~AppCurrentWindowInternalShowFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalHideFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.hide",
                             APP_CURRENTWINDOWINTERNAL_HIDE)

 protected:
  ~AppCurrentWindowInternalHideFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalSetBoundsFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setBounds",
                             APP_CURRENTWINDOWINTERNAL_SETBOUNDS)
 protected:
  ~AppCurrentWindowInternalSetBoundsFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalSetSizeConstraintsFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setSizeConstraints",
                             APP_CURRENTWINDOWINTERNAL_SETSIZECONSTRAINTS)
 protected:
  ~AppCurrentWindowInternalSetSizeConstraintsFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalSetIconFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setIcon",
                             APP_CURRENTWINDOWINTERNAL_SETICON)

 protected:
  ~AppCurrentWindowInternalSetIconFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalSetShapeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setShape",
                             APP_CURRENTWINDOWINTERNAL_SETSHAPE)

 protected:
  ~AppCurrentWindowInternalSetShapeFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalSetAlwaysOnTopFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setAlwaysOnTop",
                             APP_CURRENTWINDOWINTERNAL_SETALWAYSONTOP)

 protected:
  ~AppCurrentWindowInternalSetAlwaysOnTopFunction() override = default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalSetVisibleOnAllWorkspacesFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "app.currentWindowInternal.setVisibleOnAllWorkspaces",
      APP_CURRENTWINDOWINTERNAL_SETVISIBLEONALLWORKSPACES)

 protected:
  ~AppCurrentWindowInternalSetVisibleOnAllWorkspacesFunction() override =
      default;
  ResponseAction Run() override;
};

class AppCurrentWindowInternalSetActivateOnPointerFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setActivateOnPointer",
                             APP_CURRENTWINDOWINTERNAL_SETACTIVATEONPOINTER)

 protected:
  ~AppCurrentWindowInternalSetActivateOnPointerFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_

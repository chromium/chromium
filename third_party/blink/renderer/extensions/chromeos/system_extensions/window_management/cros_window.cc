// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window.h"

#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_management.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

CrosWindow::CrosWindow(CrosWindowManagement* manager) : manager_(manager) {}

void CrosWindow::Trace(Visitor* visitor) const {
  visitor->Trace(manager_);
  ScriptWrappable::Trace(visitor);
}

String CrosWindow::appId() {
  return String();
}

String CrosWindow::title() {
  return String();
}

bool CrosWindow::isFullscreen() {
  return false;
}

bool CrosWindow::isMinimised() {
  return false;
}

bool CrosWindow::isVisible() {
  return false;
}

size_t CrosWindow::hash() {
  return 0;
}

DOMPoint* CrosWindow::origin() {
  return DOMPoint::Create(0, 0);
}

DOMRect* CrosWindow::bounds() {
  return DOMRect::Create(0, 0, 0, 0);
}

bool CrosWindow::setOrigin(double x, double y) {
  return false;
}

bool CrosWindow::setBounds(double x, double y, double width, double height) {
  return false;
}

bool CrosWindow::setFullscreen(bool value) {
  return false;
}

bool CrosWindow::maximize() {
  return false;
}

bool CrosWindow::minimize() {
  return false;
}

bool CrosWindow::raise() {
  return false;
}

bool CrosWindow::focus() {
  return false;
}

bool CrosWindow::close() {
  return false;
}

}  // namespace blink

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_EDITCOMMAND_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_EDITCOMMAND_HELPER_H_

#import <Cocoa/Cocoa.h>

#include "base/gtest_prod_util.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/common/content_export.h"

namespace content {

// This class mimics the behavior of WebKit's WebView class in a way that makes
// sense for Chrome.
//
// WebCore has the concept of "core commands", basically named actions such as
// "Select All" and "Move Cursor Left".  The commands are executed using their
// string value by WebCore.
//
// This class is responsible for 2 things:
// 1. Provide an abstraction to determine the enabled/disabled state of menu
// items that correspond to edit commands.
// 2. Hook up a bunch of objc selectors to the RenderWidgetHostViewCocoa object.
// (note that this is not a misspelling of RenderWidgetHostViewMac, it's in
//  fact a distinct object) When these selectors are called, the relevant
// edit command is executed in WebCore.
class CONTENT_EXPORT RenderWidgetHostViewMacEditCommandHelper {
  FRIEND_TEST_ALL_PREFIXES(
      RenderWidgetHostViewMacEditCommandHelperWithTaskEnvTest,
      TestEditingCommandDelivery);

 public:
  RenderWidgetHostViewMacEditCommandHelper();

  RenderWidgetHostViewMacEditCommandHelper(
      const RenderWidgetHostViewMacEditCommandHelper&) = delete;
  RenderWidgetHostViewMacEditCommandHelper& operator=(
      const RenderWidgetHostViewMacEditCommandHelper&) = delete;

  ~RenderWidgetHostViewMacEditCommandHelper();

  // Is a given menu item currently enabled?
  // SEL - the objc selector currently associated with an NSMenuItem.
  // owner - An object we can retrieve a RenderWidgetHostViewMac from to
  // determine the command states.
  bool IsMenuItemEnabled(SEL item_action,
                         id<RenderWidgetHostNSViewHostOwner> owner);

  // Converts an editing selector into a command name that can be sent to
  // webkit.
  static NSString* CommandNameForSelector(SEL selector);

  // Adds editing selectors to the objc class using the objc runtime APIs.
  // Each selector is connected to a single c method which forwards the message
  // to WebCore's ExecuteEditCommand() function.
  // This method is idempotent.
  // The class passed in must conform to the RenderWidgetHostNSViewHostOwner
  static void AddEditingSelectorsToClass(Class klass);

  // Gets a list of all the selectors that AddEditingSelectorsToClass adds to
  // the aforementioned class.
  // returns an array of NSStrings WITHOUT the trailing ':'s.
  static NSArray* GetEditSelectorNamesForTesting();

 protected:
 private:
  std::unordered_set<std::string> edit_command_set_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_EDITCOMMAND_HELPER_H_

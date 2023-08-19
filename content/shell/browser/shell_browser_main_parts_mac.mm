// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_browser_main_parts.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/bundle_locations.h"

namespace {

NSMenuItem* CreateMenuItem(NSString* title,
                           SEL action,
                           NSString* key_equivalent) {
  return [[NSMenuItem alloc] initWithTitle:title
                                    action:action
                             keyEquivalent:key_equivalent];
}

// The App Menu refers to the menu titled "Content Shell".
NSMenu* BuildAppMenu() {
  // The title is not used, as the title will always be the name of the app.
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@""];

  NSMenuItem* item =
      CreateMenuItem(@"Hide Content Shell", @selector(hide:), @"h");
  [menu addItem:item];

  item =
      CreateMenuItem(@"Hide Others", @selector(hideOtherApplications:), @"h");
  item.keyEquivalentModifierMask =
      NSEventModifierFlagOption | NSEventModifierFlagCommand;
  [menu addItem:item];

  item = CreateMenuItem(@"Show All", @selector(unhideAllApplications:), @"");
  [menu addItem:item];

  item = CreateMenuItem(@"Quit Content Shell", @selector(terminate:), @"q");
  [menu addItem:item];

  return menu;
}

NSMenu* BuildFileMenu() {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"File"];
  NSMenuItem* item = CreateMenuItem(@"New", @selector(newDocument:), @"n");
  [menu addItem:item];

  item = CreateMenuItem(@"Close", @selector(performClose:), @"w");
  [menu addItem:item];
  return menu;
}

NSMenu* BuildEditMenu() {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Edit"];

  NSMenuItem* item = CreateMenuItem(@"Undo", @selector(undo:), @"z");
  [menu addItem:item];

  item = CreateMenuItem(@"Redo", @selector(redo:), @"Z");
  [menu addItem:item];

  item = CreateMenuItem(@"Cut", @selector(cut:), @"x");
  [menu addItem:item];

  item = CreateMenuItem(@"Copy", @selector(copy:), @"c");
  [menu addItem:item];

  item = CreateMenuItem(@"Paste", @selector(paste:), @"v");
  [menu addItem:item];

  item = CreateMenuItem(@"Select All", @selector(selectAll:), @"a");
  [menu addItem:item];
  return menu;
}

NSMenu* BuildViewMenu() {
  // AppKit auto-populates this menu.
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"View"];
  return menu;
}

NSMenu* BuildDebugMenu() {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Debug"];

  NSMenuItem* item =
      CreateMenuItem(@"Show Developer Tools", @selector(showDevTools:), @"");
  [menu addItem:item];
  return menu;
}

NSMenu* BuildWindowMenu() {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Window"];

  NSMenuItem* item =
      CreateMenuItem(@"Minimize", @selector(performMiniaturize:), @"m");
  [menu addItem:item];
  item = CreateMenuItem(@"Zoom", @selector(performZoom:), @"");
  [menu addItem:item];
  item = CreateMenuItem(@"Bring All To Front", @selector(arrangeInFront:), @"");
  [menu addItem:item];
  return menu;
}

NSMenu* BuildMainMenu() {
  NSMenu* main_menu = [[NSMenu alloc] initWithTitle:@""];

  using Builder = NSMenu* (*)();
  static const Builder kBuilderFuncs[] = {&BuildAppMenu,   &BuildFileMenu,
                                          &BuildEditMenu,  &BuildViewMenu,
                                          &BuildDebugMenu, &BuildWindowMenu};
  for (auto* builder : kBuilderFuncs) {
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:@""
                                                  action:nullptr
                                           keyEquivalent:@""];
    item.submenu = builder();
    [main_menu addItem:item];
  }
  return main_menu;
}

}  // namespace

namespace content {

void ShellBrowserMainParts::PreCreateMainMessageLoop() {
  NSMenu* main_menu = BuildMainMenu();
  [NSApplication.sharedApplication setMainMenu:main_menu];
}

}  // namespace content

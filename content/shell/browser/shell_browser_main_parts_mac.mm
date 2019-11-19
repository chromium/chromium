// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_browser_main_parts.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/bundle_locations.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "content/browser/sandbox_parameters_mac.h"  // nogncheck
#include "net/test/test_data_directory.h"

namespace {

base::scoped_nsobject<NSMenuItem> CreateMenuItem(NSString* title,
                                                 SEL action,
                                                 NSString* key_equivalent) {
  return base::scoped_nsobject<NSMenuItem>([[NSMenuItem alloc]
      initWithTitle:title
             action:action
      keyEquivalent:key_equivalent]);
}

// The App Menu refers to the dropdown titled "Content Shell".
base::scoped_nsobject<NSMenu> BuildAppMenu() {
  // The title is not used, as the title will always be the name of the App.
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);

  base::scoped_nsobject<NSMenuItem> item =
      CreateMenuItem(@"Hide Content Shell", @selector(hide:), @"h");
  [menu addItem:item];

  item =
      CreateMenuItem(@"Hide Others", @selector(hideOtherApplications:), @"h");
  item.get().keyEquivalentModifierMask =
      NSEventModifierFlagOption | NSEventModifierFlagCommand;
  [menu addItem:item];

  item = CreateMenuItem(@"Show All", @selector(unhideAllApplications:), @"");
  [menu addItem:item];

  item = CreateMenuItem(@"Quit Content Shell", @selector(terminate:), @"q");
  [menu addItem:item];

  return menu;
}

base::scoped_nsobject<NSMenu> BuildFileMenu() {
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"File"]);
  base::scoped_nsobject<NSMenuItem> item =
      CreateMenuItem(@"New", @selector(newDocument:), @"n");
  [menu addItem:item];

  item = CreateMenuItem(@"Close", @selector(performClose:), @"w");
  [menu addItem:item];
  return menu;
}

base::scoped_nsobject<NSMenu> BuildEditMenu() {
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"Edit"]);

  base::scoped_nsobject<NSMenuItem> item =
      CreateMenuItem(@"Undo", @selector(undo:), @"z");
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

base::scoped_nsobject<NSMenu> BuildViewMenu() {
  // AppKit auto-populates this menu.
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"View"]);
  return menu;
}

base::scoped_nsobject<NSMenu> BuildDebugMenu() {
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"Debug"]);

  base::scoped_nsobject<NSMenuItem> item =
      CreateMenuItem(@"Show Developer Tools", @selector(showDevTools:), @"");
  [menu addItem:item];
  return menu;
}

base::scoped_nsobject<NSMenu> BuildWindowMenu() {
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"Window"]);

  base::scoped_nsobject<NSMenuItem> item =
      CreateMenuItem(@"Minimize", @selector(performMiniaturize:), @"m");
  [menu addItem:item];
  item = CreateMenuItem(@"Zoom", @selector(performZoom:), @"");
  [menu addItem:item];
  item = CreateMenuItem(@"Bring All To Front", @selector(arrangeInFront:), @"");
  [menu addItem:item];
  return menu;
}

base::scoped_nsobject<NSMenu> BuildMainMenu() {
  base::scoped_nsobject<NSMenu> main_menu([[NSMenu alloc] initWithTitle:@""]);

  using Builder = base::scoped_nsobject<NSMenu> (*)();
  static const Builder kBuilderFuncs[] = {&BuildAppMenu,   &BuildFileMenu,
                                          &BuildEditMenu,  &BuildViewMenu,
                                          &BuildDebugMenu, &BuildWindowMenu};
  for (auto* builder : kBuilderFuncs) {
    NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:@""
                                                   action:NULL
                                            keyEquivalent:@""] autorelease];
    item.submenu = builder();
    [main_menu addItem:item];
  }
  return main_menu;
}

}  // namespace

namespace content {

void ShellBrowserMainParts::PreMainMessageLoopStart() {
  base::scoped_nsobject<NSMenu> main_menu = BuildMainMenu();
  [[NSApplication sharedApplication] setMainMenu:main_menu];

  // Expand the network service sandbox to allow reading the test TLS
  // certificates.
  SetNetworkTestCertsDirectoryForTesting(net::GetTestCertsDirectory());
}

}  // namespace content

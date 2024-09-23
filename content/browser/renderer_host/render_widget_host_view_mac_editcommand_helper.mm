// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/render_widget_host_view_mac_editcommand_helper.h"

#import <objc/runtime.h>
#include <stddef.h>

#include "content/browser/renderer_host/render_widget_host_impl.h"
#import "content/browser/renderer_host/render_widget_host_view_mac.h"

namespace content {
namespace {

// The names of all the objc selectors w/o ':'s added to an object by
// AddEditingSelectorsToClass().
//
// This needs to be kept in Sync with WEB_COMMAND list in the WebKit tree at:
// WebKit/mac/WebView/WebHTMLView.mm .
const auto kEditCommands =
    std::to_array<const char*>({"alignCenter",
                                "alignJustified",
                                "alignLeft",
                                "alignRight",
                                "copy",
                                "cut",
                                "delete",
                                "deleteBackward",
                                "deleteBackwardByDecomposingPreviousCharacter",
                                "deleteForward",
                                "deleteToBeginningOfLine",
                                "deleteToBeginningOfParagraph",
                                "deleteToEndOfLine",
                                "deleteToEndOfParagraph",
                                "deleteToMark",
                                "deleteWordBackward",
                                "deleteWordForward",
                                "ignoreSpelling",
                                "indent",
                                "insertBacktab",
                                "insertLineBreak",
                                "insertNewline",
                                "insertNewlineIgnoringFieldEditor",
                                "insertParagraphSeparator",
                                "insertTab",
                                "insertTabIgnoringFieldEditor",
                                "makeTextWritingDirectionLeftToRight",
                                "makeTextWritingDirectionNatural",
                                "makeTextWritingDirectionRightToLeft",
                                "moveBackward",
                                "moveBackwardAndModifySelection",
                                "moveDown",
                                "moveDownAndModifySelection",
                                "moveForward",
                                "moveForwardAndModifySelection",
                                "moveLeft",
                                "moveLeftAndModifySelection",
                                "moveParagraphBackwardAndModifySelection",
                                "moveParagraphForwardAndModifySelection",
                                "moveRight",
                                "moveRightAndModifySelection",
                                "moveToBeginningOfDocument",
                                "moveToBeginningOfDocumentAndModifySelection",
                                "moveToBeginningOfLine",
                                "moveToBeginningOfLineAndModifySelection",
                                "moveToBeginningOfParagraph",
                                "moveToBeginningOfParagraphAndModifySelection",
                                "moveToBeginningOfSentence",
                                "moveToBeginningOfSentenceAndModifySelection",
                                "moveToEndOfDocument",
                                "moveToEndOfDocumentAndModifySelection",
                                "moveToEndOfLine",
                                "moveToEndOfLineAndModifySelection",
                                "moveToEndOfParagraph",
                                "moveToEndOfParagraphAndModifySelection",
                                "moveToEndOfSentence",
                                "moveToEndOfSentenceAndModifySelection",
                                "moveUp",
                                "moveUpAndModifySelection",
                                "moveWordBackward",
                                "moveWordBackwardAndModifySelection",
                                "moveWordForward",
                                "moveWordForwardAndModifySelection",
                                "moveWordLeft",
                                "moveWordLeftAndModifySelection",
                                "moveWordRight",
                                "moveWordRightAndModifySelection",
                                "outdent",
                                "pageDown",
                                "pageDownAndModifySelection",
                                "pageUp",
                                "pageUpAndModifySelection",
                                "selectAll",
                                "selectLine",
                                "selectParagraph",
                                "selectSentence",
                                "selectToMark",
                                "selectWord",
                                "setMark",
                                "showGuessPanel",
                                "subscript",
                                "superscript",
                                "swapWithMark",
                                "transpose",
                                "underline",
                                "unscript",
                                "yank",
                                "yankAndSelect"});

// This function is installed via the objc runtime as the implementation of all
// the various editing selectors.
// The objc runtime hookup occurs in
// RenderWidgetHostViewMacEditCommandHelper::AddEditingSelectorsToClass().
//
// self - the object we're attached to; it must implement the
// RenderWidgetHostNSViewHostOwner protocol.
// _cmd - the selector that fired.
// sender - the id of the object that sent the message.
//
// The selector is translated into an edit comand and then forwarded down the
// pipeline to WebCore.
// The route the message takes is:
// RenderWidgetHostViewMac -> RenderViewHost ->
// | IPC | ->
// `blink::WebView` -> currently focused WebFrame.
// The WebFrame is in the Chrome glue layer and forwards the message to WebCore.
void EditCommandImp(id self, SEL _cmd, id sender) {
  // Make sure |self| is the right type.
  DCHECK([self respondsToSelector:@selector(renderWidgetHostNSViewHost)]);

  // SEL -> command name string.
  NSString* command_name_ns =
      RenderWidgetHostViewMacEditCommandHelper::CommandNameForSelector(_cmd);
  std::string command([command_name_ns UTF8String]);

  // Forward the edit command string down the pipeline.
  remote_cocoa::mojom::RenderWidgetHostNSViewHost* host =
      [self renderWidgetHostNSViewHost];
  DCHECK(host);
  host->ExecuteEditCommand(command);
}

}  // namespace

// Maps an objc-selector to a core command name.
//
// Returns the core command name (which is the selector name with the trailing
// ':' stripped in most cases).
//
// Adapted from a function by the same name in
// WebKit/mac/WebView/WebHTMLView.mm .
// Capitalized names are returned from this function, but that's simply
// matching WebHTMLView.mm.
NSString* RenderWidgetHostViewMacEditCommandHelper::CommandNameForSelector(
    SEL selector) {
  if (selector == @selector(insertParagraphSeparator:) ||
      selector == @selector(insertNewlineIgnoringFieldEditor:))
    return @"InsertNewline";
  if (selector == @selector(insertTabIgnoringFieldEditor:))
    return @"InsertTab";
  if (selector == @selector(pageDown:))
    return @"MovePageDown";
  if (selector == @selector(pageDownAndModifySelection:))
    return @"MovePageDownAndModifySelection";
  if (selector == @selector(pageUp:))
    return @"MovePageUp";
  if (selector == @selector(pageUpAndModifySelection:))
    return @"MovePageUpAndModifySelection";
  if (selector == @selector(showGuessPanel:))
    return @"ToggleSpellPanel";

  // Remove the trailing colon.
  NSString* selector_str = NSStringFromSelector(selector);
  int selector_len = [selector_str length];
  return [selector_str substringToIndex:selector_len - 1];
}

RenderWidgetHostViewMacEditCommandHelper::
    RenderWidgetHostViewMacEditCommandHelper() {
  for (const char* command : kEditCommands) {
    edit_command_set_.insert(command);
  }
}

RenderWidgetHostViewMacEditCommandHelper::
    ~RenderWidgetHostViewMacEditCommandHelper() {}


bool RenderWidgetHostViewMacEditCommandHelper::IsMenuItemEnabled(
    SEL item_action,
    id<RenderWidgetHostNSViewHostOwner> owner) {
  const char* selector_name = sel_getName(item_action);
  // TODO(jeremy): The final form of this function will check state
  // associated with the Browser.

  // For now just mark all edit commands as enabled.
  NSString* selector_name_ns = [NSString stringWithUTF8String:selector_name];

  // Remove trailing ':'
  size_t str_len = [selector_name_ns length];
  selector_name_ns = [selector_name_ns substringToIndex:str_len - 1];
  std::string edit_command_name([selector_name_ns UTF8String]);

  // search for presence in set and return.
  bool ret = edit_command_set_.find(edit_command_name) !=
      edit_command_set_.end();
  return ret;
}

// static
void RenderWidgetHostViewMacEditCommandHelper::AddEditingSelectorsToClass(
    Class klass) {
  for (const char* command : kEditCommands) {
    // Append trailing ':' to command name to get selector name.
    NSString* sel_str = [NSString stringWithFormat:@"%s:", command];

    SEL edit_selector = NSSelectorFromString(sel_str);
    // May want to use @encode() for the last parameter to this method.
    // If class_addMethod fails we assume that all the editing selectors where
    // added to the class.
    // If a certain class already implements a method then class_addMethod
    // returns NO, which we can safely ignore.
    class_addMethod(klass, edit_selector, (IMP)EditCommandImp, "v@:@");
  }
}

// static
NSArray*
RenderWidgetHostViewMacEditCommandHelper::GetEditSelectorNamesForTesting() {
  size_t num_edit_commands = std::size(kEditCommands);
  NSMutableArray* ret = [NSMutableArray arrayWithCapacity:num_edit_commands];

  for (const char* command : kEditCommands) {
    [ret addObject:[NSString stringWithUTF8String:command]];
  }

  return ret;
}

}  // namespace content

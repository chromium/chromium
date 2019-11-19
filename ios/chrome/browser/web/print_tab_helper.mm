// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/print_tab_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#import "ios/chrome/browser/web/web_state_printer.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Prefix for print JavaScript command.
const char kPrintCommandPrefix[] = "print";
}

PrintTabHelper::PrintTabHelper(web::WebState* web_state) {
  web_state->AddObserver(this);
  subscription_ = web_state->AddScriptCommandCallback(
      base::Bind(&PrintTabHelper::OnPrintCommand, base::Unretained(this),
                 base::Unretained(web_state)),
      kPrintCommandPrefix);
}

PrintTabHelper::~PrintTabHelper() = default;

void PrintTabHelper::set_printer(id<WebStatePrinter> printer) {
  printer_ = printer;
}

void PrintTabHelper::WebStateDestroyed(web::WebState* web_state) {
  // Stops handling print requests from the web page.
  web_state->RemoveObserver(this);
}

void PrintTabHelper::OnPrintCommand(web::WebState* web_state,
                                    const base::DictionaryValue& command,
                                    const GURL& page_url,
                                    bool user_is_interacting,
                                    web::WebFrame* sender_frame) {
  if (!sender_frame->IsMainFrame() && !user_is_interacting) {
    // Ignore non user-initiated window.print() calls from iframes, to prevent
    // abusive behavior from web sites.
    return;
  }
  DCHECK(web_state);
  DCHECK(printer_);
  [printer_ printWebState:web_state];
}

WEB_STATE_USER_DATA_KEY_IMPL(PrintTabHelper)

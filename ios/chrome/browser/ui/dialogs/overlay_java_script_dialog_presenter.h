// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DIALOGS_OVERLAY_JAVA_SCRIPT_DIALOG_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_DIALOGS_OVERLAY_JAVA_SCRIPT_DIALOG_PRESENTER_H_

#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_source.h"
#include "ios/web/public/ui/java_script_dialog_presenter.h"

class OverlayResponse;

// Implementation of JavaScriptDialogPresenter that uses OverlayPresenter to run
// JavaScript dialogs.
class OverlayJavaScriptDialogPresenter final
    : public web::JavaScriptDialogPresenter {
 public:
  OverlayJavaScriptDialogPresenter();
  ~OverlayJavaScriptDialogPresenter() override;

  // Notifies the presenter that the presenter that its tab is being closed.
  void Close();

  // web::JavaScriptDialogPresenter:
  void RunJavaScriptDialog(web::WebState* web_state,
                           const GURL& origin_url,
                           web::JavaScriptDialogType dialog_type,
                           NSString* message_text,
                           NSString* default_prompt_text,
                           web::DialogClosedCallback callback) override;
  void CancelDialogs(web::WebState* web_state) override;

 private:
  // Executes |callback| using the user interaction information from |response|.
  void HandleJavaScriptDialogResponse(web::DialogClosedCallback callback,
                                      JavaScriptDialogSource source,
                                      web::JavaScriptDialogType dialog_type,
                                      OverlayResponse* response);

  DISALLOW_COPY_AND_ASSIGN(OverlayJavaScriptDialogPresenter);

  bool closing_ = false;
  base::WeakPtrFactory<OverlayJavaScriptDialogPresenter> weak_factory_;
};

#endif  // IOS_CHROME_BROWSER_UI_DIALOGS_OVERLAY_JAVA_SCRIPT_DIALOG_PRESENTER_H_

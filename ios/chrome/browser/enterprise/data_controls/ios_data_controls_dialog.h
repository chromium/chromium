// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_IOS_DATA_CONTROLS_DIALOG_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_IOS_DATA_CONTROLS_DIALOG_H_

#import "base/functional/callback.h"
#import "components/enterprise/data_controls/core/browser/data_controls_dialog.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace web {
class WebState;
}  // namespace web

namespace data_controls {

class IOSDataControlsDialogFactory;

// IOS implementation of `DataControlsDialog`. The warning dialog or blocking
// toast shown to the user when a Data Controls policy is triggered.
class IOSDataControlsDialog : public DataControlsDialog,
                              public web::WebStateObserver {
 public:
  IOSDataControlsDialog(Type type,
                        web::WebState* web_state,
                        base::OnceCallback<void(bool)> callback);
  ~IOSDataControlsDialog() override;

  // DataControlsDialog:
  void Show(base::OnceClosure on_destructed) override;

 private:
  friend IOSDataControlsDialogFactory;

  std::u16string GetDialogTitle() const;
  std::u16string GetDialogLabel() const;

  // The WebState this dialog is associated with.
  raw_ptr<web::WebState> web_state_;
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_IOS_DATA_CONTROLS_DIALOG_H_
